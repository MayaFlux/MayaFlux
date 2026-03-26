#pragma once

#include <asio.hpp>
#include <expected>

#include "EventBus.hpp"

namespace Lila {

class ClientSession;

/**
 * @class Server
 * @brief TCP server for interactive live coding sessions in MayaFlux
 *
 * Async TCP server built on asio. Accepts newline-delimited messages,
 * dispatches them to a message handler, and returns the response.
 * Control messages prefixed with '@' are handled internally.
 */
class LILA_API Server {
public:
    /// @brief Handler for processing incoming client messages
    using MessageHandler = std::function<std::expected<std::string, std::string>(std::string_view)>;

    /// @brief Handler for client connection/disconnection events
    using ConnectionHandler = std::function<void(const ClientInfo&)>;

    /// @brief Handler for server start events
    using StartHandler = std::function<void()>;

    explicit Server(int port = 9090);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    [[nodiscard]] bool start() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool is_running() const { return m_running.load(std::memory_order_acquire); }

    void set_message_handler(MessageHandler handler) { m_message_handler = std::move(handler); }
    void on_client_connected(ConnectionHandler handler) { m_connect_handler = std::move(handler); }
    void on_client_disconnected(ConnectionHandler handler) { m_disconnect_handler = std::move(handler); }
    void on_server_started(StartHandler handler) { m_start_handler = std::move(handler); }

    EventBus& event_bus() { return m_event_bus; }
    const EventBus& event_bus() const noexcept { return m_event_bus; }

    void broadcast_event(const StreamEvent& event,
        std::optional<std::string_view> target_session = std::nullopt);
    void broadcast_to_all(std::string_view message);

    void set_client_session(int client_id, std::string session_id);
    [[nodiscard]] std::optional<std::string> get_client_session(int client_id) const;
    [[nodiscard]] std::vector<ClientInfo> get_connected_clients() const;

private:
    void start_accept();
    void register_session(const std::shared_ptr<ClientSession>& session);
    void remove_session(int client_id);
    void process_control_message(int client_id, std::string_view message);

    friend class ClientSession;

    int m_port;
    std::atomic<bool> m_running { false };
    int m_next_client_id { 1 };

    asio::io_context m_io_context;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::thread m_io_thread;

    MessageHandler m_message_handler;
    ConnectionHandler m_connect_handler;
    ConnectionHandler m_disconnect_handler;
    StartHandler m_start_handler;
    EventBus m_event_bus;

    mutable std::shared_mutex m_clients_mutex;
    std::unordered_map<int, std::shared_ptr<ClientSession>> m_sessions;
};

} // namespace Lila
