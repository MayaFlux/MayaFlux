#pragma once

#include "StreamBackend.hpp"

#include <asio/post.hpp>
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

    if (const auto connections = m_connections.read()) {
        for (const auto& [id, conn] : *connections) {
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

    if (const auto listeners = m_listeners.read()) {
        for (const auto& [id, listener] : *listeners) {
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

    if (const auto connections = m_connections.read()) {
        for (const auto& [id, conn] : *connections) {
            conn->closed.store(true);
            asio::error_code ec;
            (void)conn->socket.close(ec);
            if (conn->reconnect_timer) {
                conn->reconnect_timer->cancel();
            }
        }
    }

    if (const auto listeners = m_listeners.read()) {
        for (const auto& [id, listener] : *listeners) {
            close_listener(*listener);
        }
    }

    m_connections.reset();
    m_listeners.reset();

    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Shutdown,
        "{} backend shutdown", Transport::label);
}

template <typename Transport>
void StreamBackend<Transport>::close_listener(ListenerState& listener)
{
    listener.closed.store(true);

    asio::error_code ec;
    if (listener.acceptor.close(ec)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Error closing {} listener {}: {}", Transport::label, listener.info.id, ec.message());
    }
    Transport::release(listener.bound);
}

template <typename Transport>
auto StreamBackend<Transport>::find_connection(uint64_t endpoint_id) const -> ConnectionPtr
{
    const auto table = m_connections.read();
    if (!table) {
        return nullptr;
    }
    const auto it = table->find(endpoint_id);
    return it == table->end() ? nullptr : it->second;
}

template <typename Transport>
auto StreamBackend<Transport>::find_listener(uint64_t endpoint_id) const -> ListenerPtr
{
    const auto table = m_listeners.read();
    if (!table) {
        return nullptr;
    }
    const auto it = table->find(endpoint_id);
    return it == table->end() ? nullptr : it->second;
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
        auto listener = std::make_shared<ListenerState>(m_context);
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

        m_listeners.update([&](ListenerTable& table) {
            table.insert_or_assign(info.id, listener);
        });

        if (m_state_callback) {
            m_state_callback(listener->info, EndpointState::OPENING, EndpointState::OPEN);
        }

        start_accept(listener);

        MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} listener {} opened on {}", Transport::label, info.id, Transport::describe(info));

        return info.id;
    }

    auto conn = std::make_shared<ConnectionState>(m_context, info.framing, info.max_message_size);
    conn->info = info;
    conn->state.store(EndpointState::OPENING);
    conn->is_outbound = true;

    m_connections.update([&](ConnectionTable& table) {
        table.insert_or_assign(info.id, conn);
    });

    start_connect(conn);

    MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
        "{} connection {} opening to {}", Transport::label, info.id, Transport::describe(info));

    return info.id;
}

template <typename Transport>
void StreamBackend<Transport>::close_endpoint(uint64_t endpoint_id)
{
    if (const auto conn = find_connection(endpoint_id)) {
        conn->closed.store(true);

        asio::error_code ec;
        if (conn->socket.close(ec)) {
            MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                "Error closing {} connection {}: {}", Transport::label, endpoint_id, ec.message());
        }

        if (conn->reconnect_timer) {
            conn->reconnect_timer->cancel();
        }

        m_connections.update([endpoint_id](ConnectionTable& table) {
            return table.erase(endpoint_id);
        });

        MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} connection {} closed", Transport::label, endpoint_id);
        return;
    }

    if (const auto listener = find_listener(endpoint_id)) {
        close_listener(*listener);

        m_listeners.update([endpoint_id](ListenerTable& table) {
            return table.erase(endpoint_id);
        });

        MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} listener {} closed", Transport::label, endpoint_id);
    }
}

template <typename Transport>
EndpointState StreamBackend<Transport>::get_endpoint_state(uint64_t endpoint_id) const
{
    if (const auto conn = find_connection(endpoint_id)) {
        return conn->state.load();
    }

    if (const auto listener = find_listener(endpoint_id)) {
        return listener->info.state;
    }

    return EndpointState::CLOSED;
}

