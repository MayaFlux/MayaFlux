#pragma once

#include "StreamBackend.hpp"

#include <asio/write.hpp>

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

template <typename Transport>
StreamBackend<Transport>::StreamBackend(const Config& config, asio::io_context& context)
    : m_config(config)
    , m_context(context)
{
}

template <typename Transport>
StreamBackend<Transport>::~StreamBackend()
{
    shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
bool StreamBackend<Transport>::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    m_initialized.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "{} backend initialized", Transport::label);

    return true;
}

template <typename Transport>
void StreamBackend<Transport>::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);

    MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
        "{} backend started", Transport::label);
}

template <typename Transport>
void StreamBackend<Transport>::stop()
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
                    "Error closing {} connection {}: {}", Transport::label, id, ec.message());
            }

            if (conn->reconnect_timer) {
                conn->reconnect_timer->cancel();
            }
        }
    }

    {
        std::shared_lock lock(m_listeners_mutex);
        for (auto& [id, listener] : m_listeners) {
            close_listener(*listener);
        }
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::Shutdown,
        "{} backend stopped", Transport::label);
}

template <typename Transport>
void StreamBackend<Transport>::shutdown()
{
    stop();

    {
        std::unique_lock lock(m_connections_mutex);
        m_connections.clear();
    }

    {
        std::unique_lock lock(m_listeners_mutex);
        for (auto& [id, listener] : m_listeners) {
            close_listener(*listener);
        }
        m_listeners.clear();
    }

    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Shutdown,
        "{} backend shutdown", Transport::label);
}

template <typename Transport>
void StreamBackend<Transport>::close_listener(ListenerState& listener)
{
    asio::error_code ec;
    if (listener.acceptor.close(ec)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Error closing {} listener {}: {}", Transport::label, listener.info.id, ec.message());
    }
    Transport::release(listener.bound);
}

// ─────────────────────────────────────────────────────────────────────────────
// Endpoint management
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
uint64_t StreamBackend<Transport>::open_endpoint(const EndpointInfo& info)
{
    if (info.framing == FramingPolicy::CHUNKED) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} refused: CHUNKED framing is not supported on streams",
            Transport::label, info.id);
        return 0;
    }

    if (Transport::is_listener(info)) {
        auto listener = std::make_unique<ListenerState>(m_context);
        listener->info = info;
        listener->info.state = EndpointState::OPENING;

        auto bound = Transport::bind(listener->acceptor, info);
        if (!bound) {
            MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} listener {} failed to open: {}", Transport::label, info.id, bound.error());
            return 0;
        }

        listener->bound = std::move(*bound);
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
            "{} listener {} opened on {}", Transport::label, info.id, Transport::describe(info));

        return info.id;
    }

    auto conn = std::make_unique<ConnectionState>(m_context, info.framing, info.max_message_size);
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
        "{} connection {} opening to {}", Transport::label, info.id, Transport::describe(info));

    return info.id;
}

template <typename Transport>
void StreamBackend<Transport>::close_endpoint(uint64_t endpoint_id)
{
    {
        std::unique_lock lock(m_connections_mutex);
        auto it = m_connections.find(endpoint_id);
        if (it != m_connections.end()) {
            asio::error_code ec;

            if (it->second->socket.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "Error closing {} connection {}: {}", Transport::label, endpoint_id, ec.message());
            }

            if (it->second->reconnect_timer) {
                it->second->reconnect_timer->cancel();
            }
            m_connections.erase(it);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} connection {} closed", Transport::label, endpoint_id);
            return;
        }
    }

    {
        std::unique_lock lock(m_listeners_mutex);
        auto it = m_listeners.find(endpoint_id);
        if (it != m_listeners.end()) {
            close_listener(*it->second);
            m_listeners.erase(it);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} listener {} closed", Transport::label, endpoint_id);
        }
    }
}

template <typename Transport>
EndpointState StreamBackend<Transport>::get_endpoint_state(uint64_t endpoint_id) const
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

template <typename Transport>
std::vector<EndpointInfo> StreamBackend<Transport>::get_endpoints() const
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

template <typename Transport>
bool StreamBackend<Transport>::send(uint64_t endpoint_id, const uint8_t* data, size_t size)
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

    auto frame = std::make_shared<std::vector<uint8_t>>();
    frame->reserve(sizeof(uint32_t) + size);
    const Transitive::Protocol::Message payload { data, size };
    bool encoded = false;
    switch (conn.info.framing) {
    case FramingPolicy::LINE:
        encoded = Transitive::Protocol::encode_line(payload, *frame);
        break;
    case FramingPolicy::NONE:
        encoded = !payload.empty();
        frame->assign(payload.begin(), payload.end());
        break;
    default:
        encoded = Transitive::Protocol::encode_length_prefixed(
            payload, *frame,
            conn.info.max_message_size != 0 ? conn.info.max_message_size : Transitive::Protocol::default_max_frame,
            true);
        break;
    }
    if (!encoded) {
        return false;
    }

    asio::async_write(
        conn.socket,
        asio::buffer(*frame),
        [frame, endpoint_id, this](const asio::error_code& ec, size_t) {
            if (ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "{} write failed on endpoint {}: {}", Transport::label, endpoint_id, ec.message());

                std::shared_lock lk(m_connections_mutex);
                auto cit = m_connections.find(endpoint_id);
                if (cit != m_connections.end()) {
                    on_connection_error(*cit->second, ec);
                }
            }
        });

    return true;
}

