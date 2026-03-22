#include "TCPBackend.hpp"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

TCPBackend::TCPBackend(const TCPBackendInfo& config, asio::io_context& context)
    : m_config(config)
    , m_context(context)
{
}

TCPBackend::~TCPBackend()
{
    shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool TCPBackend::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    m_initialized.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "TCP backend initialized");

    return true;
}

void TCPBackend::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);

    MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
        "TCP backend started");
}

void TCPBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    {
        std::shared_lock lock(m_connections_mutex);
        for (auto& [id, conn] : m_connections) {
            asio::error_code ec;

            if (conn->socket.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "Error closing TCP connection {}: {}", id, ec.message());
            }

            if (conn->reconnect_timer) {
                conn->reconnect_timer->cancel();
            }
        }
    }

    {
        std::shared_lock lock(m_listeners_mutex);
        for (auto& [id, listener] : m_listeners) {
            asio::error_code ec;
            if (!listener->acceptor.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "Error closing TCP listener {}: {}", id, ec.message());
            }
        }
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::Shutdown,
        "TCP backend stopped");
}

void TCPBackend::shutdown()
{
    stop();

    {
        std::unique_lock lock(m_connections_mutex);
        m_connections.clear();
    }

    {
        std::unique_lock lock(m_listeners_mutex);
        m_listeners.clear();
    }

    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Shutdown,
        "TCP backend shutdown");
}

// ─────────────────────────────────────────────────────────────────────────────
// Endpoint management
// ─────────────────────────────────────────────────────────────────────────────

uint64_t TCPBackend::open_endpoint(const EndpointInfo& info)
{
    bool is_listener = !info.remote_address.empty() ? false
                                                    : (info.local_port > 0);

    if (is_listener) {
        auto listener = std::make_unique<ListenerState>(m_context);
        listener->info = info;
        listener->info.state = EndpointState::OPENING;

        asio::error_code ec;
        auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), info.local_port);

        if (listener->acceptor.open(endpoint.protocol(), ec)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
                "Failed to open TCP acceptor: {}", ec.message());
            return 0;
        }

        if (listener->acceptor.set_option(asio::socket_base::reuse_address(true), ec)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
                "Failed to set SO_REUSEADDR on TCP acceptor: {}", ec.message());
            return 0;
        }

        if (listener->acceptor.bind(endpoint, ec)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
                "Failed to bind TCP acceptor to port {}: {}", info.local_port, ec.message());
            return 0;
        }

        if (listener->acceptor.listen(asio::socket_base::max_listen_connections, ec)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
                "Failed to listen on port {}: {}", info.local_port, ec.message());
            return 0;
        }

        listener->info.state = EndpointState::OPEN;

        auto* raw = listener.get();

        {
            std::unique_lock lock(m_listeners_mutex);
            m_listeners[info.id] = std::move(listener);
        }

        if (m_state_callback) {
            m_state_callback(raw->info, EndpointState::OPENING, EndpointState::OPEN);
        }

        start_accept(*raw);

        MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
            "TCP listener {} opened on port {}", info.id, info.local_port);

        return info.id;
    }

    auto conn = std::make_unique<ConnectionState>(m_context);
    conn->info = info;
    conn->info.state = EndpointState::OPENING;
    conn->is_outbound = true;

    auto* raw = conn.get();

    {
        std::unique_lock lock(m_connections_mutex);
        m_connections[info.id] = std::move(conn);
    }

    start_connect(*raw);

    MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
        "TCP connection {} opening to {}:{}", info.id, info.remote_address, info.remote_port);

    return info.id;
}

void TCPBackend::close_endpoint(uint64_t endpoint_id)
{
    {
        std::unique_lock lock(m_connections_mutex);
        auto it = m_connections.find(endpoint_id);
        if (it != m_connections.end()) {
            asio::error_code ec;

            if (!it->second->socket.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "Error closing TCP connection {}: {}", endpoint_id, ec.message());
            }

            if (it->second->reconnect_timer) {
                it->second->reconnect_timer->cancel();
            }
            m_connections.erase(it);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "TCP connection {} closed", endpoint_id);
            return;
        }
    }

    {
        std::unique_lock lock(m_listeners_mutex);
        auto it = m_listeners.find(endpoint_id);
        if (it != m_listeners.end()) {
            asio::error_code ec;

            if (it->second->acceptor.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "Error closing TCP listener {}: {}", endpoint_id, ec.message());
            }

            m_listeners.erase(it);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "TCP listener {} closed", endpoint_id);
        }
    }
}

