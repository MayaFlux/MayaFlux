#include "Server.hpp"

#include "Commentator.hpp"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Lila {

Server::Server(int port)
    : m_port(port)
    , m_server_fd(-1)
{
}

Server::~Server()
{
    stop();
}

void Server::run()
{
    m_running = true;
    server_loop();
}

void Server::start()
{
    if (m_running)
        return;

    m_running = true;
    m_server_thread = std::thread([this]() {
        server_loop();
    });
}

void Server::stop()
{
    m_running = false;

    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
    }

    if (m_server_thread.joinable()) {
        m_server_thread.join();
    }
}

void Server::server_loop()
{
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        perror("socket failed");
        return;
    }

    int opt = 1;
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt failed\n";
        close(m_server_fd);
        return;
    }

    int flags = fcntl(m_server_fd, F_GETFL, 0);
    fcntl(m_server_fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    if (bind(m_server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port " << m_port << "\n";
        close(m_server_fd);
        return;
    }

    if (listen(m_server_fd, 3) < 0) {
        perror("listen failed");
        close(m_server_fd);
        return;
    }

    LILA_INFO(Emitter::SERVER, "Lila server listening on port " + m_port);

    while (m_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_server_fd, &readfds);

        struct timeval timeout {};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(m_server_fd + 1, &readfds, nullptr, nullptr, &timeout);

        if (activity < 0 && errno != EINTR) {
            if (m_running) {
                std::cerr << "Select error\n";
            }
            break;
        }

        if (activity == 0) {
            continue;
        }

        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(m_server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (m_running) {
                std::cerr << "Accept failed: " << strerror(errno) << "\n";
            }
            continue;
        }

        if (m_connect_handler) {
            m_connect_handler(client_fd);
        }

        handle_client(client_fd);

        if (m_disconnect_handler) {
            m_disconnect_handler(client_fd);
        }

        close(client_fd);
    }
}

void Server::handle_client(int client_fd)
{
    while (m_running) {
        std::string message = read_message(client_fd);
        if (message.empty())
            break;

        std::string response;
        if (m_message_handler) {
            response = m_message_handler(message);
        } else {
            response = R"({"status":"error","message":"No message handler configured"})";
        }

        send_response(client_fd, response);
    }
}

std::string Server::read_message(int client_fd)
{
    constexpr size_t BUFFER_SIZE = 4096;
    std::string result;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);

    if (bytes_read <= 0)
        return {};

    buffer[bytes_read] = '\0';
    result = buffer;

    if (result == "\n" || result == "\r\n")
        return {};

    return result;
}

void Server::send_response(int client_fd, const std::string& response)
{
    std::string msg = response + "\n";
    send(client_fd, msg.c_str(), msg.length(), 0);
}

}