template <typename Transport>
bool StreamBackend<Transport>::send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
    const std::string&, uint16_t)
{
    return send(endpoint_id, data, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::set_receive_callback(NetworkReceiveCallback callback)
{
    m_receive_callback = std::move(callback);
}

template <typename Transport>
void StreamBackend<Transport>::set_state_callback(EndpointStateCallback callback)
{
    m_state_callback = std::move(callback);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async connect
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::start_connect(ConnectionState& conn)
{
    auto target = Transport::connect_target(conn.info);
    if (!target) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Invalid {} target for endpoint {}: {}", Transport::label, conn.info.id, target.error());
        transition_state(conn.info, EndpointState::ERROR);
        return;
    }

    conn.socket.async_connect(
        *target,
        [this, ep_id = conn.info.id](const asio::error_code& connect_ec) {
            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }

            auto& c = *it->second;

            if (connect_ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "{} connect failed for endpoint {}: {}",
                    Transport::label, ep_id, connect_ec.message());

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
                "{} endpoint {} connected to {}",
                Transport::label, ep_id, Transport::describe(c.info));
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async accept
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::start_accept(ListenerState& listener)
{
    listener.pending_socket = typename Protocol::socket(m_context);

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
                    "{} accept error on listener {}: {}", Transport::label, listener_id, ec.message());
                start_accept(lst);
                return;
            }

            uint64_t new_id = 0;
            if (m_allocate_endpoint_id) {
                new_id = m_allocate_endpoint_id();
            }

            auto conn = std::make_unique<ConnectionState>(m_context, lst.info.framing, lst.info.max_message_size);
            conn->socket = std::move(lst.pending_socket);
            conn->is_outbound = false;
            conn->info.id = new_id;
            conn->info.transport = Transport::id;
            conn->info.role = EndpointRole::BIDIRECTIONAL;
            conn->info.state = EndpointState::OPEN;
            conn->info.parent_id = listener_id;
            conn->info.framing = lst.info.framing;
            conn->info.max_message_size = lst.info.max_message_size;
            Transport::describe_accepted(conn->socket, lst.info, conn->info);

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
                "{} accepted connection {} from {} on listener {}",
                Transport::label, new_id, Transport::describe(raw->info), listener_id);

            start_accept(lst);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: framed receive chain
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::start_receive_chain(ConnectionState& conn)
{
    conn.socket.async_read_some(
        asio::buffer(conn.read_buffer),
        [this, ep_id = conn.info.id](const asio::error_code& ec, size_t bytes) {
            std::shared_lock lock(m_connections_mutex);
            auto it = m_connections.find(ep_id);
            if (it == m_connections.end()) {
                return;
            }
            on_bytes_received(*it->second, ec, bytes);
        });
}

template <typename Transport>
void StreamBackend<Transport>::on_bytes_received(ConnectionState& conn,
    const asio::error_code& ec, size_t bytes)
{
    if (ec) {
        on_connection_error(conn, ec);
        return;
    }

    const std::string peer = Transport::peer_label(conn.info);
    const Transitive::Protocol::Message chunk { conn.read_buffer.data(), bytes };

    auto deliver = [&](Transitive::Protocol::Message message) {
        if (m_receive_callback) {
            m_receive_callback(conn.info.id, message.data(), message.size(), peer);
        }
    };

    bool corrupt = false;
    size_t dropped = 0;
    std::visit(
        [&](auto& framer) {
            const auto report = framer.push(chunk, deliver);
            dropped = report.discarded;
            if constexpr (std::is_same_v<std::decay_t<decltype(framer)>, Transitive::Protocol::LengthPrefixFramer>) {
                corrupt = !report.ok();
            }
        },
        conn.framer);

    if (corrupt) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} received an invalid frame length",
            Transport::label, conn.info.id);
        on_connection_error(conn, asio::error::message_size);
        return;
    }

    if (dropped != 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} dropped {} line(s) over max_message_size",
            Transport::label, conn.info.id, dropped);
    }

    start_receive_chain(conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: error handling and reconnect
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::on_connection_error(ConnectionState& conn, const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted) {
        return;
    }

    if (ec == asio::error::eof) {
        MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} peer disconnected", Transport::label, conn.info.id);
    } else {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} error: {}", Transport::label, conn.info.id, ec.message());
    }

    asio::error_code close_ec;
    if (conn.socket.close(close_ec)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Error closing {} socket for endpoint {}: {}", Transport::label, conn.info.id, close_ec.message());
    }

    if (conn.is_outbound && m_config.auto_reconnect) {
        transition_state(conn.info, EndpointState::RECONNECTING);
        schedule_reconnect(conn);
    } else {
        transition_state(conn.info, EndpointState::ERROR);
    }
}

template <typename Transport>
void StreamBackend<Transport>::schedule_reconnect(ConnectionState& conn)
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

            c.socket = typename Protocol::socket(m_context);
            std::visit([](auto& framer) { framer.reset(); }, c.framer);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} endpoint {} attempting reconnect to {}",
                Transport::label, ep_id, Transport::describe(c.info));

            start_connect(c);
        });
}

template <typename Transport>
void StreamBackend<Transport>::transition_state(EndpointInfo& info, EndpointState new_state)
{
    EndpointState prev = info.state;
    info.state = new_state;

    if (m_state_callback && prev != new_state) {
        m_state_callback(info, prev, new_state);
    }
}

} // namespace MayaFlux::Core
