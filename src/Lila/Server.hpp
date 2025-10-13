#pragma once

#include <netinet/in.h>

namespace Lila {

class Server {
public:
    using MessageHandler = std::function<std::string(const std::string&)>;
    using ConnectionHandler = std::function<void(int client_fd)>;

    Server(int port = 9090);
    ~Server();

    void run();
    void start();
    void stop();

    [[nodiscard]] bool is_running() const { return m_running; }

    void set_message_handler(MessageHandler handler) { m_message_handler = std::move(handler); }
    void on_client_connected(ConnectionHandler handler) { m_connect_handler = std::move(handler); }
    void on_client_disconnected(ConnectionHandler handler) { m_disconnect_handler = std::move(handler); }

private:
    int m_port;
    int m_server_fd;
    std::atomic<bool> m_running { false };
    std::thread m_server_thread;

    MessageHandler m_message_handler;
    ConnectionHandler m_connect_handler;
    ConnectionHandler m_disconnect_handler;

    void server_loop();
    void handle_client(int client_fd);
    std::string read_message(int client_fd);
    void send_response(int client_fd, const std::string& response);
};

} // namespace Lila