EndpointState TCPBackend::get_endpoint_state(uint64_t endpoint_id) const
{
    {
        std::shared_lock lock(m_connections_mutex);
        auto it = m_connections.find(endpoint_id);
        if (it != m_connections.end()) {
            return it->second->info.state;
        }
    }

    {
        std::shared_lock lock(m_listeners_mutex);
        auto it = m_listeners.find(endpoint_id);
        if (it != m_listeners.end()) {
            return it->second->info.state;
        }
    }

    return EndpointState::CLOSED;
}

std::vector<EndpointInfo> TCPBackend::get_endpoints() const
{
    std::vector<EndpointInfo> result;

    {
        std::shared_lock lock(m_connections_mutex);
        result.reserve(m_connections.size());
        for (const auto& [id, conn] : m_connections) {
            result.push_back(conn->info);
        }
    }

    {
        std::shared_lock lock(m_listeners_mutex);
        for (const auto& [id, listener] : m_listeners) {
            result.push_back(listener->info);
        }
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data transfer
// ─────────────────────────────────────────────────────────────────────────────

bool TCPBackend::send(uint64_t endpoint_id, const uint8_t* data, size_t size)
{
    std::shared_lock lock(m_connections_mutex);
    auto it = m_connections.find(endpoint_id);
    if (it == m_connections.end()) {
        return false;
    }

    auto& conn = *it->second;
    if (conn.info.state != EndpointState::OPEN) {
        return false;
    }

    auto frame = std::make_shared<std::vector<uint8_t>>(sizeof(uint32_t) + size);
    uint32_t net_len = htonl(static_cast<uint32_t>(size));
    std::memcpy(frame->data(), &net_len, sizeof(uint32_t));
    std::memcpy(frame->data() + sizeof(uint32_t), data, size);

    asio::async_write(
        conn.socket,
        asio::buffer(*frame),
        [frame, endpoint_id, this](const asio::error_code& ec, size_t) {
            if (ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "TCP write failed on endpoint {}: {}", endpoint_id, ec.message());

                std::shared_lock lk(m_connections_mutex);
                auto cit = m_connections.find(endpoint_id);
                if (cit != m_connections.end()) {
                    on_connection_error(*cit->second, ec);
                }
            }
        });

    return true;
}

bool TCPBackend::send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
    const std::string&, uint16_t)
{
    return send(endpoint_id, data, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void TCPBackend::set_receive_callback(NetworkReceiveCallback callback)
{
    m_receive_callback = std::move(callback);
}

void TCPBackend::set_state_callback(EndpointStateCallback callback)
{
    m_state_callback = std::move(callback);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async connect
// ─────────────────────────────────────────────────────────────────────────────

void TCPBackend::start_connect(ConnectionState& conn)
{
    asio::error_code ec;
    auto addr = asio::ip::make_address(conn.info.remote_address, ec);
    if (ec) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Invalid remote address '{}': {}", conn.info.remote_address, ec.message());
        transition_state(conn.info, EndpointState::ERROR);
        return;
    }

    asio::ip::tcp::endpoint target(addr, conn.info.remote_port);

    conn.socket.async_connect(
        target,
        [this, ep_id = conn.info.id](const asio::error_code& connect_ec) {
            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }

            auto& c = *it->second;

            if (connect_ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "TCP connect failed for endpoint {}: {}",
                    ep_id, connect_ec.message());

                if (m_config.auto_reconnect) {
                    transition_state(c.info, EndpointState::RECONNECTING);
                    schedule_reconnect(c);
                } else {
                    transition_state(c.info, EndpointState::ERROR);
                }
                return;
            }

            transition_state(c.info, EndpointState::OPEN);
            start_receive_chain(c);

            MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
                "TCP endpoint {} connected to {}:{}",
                ep_id, c.info.remote_address, c.info.remote_port);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async accept
// ─────────────────────────────────────────────────────────────────────────────

void TCPBackend::start_accept(ListenerState& listener)
{
    listener.pending_socket = asio::ip::tcp::socket(m_context);

    listener.acceptor.async_accept(
        listener.pending_socket,
        [this, listener_id = listener.info.id](const asio::error_code& ec) {
            std::shared_lock lock(m_listeners_mutex);
            auto it = m_listeners.find(listener_id);
            if (it == m_listeners.end()) {
                return;
            }

            auto& lst = *it->second;

            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    return;
                }
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "TCP accept error on listener {}: {}", listener_id, ec.message());
                start_accept(lst);
                return;
            }

            uint64_t new_id = 0;
            if (m_allocate_endpoint_id) {
                new_id = m_allocate_endpoint_id();
            }

            auto peer_ep = lst.pending_socket.remote_endpoint();
            std::string peer_addr = peer_ep.address().to_string();
            uint16_t peer_port = peer_ep.port();

            auto conn = std::make_unique<ConnectionState>(m_context);
            conn->socket = std::move(lst.pending_socket);
            conn->is_outbound = false;
            conn->info.id = new_id;
            conn->info.transport = NetworkTransport::TCP;
            conn->info.role = EndpointRole::BIDIRECTIONAL;
            conn->info.state = EndpointState::OPEN;
            conn->info.remote_address = peer_addr;
            conn->info.remote_port = peer_port;
            conn->info.local_port = lst.info.local_port;

            auto* raw = conn.get();

            {
                std::unique_lock conn_lock(m_connections_mutex);
                m_connections[new_id] = std::move(conn);
            }

            if (m_state_callback) {
                m_state_callback(raw->info, EndpointState::CLOSED, EndpointState::OPEN);
            }

            start_receive_chain(*raw);

            MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
                "TCP accepted connection {} from {}:{} on listener {}",
                new_id, peer_addr, peer_port, listener_id);

            start_accept(lst);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: framed receive chain
// ─────────────────────────────────────────────────────────────────────────────

void TCPBackend::start_receive_chain(ConnectionState& conn)
{
    asio::async_read(
        conn.socket,
        asio::buffer(conn.header_buf),
        [this, ep_id = conn.info.id](const asio::error_code& ec, size_t bytes) {
            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }
            on_header_received(*it->second, ec, bytes);
        });
}

