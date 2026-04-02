#include "Server.hpp"

#include "Commentator.hpp"

namespace Lila {

// ---------------------------------------------------------------------------
// ClientSession: one per connected client, drives the async read chain
// ---------------------------------------------------------------------------

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(asio::ip::tcp::socket socket, int client_id, Server& server)
        : m_socket(std::move(socket))
        , m_server(server)
    {
        m_info = ClientInfo {
            .fd = client_id,
            .session_id = "",
            .connected_at = std::chrono::system_clock::now()
        };

        asio::ip::tcp::no_delay nodelay(true);
        asio::error_code ec;
        if (m_socket.set_option(nodelay, ec)) {
            LILA_WARN(Emitter::SERVER,
                "Failed to set TCP_NODELAY on client " + std::to_string(client_id)
                    + ": " + ec.message());
        }
    }

    void start() { read_chunk(); }

    void send(std::string_view message)
    {
        auto msg = std::make_shared<std::string>(message);
        msg->push_back('\n');

        asio::async_write(m_socket, asio::buffer(*msg),
            [self = shared_from_this(), msg](const asio::error_code& ec, size_t) {
                if (ec) {
                    LILA_WARN(Emitter::SERVER,
                        "Write failed for client " + std::to_string(self->m_info.fd)
                            + ": " + ec.message());
                }
            });
    }

    void close()
    {
        asio::error_code ec;
        if (m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec)) {
            LILA_WARN(Emitter::SERVER,
                "Failed to shutdown socket for client " + std::to_string(m_info.fd)
                    + ": " + ec.message());
        }

        if (m_socket.close(ec)) {
            LILA_WARN(Emitter::SERVER,
                "Failed to close socket for client " + std::to_string(m_info.fd)
                    + ": " + ec.message());
        }
    }

    ClientInfo& info() { return m_info; }
    const ClientInfo& info() const { return m_info; }

private:
    void read_chunk()
    {
        m_socket.async_read_some(asio::buffer(m_read_buf),
            [self = shared_from_this()](const asio::error_code& ec, size_t bytes) {
                self->on_chunk(ec, bytes);
            });
    }

    void on_chunk(const asio::error_code& ec, size_t bytes)
    {
        if (ec) {
            m_server.remove_session(m_info.fd);
            return;
        }

        m_block_buf.append(m_read_buf.data(), bytes);

        if (m_block_buf.size() > static_cast<size_t>(1024 * 1024)) {
            m_block_buf.clear();
            read_chunk();
            return;
        }

        if (m_block_buf.back() == '\n') {
            while (!m_block_buf.empty() && (m_block_buf.back() == '\n' || m_block_buf.back() == '\r'))
                m_block_buf.pop_back();

            if (!m_block_buf.empty()) {
                if (m_block_buf.starts_with('@')) {
                    m_server.process_control_message(m_info.fd, std::string_view(m_block_buf).substr(1));
                } else if (m_server.m_message_handler) {
                    auto response = m_server.m_message_handler(m_block_buf);
                    if (response) {
                        send(*response);
                    } else {
                        send(R"({"status":"error","message":")" + response.error() + "\"}");
                    }
                }
            }
            m_block_buf.clear();
        }

        read_chunk();
    }

    asio::ip::tcp::socket m_socket;
    std::array<char, 4096> m_read_buf {};
    std::string m_block_buf;
    ClientInfo m_info;
    Server& m_server;

    friend class Server;
};

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

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
    if (m_running.load(std::memory_order_acquire)) {
        LILA_WARN(Emitter::SERVER, "Server already running");
        return false;
    }

    try {
        auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<asio::ip::port_type>(m_port));
        m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(m_io_context);
        m_acceptor->open(endpoint.protocol());
        m_acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true));
        m_acceptor->bind(endpoint);
        m_acceptor->listen(10);
    } catch (const asio::system_error& e) {
        LILA_ERROR(Emitter::SERVER,
            "Failed to start server: " + std::string(e.what()));
        m_acceptor.reset();
        return false;
    }

    m_running.store(true, std::memory_order_release);
    m_event_bus.publish(StreamEvent { EventType::ServerStart });

    if (m_start_handler) {
        m_start_handler();
    }

    start_accept();

    m_io_thread = std::thread([this]() {
        LILA_DEBUG(Emitter::SERVER, "IO thread started");
        m_io_context.run();
        LILA_DEBUG(Emitter::SERVER, "IO thread exiting");
    });

    LILA_INFO(Emitter::SERVER, "Server started on port " + std::to_string(m_port));
    return true;
}