template <typename Transport>
std::vector<EndpointInfo> StreamBackend<Transport>::get_endpoints() const
{
    std::vector<EndpointInfo> result;

    if (const auto connections = m_connections.read()) {
        result.reserve(connections->size());
        for (const auto& [id, conn] : *connections) {
            result.push_back(conn->snapshot());
        }
    }

    if (const auto listeners = m_listeners.read()) {
        for (const auto& [id, listener] : *listeners) {
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
    const auto conn = find_connection(endpoint_id);
    if (!conn || conn->closed.load() || conn->state.load() != EndpointState::OPEN) {
        return false;
    }

    auto frame = std::make_shared<std::vector<uint8_t>>();
    frame->reserve(sizeof(uint32_t) + size);
    const Transitive::Protocol::Message payload { data, size };
    bool encoded = false;
    switch (conn->info.framing) {
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
            conn->info.max_message_size != 0 ? conn->info.max_message_size : Transitive::Protocol::default_max_frame,
            true);
        break;
    }
    if (!encoded) {
        return false;
    }

    const size_t unit = conn->info.max_message_size != 0 ? conn->info.max_message_size : Transitive::Protocol::default_max_frame;
    if (conn->pending_bytes.fetch_add(frame->size()) + frame->size() > 4 * unit) {
        conn->pending_bytes.fetch_sub(frame->size());
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} refused a send: the peer has left too much unread",
            Transport::label, endpoint_id);
        return false;
    }

    asio::post(m_context, [this, conn, frame = typename ConnectionState::Frame(std::move(frame))]() mutable {
        enqueue_write(conn, std::move(frame));
    });

    return true;
}

template <typename Transport>
void StreamBackend<Transport>::enqueue_write(const ConnectionPtr& conn, ConnectionState::Frame frame)
{
    if (conn->closed.load() || conn->state.load() != EndpointState::OPEN) {
        conn->pending_bytes.fetch_sub(frame->size());
        return;
    }

    conn->push(std::move(frame));
    if (!conn->writing) {
        start_write(conn);
    }
}

template <typename Transport>
void StreamBackend<Transport>::start_write(const ConnectionPtr& conn)
{
    conn->writing = true;
    const auto frame = conn->front();

    asio::async_write(
        conn->socket,
        asio::buffer(*frame),
        [this, conn, frame](const asio::error_code& ec, size_t) {
            conn->pending_bytes.fetch_sub(frame->size());
            conn->pop();
            conn->writing = false;

            if (ec) {
                drop_queued_writes(*conn);
                if (ec != asio::error::operation_aborted) {
                    MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                        "{} write failed on endpoint {}: {}", Transport::label, conn->info.id, ec.message());
                }
                on_connection_error(conn, ec);
                return;
            }

            if (conn->has_queued() && !conn->closed.load()) {
                start_write(conn);
            }
        });
}

template <typename Transport>
void StreamBackend<Transport>::drop_queued_writes(ConnectionState& conn)
{
    for (size_t i = conn.outbox_head; i < conn.outbox.size(); ++i) {
        conn.pending_bytes.fetch_sub(conn.outbox[i]->size());
    }
    conn.outbox.clear();
    conn.outbox_head = 0;
}

template <typename Transport>
bool StreamBackend<Transport>::send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
    const std::string&, uint16_t)
{
    return send(endpoint_id, data, size);
}

template <typename Transport>
size_t StreamBackend<Transport>::get_write_backlog(uint64_t endpoint_id) const
{
    const auto conn = find_connection(endpoint_id);
    return conn ? conn->pending_bytes.load() : 0;
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
void StreamBackend<Transport>::start_connect(const ConnectionPtr& conn)
{
    auto target = Transport::connect_target(conn->info);
    if (!target) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Invalid {} target for endpoint {}: {}", Transport::label, conn->info.id, target.error());
        transition_state(*conn, EndpointState::ERROR);
        return;
    }

    conn->socket.async_connect(
        *target,
        [this, conn](const asio::error_code& connect_ec) {
            if (conn->closed.load() || connect_ec == asio::error::operation_aborted) {
                return;
            }

            if (connect_ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "{} connect failed for endpoint {}: {}",
                    Transport::label, conn->info.id, connect_ec.message());

                if (m_config.auto_reconnect) {
                    transition_state(*conn, EndpointState::RECONNECTING);
                    schedule_reconnect(conn);
                } else {
                    transition_state(*conn, EndpointState::ERROR);
                }
                return;
            }

            transition_state(*conn, EndpointState::OPEN);
            start_receive_chain(conn);

            MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} endpoint {} connected to {}",
                Transport::label, conn->info.id, Transport::describe(conn->info));
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async accept
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::start_accept(const ListenerPtr& listener)
{
    listener->pending_socket = typename Protocol::socket(m_context);

    listener->acceptor.async_accept(
        listener->pending_socket,
        [this, listener](const asio::error_code& ec) {
            if (listener->closed.load()) {
                return;
            }

            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    return;
                }
                MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
                    "{} accept error on listener {}: {}", Transport::label, listener->info.id, ec.message());
                start_accept(listener);
                return;
            }

            uint64_t new_id = 0;
            if (m_allocate_endpoint_id) {
                new_id = m_allocate_endpoint_id();
            }

            auto conn = std::make_shared<ConnectionState>(
                m_context, listener->info.framing, listener->info.max_message_size);
            conn->socket = std::move(listener->pending_socket);
            conn->is_outbound = false;
            conn->info.id = new_id;
            conn->info.transport = Transport::id;
            conn->info.role = EndpointRole::BIDIRECTIONAL;
            conn->info.parent_id = listener->info.id;
            conn->info.framing = listener->info.framing;
            conn->info.max_message_size = listener->info.max_message_size;
            Transport::describe_accepted(conn->socket, listener->info, conn->info);
            conn->state.store(EndpointState::OPEN);

            m_connections.update([&](ConnectionTable& table) {
                table.insert_or_assign(new_id, conn);
            });

            if (m_state_callback) {
                m_state_callback(conn->snapshot(), EndpointState::CLOSED, EndpointState::OPEN);
            }

            start_receive_chain(conn);

            MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} accepted connection {} from {} on listener {}",
                Transport::label, new_id, Transport::describe(conn->info), listener->info.id);

            start_accept(listener);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: framed receive chain
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::start_receive_chain(const ConnectionPtr& conn)
{
    conn->socket.async_read_some(
        asio::buffer(conn->read_buffer),
        [this, conn](const asio::error_code& ec, size_t bytes) {
            on_bytes_received(conn, ec, bytes);
        });
}

