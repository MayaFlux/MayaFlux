#pragma once

#include "Lila.hpp"

namespace Lila {

class Server {
public:
    Server(std::shared_ptr<Lila> lila, int port = 9090);
    ~Server();

    void run();

    void start();

    void stop();

    void on_eval(std::function<void(const std::string&)> callback);
    void on_error(std::function<void(const std::string&)> callback);

private:
    std::shared_ptr<Lila> m_lila;
    int m_port;
    int m_server_fd;
    std::atomic<bool> m_running { false };
    std::thread m_server_thread;

    std::function<void(const std::string&)> m_eval_callback;
    std::function<void(const std::string&)> m_error_callback;

    void server_loop();
    void handle_client(int client_fd);
    std::string read_message(int client_fd);
    void send_response(int client_fd, const std::string& response);
};

} // namespace Lila
