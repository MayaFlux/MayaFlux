#include "Server.hpp"

#include "Commentator.hpp"

#include <cerrno>
#include <cstddef>
#include <fcntl.h>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <mstcpip.h>
using ssize_t = int;
#else
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Lila {

// --- Small platform abstraction helpers ------------------------------------

static int socket_errno()
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

static std::string socket_error_string(int code)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    // Keep it simple: return numeric WSA code (can be extended with FormatMessage)
    return "WSA error " + std::to_string(code);
#else
    return std::string(strerror(code));
#endif
}

static void socket_close(int fd)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    closesocket(static_cast<SOCKET>(fd));
#else
    ::close(fd);
#endif
}

static ssize_t socket_read(int fd, void* buf, size_t len)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    // recv returns int
    return recv(static_cast<SOCKET>(fd), static_cast<char*>(buf), static_cast<int>(len), 0);
#else
    return ::read(fd, buf, len);
#endif
}

static int set_nonblocking(int fd)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    u_long mode = 1;
    return ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

// --------------------------------------------------------------------------

Server::Server(int port)
    : m_port(port)
{
    LILA_DEBUG(Emitter::SYSTEM, "Server instance created on port " + std::to_string(port));
}

Server::~Server()
{
    stop();
}

bool Server::start() noexcept
{
    if (m_running) {
        LILA_WARN(Emitter::SERVER, "Server already running");
        return false;
    }

    try {
#ifdef MAYAFLUX_PLATFORM_WINDOWS
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::system_error(WSAGetLastError(), std::system_category(), "WSAStartup failed");
        }
#endif

        m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_server_fd < 0) {
            throw std::system_error(socket_errno(), std::system_category(), "socket creation failed");
        }

        int opt = 1;
        if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0) {
            throw std::system_error(socket_errno(), std::system_category(), "setsockopt failed");
        }

        if (set_nonblocking(m_server_fd) < 0) {
            throw std::system_error(socket_errno(), std::system_category(), "set non-blocking failed");
        }

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        if (bind(m_server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            throw std::system_error(socket_errno(), std::system_category(), "bind failed");
        }

        if (listen(m_server_fd, 10) < 0) {
            throw std::system_error(socket_errno(), std::system_category(), "listen failed");
        }

        m_running = true;
        m_event_bus.publish(StreamEvent { EventType::ServerStart });
        if (m_start_handler) {
            m_start_handler();
        }

        m_server_thread = ServerThread([this](const auto& token) { server_loop(token); });

        LILA_INFO(Emitter::SERVER, "Server started on port " + std::to_string(m_port));

        m_event_bus.publish(StreamEvent { EventType::ServerStart });

        return true;

    } catch (const std::system_error& e) {
        LILA_ERROR(Emitter::SERVER, "Failed to start server: " + std::string(e.what()) + " (code: " + std::to_string(e.code().value()) + ")");
        if (m_server_fd >= 0) {
            socket_close(m_server_fd);
            m_server_fd = -1;
        }
#ifdef MAYAFLUX_PLATFORM_WINDOWS
        WSACleanup();
#endif
        return false;
    }
}

void Server::stop() noexcept
{
    if (!m_running)
        return;

    m_running = false;

    if (m_server_thread.joinable()) {
        m_server_thread.request_stop();
        m_server_thread.join();
    }

    if (m_server_fd >= 0) {
        socket_close(m_server_fd);
        m_server_fd = -1;
    }

    {
        std::unique_lock lock(m_clients_mutex);
        for (const auto& [fd, client] : m_connected_clients) {
            socket_close(fd);
        }
        m_connected_clients.clear();
    }

    m_event_bus.publish(StreamEvent { EventType::ServerStop });

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    WSACleanup();
#endif

    LILA_INFO(Emitter::SERVER, "Server stopped");
}

#ifndef MAYAFLUX_JTHREAD_BROKEN
void Server::server_loop(const std::stop_token& stop_token)
#else
void Server::server_loop(const ServerThread::StopToken& stop_token)
#endif
{
    while (!stop_token.stop_requested() && m_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_server_fd, &readfds);

        timeval timeout { .tv_sec = 1, .tv_usec = 0 };

#ifdef MAYAFLUX_PLATFORM_WINDOWS
        int activity = select(0, &readfds, nullptr, nullptr, &timeout); // nfds ignored on Windows
#else
        int activity = select(m_server_fd + 1, &readfds, nullptr, nullptr, &timeout);
#endif

        if (activity < 0) {
            int code = socket_errno();
#ifndef MAYAFLUX_PLATFORM_WINDOWS
            if (code == EINTR)
                continue;
#endif
            if (m_running) {
                LILA_ERROR(Emitter::SERVER, "Select error: " + socket_error_string(code));
            }
            break;
        }

        if (activity == 0)
            continue;

        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);

#ifdef MAYAFLUX_PLATFORM_WINDOWS
        SOCKET client_socket = accept(static_cast<SOCKET>(m_server_fd), reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        int client_fd = static_cast<int>(client_socket);
        if (client_socket == INVALID_SOCKET) {
            int code = socket_errno();
            if (code == WSAEWOULDBLOCK)
                continue;
            if (m_running) {
                LILA_ERROR(Emitter::SERVER, "Accept failed: " + socket_error_string(code));
            }
            continue;
        }
#else
        int client_fd = accept(m_server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            if (m_running) {
                LILA_ERROR(Emitter::SERVER, "Accept failed: " + std::string(strerror(errno)));
            }
            continue;
        }
#endif

        if (set_nonblocking(client_fd) < 0) {
            LILA_WARN(Emitter::SERVER, "Failed to set non-blocking on client fd " + std::to_string(client_fd));
        }

        handle_client(client_fd);
    }
}