template <typename Transport>
void StreamBackend<Transport>::on_bytes_received(const ConnectionPtr& conn,
    const asio::error_code& ec, size_t bytes)
{
    if (conn->closed.load()) {
        return;
    }

    if (ec) {
        on_connection_error(conn, ec);
        return;
    }

    const std::string peer = Transport::peer_label(conn->info);
    const Transitive::Protocol::Message chunk { conn->read_buffer.data(), bytes };

    auto deliver = [&](Transitive::Protocol::Message message) {
        if (m_receive_callback) {
            m_receive_callback(conn->info.id, message.data(), message.size(), peer);
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
        conn->framer);

    if (corrupt) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} received an invalid frame length",
            Transport::label, conn->info.id);
        on_connection_error(conn, asio::error::message_size);
        return;
    }

    if (dropped != 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} dropped {} line(s) over max_message_size",
            Transport::label, conn->info.id, dropped);
    }

    if (conn->closed.load()) {
        return;
    }

    start_receive_chain(conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: error handling and reconnect
// ─────────────────────────────────────────────────────────────────────────────

template <typename Transport>
void StreamBackend<Transport>::on_connection_error(const ConnectionPtr& conn, const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted) {
        return;
    }

    if (ec == asio::error::eof) {
        MF_INFO(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} peer disconnected", Transport::label, conn->info.id);
    } else {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "{} endpoint {} error: {}", Transport::label, conn->info.id, ec.message());
    }

    asio::error_code close_ec;
    if (conn->socket.close(close_ec)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkBackend,
            "Error closing {} socket for endpoint {}: {}", Transport::label, conn->info.id, close_ec.message());
    }

    if (conn->is_outbound && m_config.auto_reconnect) {
        transition_state(*conn, EndpointState::RECONNECTING);
        schedule_reconnect(conn);
    } else {
        transition_state(*conn, EndpointState::ERROR);
    }
}

template <typename Transport>
void StreamBackend<Transport>::schedule_reconnect(const ConnectionPtr& conn)
{
    if (!conn->reconnect_timer) {
        conn->reconnect_timer = std::make_unique<asio::steady_timer>(m_context);
    }

    conn->reconnect_timer->expires_after(
        std::chrono::milliseconds(m_config.reconnect_interval_ms));

    conn->reconnect_timer->async_wait(
        [this, weak = std::weak_ptr<ConnectionState>(conn)](const asio::error_code& ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }

            const auto c = weak.lock();
            if (!c || c->closed.load()) {
                return;
            }

            c->socket = typename Protocol::socket(m_context);
            std::visit([](auto& framer) { framer.reset(); }, c->framer);

            MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkBackend,
                "{} endpoint {} attempting reconnect to {}",
                Transport::label, c->info.id, Transport::describe(c->info));

            start_connect(c);
        });
}

template <typename Transport>
void StreamBackend<Transport>::transition_state(ConnectionState& conn, EndpointState new_state)
{
    const EndpointState previous = conn.state.exchange(new_state);

    if (m_state_callback && previous != new_state) {
        m_state_callback(conn.snapshot(), previous, new_state);
    }
}

} // namespace MayaFlux::Core