void Server::stop() noexcept
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    asio::error_code ec;
    if (m_acceptor) {
        if (m_acceptor->close(ec)) {
            LILA_WARN(Emitter::SERVER,
                "Failed to close acceptor: " + ec.message());
        }
    }

    {
        std::unique_lock lock(m_clients_mutex);
        for (auto& [id, session] : m_sessions) {
            session->close();
        }
        m_sessions.clear();
    }

    m_io_context.stop();

    if (m_io_thread.joinable()) {
        m_io_thread.join();
    }

    m_acceptor.reset();
    m_event_bus.publish(StreamEvent { EventType::ServerStop });
    LILA_INFO(Emitter::SERVER, "Server stopped");
}

void Server::start_accept()
{
    m_acceptor->async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
                if (m_running.load(std::memory_order_acquire)) {
                    LILA_ERROR(Emitter::SERVER, "Accept failed: " + ec.message());
                }
                return;
            }

            int client_id = m_next_client_id++;
            auto session = std::make_shared<ClientSession>(std::move(socket), client_id, *this);
            register_session(session);
            session->start();

            start_accept();
        });
}

void Server::register_session(const std::shared_ptr<ClientSession>& session)
{
    int id = session->info().fd;

    {
        std::unique_lock lock(m_clients_mutex);
        m_sessions[id] = session;
    }

    if (m_connect_handler) {
        m_connect_handler(session->info());
    }

    m_event_bus.publish(StreamEvent { EventType::ClientConnected, session->info() });
    LILA_INFO(Emitter::SERVER, "Client connected id: " + std::to_string(id));
}

void Server::remove_session(int client_id)
{
    std::optional<ClientInfo> info;

    {
        std::unique_lock lock(m_clients_mutex);
        auto it = m_sessions.find(client_id);
        if (it == m_sessions.end()) {
            return;
        }
        info = it->second->info();
        it->second->close();
        m_sessions.erase(it);
    }

    if (info) {
        if (m_disconnect_handler) {
            m_disconnect_handler(*info);
        }
        m_event_bus.publish(StreamEvent { EventType::ClientDisconnected, *info });
        LILA_INFO(Emitter::SERVER, "Client disconnected (id: " + std::to_string(client_id) + ")");
    }
}

void Server::process_control_message(int client_id, std::string_view message)
{
    if (message.starts_with("session ")) {
        std::string session_id(message.substr(8));
        set_client_session(client_id, session_id);

        std::shared_lock lock(m_clients_mutex);
        if (auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            it->second->send(
                R"({"status":"success","message":"Session ID set to ')" + session_id + R"('"})");
        }

    } else if (message.starts_with("ping")) {
        std::shared_lock lock(m_clients_mutex);
        if (auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            it->second->send(R"({"status":"success","message":"pong"})");
        }

    } else {
        std::shared_lock lock(m_clients_mutex);
        if (auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            it->second->send(
                R"({"status":"error","message":"Unknown command: )" + std::string(message) + "\"}");
        }
    }
}

void Server::set_client_session(int client_id, std::string session_id)
{
    std::unique_lock lock(m_clients_mutex);
    if (auto it = m_sessions.find(client_id); it != m_sessions.end()) {
        it->second->info().session_id = std::move(session_id);
    }
}

std::optional<std::string> Server::get_client_session(int client_id) const
{
    std::shared_lock lock(m_clients_mutex);
    auto it = m_sessions.find(client_id);
    if (it == m_sessions.end()) {
        return std::nullopt;
    }
    const auto& sid = it->second->info().session_id;
    return sid.empty() ? std::nullopt : std::optional<std::string>(sid);
}

void Server::broadcast_event(const StreamEvent& /*event*/,
    std::optional<std::string_view> target_session)
{
    std::shared_lock lock(m_clients_mutex);

    for (const auto& [id, session] : m_sessions) {
        if (target_session && session->info().session_id != *target_session) {
            continue;
        }
        session->send(R"({"status":"info","message":"Event broadcast not implemented yet"})");
    }
}

void Server::broadcast_to_all(std::string_view message)
{
    std::shared_lock lock(m_clients_mutex);
    for (const auto& [id, session] : m_sessions) {
        session->send(message);
    }
}

std::vector<ClientInfo> Server::get_connected_clients() const
{
    std::shared_lock lock(m_clients_mutex);
    std::vector<ClientInfo> result;
    result.reserve(m_sessions.size());
    for (const auto& [id, session] : m_sessions) {
        result.push_back(session->info());
    }
    return result;
}

} // namespace Lila
