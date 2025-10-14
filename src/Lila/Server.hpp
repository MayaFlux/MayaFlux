#pragma once

#include <expected>
#include <netinet/in.h>

#include "Event.hpp"

namespace Lila {

class Server {
public:
    using MessageHandler = std::function<std::expected<std::string, std::string>(std::string_view)>;
    using ConnectionHandler = std::function<void(const ClientInfo&)>;
    using StartHandler = std::function<void()>;

    Server(int port = 9090);
    ~Server();

    [[nodiscard]] bool start() noexcept;
    void stop() noexcept;

    [[nodiscard]] bool is_running() const { return m_running; }

    void set_message_handler(MessageHandler handler) { m_message_handler = std::move(handler); }
    void on_client_connected(ConnectionHandler handler) { m_connect_handler = std::move(handler); }
    void on_client_disconnected(ConnectionHandler handler) { m_disconnect_handler = std::move(handler); }
    void on_server_started(StartHandler handler) { m_start_handler = std::move(handler); }

    EventBus& event_bus() { return m_event_bus; }
    const EventBus& event_bus() const noexcept { return m_event_bus; }

    void broadcast_event(const Event& event, std::optional<std::string_view> target_session = std::nullopt);
    void broadcast_to_all(std::string_view message);

    void set_client_session(int client_fd, std::string session_id);
    [[nodiscard]] std::optional<std::string> get_client_session(int client_fd) const;
    [[nodiscard]] std::vector<ClientInfo> get_connected_clients() const;

private:
    int m_port;
    int m_server_fd { -1 };
    std::atomic<bool> m_running { false };
    std::jthread m_server_thread;

    MessageHandler m_message_handler;
    ConnectionHandler m_connect_handler;
    ConnectionHandler m_disconnect_handler;
    StartHandler m_start_handler;
    EventBus m_event_bus;

    mutable std::shared_mutex m_clients_mutex;
    std::unordered_map<int, ClientInfo> m_connected_clients;

    void server_loop(const std::stop_token& stop_token);
    void handle_client(int client_fd);

    [[nodiscard]] std::expected<std::string, std::string> read_message(int client_fd);
    [[nodiscard]] bool send_message(int client_fd, std::string_view message);

    void process_control_message(int client_fd, std::string_view message);
    void cleanup_client(int client_fd);
};

} // namespace Lila