void TCPBackend::on_header_received(ConnectionState& conn,
    const asio::error_code& ec, size_t)
{
    if (ec) {
        on_connection_error(conn, ec);
        return;
    }

    uint32_t net_len {};
    std::memcpy(&net_len, conn.header_buf.data(), sizeof(uint32_t));
    uint32_t payload_size = ntohl(net_len);

    if (payload_size == 0 || payload_size > 64 * 1024 * 1024) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "TCP endpoint {} received invalid frame length: {}",
            conn.info.id, payload_size);
        on_connection_error(conn, asio::error::message_size);
        return;
    }

    conn.payload_buf.resize(payload_size);

    asio::async_read(
        conn.socket,
        asio::buffer(conn.payload_buf),
        [this, ep_id = conn.info.id](const asio::error_code& read_ec, size_t bytes) {
            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }
            on_payload_received(*it->second, read_ec, bytes);
        });
}

void TCPBackend::on_payload_received(ConnectionState& conn,
    const asio::error_code& ec, size_t /*bytes*/)
{
    if (ec) {
        on_connection_error(conn, ec);
        return;
    }

    if (m_receive_callback) {
        std::string peer_str = conn.info.remote_address + ":" + std::to_string(conn.info.remote_port);

        m_receive_callback(conn.info.id,
            conn.payload_buf.data(), conn.payload_buf.size(),
            peer_str);
    }

    start_receive_chain(conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: error handling and reconnect
// ─────────────────────────────────────────────────────────────────────────────

void TCPBackend::on_connection_error(ConnectionState& conn, const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted) {
        return;
    }

    if (ec == asio::error::eof) {
        MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
            "TCP endpoint {} peer disconnected", conn.info.id);
    } else {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "TCP endpoint {} error: {}", conn.info.id, ec.message());
    }

    asio::error_code close_ec;
    if (!conn.socket.close(close_ec)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Error closing TCP socket for endpoint {}: {}", conn.info.id, close_ec.message());
    }

    if (conn.is_outbound && m_config.auto_reconnect) {
        transition_state(conn.info, EndpointState::RECONNECTING);
        schedule_reconnect(conn);
    } else {
        transition_state(conn.info, EndpointState::ERROR);
    }
}

void TCPBackend::schedule_reconnect(ConnectionState& conn)
{
    if (!conn.reconnect_timer) {
        conn.reconnect_timer = std::make_unique<asio::steady_timer>(m_context);
    }

    conn.reconnect_timer->expires_after(
        std::chrono::milliseconds(m_config.reconnect_interval_ms));

    conn.reconnect_timer->async_wait(
        [this, ep_id = conn.info.id](const asio::error_code& ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }

            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }

            auto& c = *it->second;

            c.socket = asio::ip::tcp::socket(m_context);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "TCP endpoint {} attempting reconnect to {}:{}",
                ep_id, c.info.remote_address, c.info.remote_port);

            start_connect(c);
        });
}

void TCPBackend::transition_state(EndpointInfo& info, EndpointState new_state)
{
    EndpointState prev = info.state;
    info.state = new_state;

    if (m_state_callback && prev != new_state) {
        m_state_callback(info, prev, new_state);
    }
}

} // namespace MayaFlux::Core