void Server::handle_client(int client_fd)
{
    int enable = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&enable), sizeof(enable)) < 0) {
        LILA_WARN(Emitter::SERVER, "Failed to set TCP_NODELAY on client fd " + std::to_string(client_fd) + ": " + socket_error_string(socket_errno()));
    }

    ClientInfo client_info {
        .fd = client_fd,
        .session_id = "",
        .connected_at = std::chrono::system_clock::now()
    };

    {
        std::unique_lock lock(m_clients_mutex);
        m_connected_clients[client_fd] = client_info;
    }

    if (m_connect_handler) {
        m_connect_handler(client_info);
    }

    m_event_bus.publish(StreamEvent { EventType::ClientConnected, client_info });

    LILA_INFO(Emitter::SERVER, "Client connected fd: " + std::to_string(client_fd));

    while (m_running) {
        auto message = read_message(client_fd);

        if (!message) {
            if (message.error() != "timeout" && message.error() != "would_block") {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }

        if (message->empty())
            continue;

        if (message->starts_with('@')) {
            process_control_message(client_fd, message->substr(1));
            continue;
        }

        if (m_message_handler) {
            auto response = m_message_handler(*message);
            if (response) {
                send_message(client_fd, *response);
            } else {
                send_message(client_fd, "{\"status\":\"error\",\"message\":\"" + response.error() + "\"}");
            }
        }
    }

    cleanup_client(client_fd);
}

void Server::process_control_message(int client_fd, std::string_view message)
{
    if (message.starts_with("session ")) {
        std::string session_id(message.substr(8));
        set_client_session(client_fd, session_id);
        send_message(client_fd, R"({"status":"success","message":"Session ID set"})");
    } else if (message.starts_with("ping")) {
        send_message(client_fd, R"({"status":"success","message":"pong"})");
    } else {
        send_message(client_fd, "{\"status\":\"error\",\"message\":\"Unknown command: " + std::string(message) + "\"}");
    }
}

void Server::set_client_session(int client_fd, std::string session_id)
{
    std::unique_lock lock(m_clients_mutex);
    if (auto it = m_connected_clients.find(client_fd); it != m_connected_clients.end()) {
        it->second.session_id = std::move(session_id);
    }
}

std::expected<std::string, std::string> Server::read_message(int client_fd)
{
    constexpr size_t BUFFER_SIZE = 4096;
    std::string result;

    result.reserve(BUFFER_SIZE);
    std::array<char, BUFFER_SIZE> buffer {};

    while (true) {
        ssize_t bytes_read = socket_read(client_fd, buffer.data(), buffer.size() - 1);

        if (bytes_read < 0) {
#ifdef MAYAFLUX_PLATFORM_WINDOWS
            int code = socket_errno();
            if (code == WSAEWOULDBLOCK) {
                return std::unexpected("would_block");
            }
            return std::unexpected("read error: " + socket_error_string(code));
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return std::unexpected("would_block");
            }
            return std::unexpected("read error: " + std::string(strerror(errno)));
#endif
        }

        if (bytes_read == 0) {
            return std::unexpected("client_disconnected");
        }

        buffer[bytes_read] = '\0';
        result.append(buffer.data());

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
            if (!result.empty() && result.back() == '\r') {
                result.pop_back();
            }
            break;
        }

        if (result.size() > static_cast<size_t>(1024 * 1024)) {
            return std::unexpected("message_too_large");
        }
    }

    return result;
}

bool Server::send_message(int client_fd, std::string_view message)
{
    std::string msg_with_newline = std::string(message) + "\n";
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    int sent = send(static_cast<SOCKET>(client_fd), msg_with_newline.data(), static_cast<int>(msg_with_newline.size()), 0);
    return sent == static_cast<int>(msg_with_newline.size());
#else
    ssize_t sent = send(client_fd, msg_with_newline.data(), msg_with_newline.size(), 0);
    return sent == static_cast<ssize_t>(msg_with_newline.size());
#endif
}

void Server::cleanup_client(int client_fd)
{
    std::optional<ClientInfo> client_info;

    {
        std::unique_lock lock(m_clients_mutex);
        if (auto it = m_connected_clients.find(client_fd); it != m_connected_clients.end()) {
            client_info = it->second;
            m_connected_clients.erase(it);
        }
    }

    if (client_info) {
        if (m_disconnect_handler) {
            m_disconnect_handler(*client_info);
        }

        m_event_bus.publish(StreamEvent { EventType::ClientDisconnected, *client_info });

        LILA_INFO(Emitter::SERVER, "Client disconnected (fd: " + std::to_string(client_fd) + ")");
    }

    socket_close(client_fd);
}

void Server::broadcast_event(const StreamEvent& /*event*/, std::optional<std::string_view> target_session)
{
    std::shared_lock lock(m_clients_mutex);

    for (const auto& [fd, client] : m_connected_clients) {
        if (target_session && client.session_id != *target_session) {
            continue;
        }

        send_message(fd, "TODO: Serialize event to JSON");
    }
}

std::vector<ClientInfo> Server::get_connected_clients() const
{
    std::shared_lock lock(m_clients_mutex);
    return m_connected_clients | std::views::values | std::ranges::to<std::vector>();
}

} // namespace Lila
