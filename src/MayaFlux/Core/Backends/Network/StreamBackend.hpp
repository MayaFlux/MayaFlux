#pragma once

#include "NetworkBackend.hpp"

#include "MayaFlux/Transitive/Protocol/Framing.hpp"

#include <expected>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#if defined(_WIN32) || defined(_WIN64)
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace MayaFlux::Core {

namespace detail {

    /**
     * @brief The framer for raw streams: every read is one message, with nothing to reassemble.
     */
    struct RawChunkFramer {
        template <Transitive::Protocol::MessageSink Sink>
        Transitive::Protocol::FrameReport push(Transitive::Protocol::Message bytes, Sink&& sink)
        {
            Transitive::Protocol::FrameReport report;
            if (!bytes.empty()) {
                sink(bytes);
                report.messages = 1;
            }
            return report;
        }

        void reset() noexcept { }
    };

    using StreamFramer = std::variant<Transitive::Protocol::LengthPrefixFramer,
        Transitive::Protocol::LineFramer, RawChunkFramer>;

    inline StreamFramer make_stream_framer(FramingPolicy framing)
    {
        if (framing == FramingPolicy::LINE) {
            return StreamFramer { std::in_place_type<Transitive::Protocol::LineFramer> };
        }
        if (framing == FramingPolicy::NONE) {
            return StreamFramer { std::in_place_type<RawChunkFramer> };
        }
        return StreamFramer { std::in_place_type<Transitive::Protocol::LengthPrefixFramer>,
            Transitive::Protocol::default_max_frame, true };
    }

}

/**
 * @class StreamBackend
 * @brief Connection-oriented reliable message transport over an Asio stream protocol
 *
 * Everything that does not depend on the socket type lives here: listener and
 * connection bookkeeping, async accept, connect and reconnect, the framed receive
 * chain and the framed send. A Transport policy supplies what differs, so TCP and
 * Unix domain sockets are two instances of one backend.
 *
 * A Transport provides:
 *   - Protocol: an Asio stream protocol (asio::ip::tcp, asio::local::stream_protocol).
 *   - Config: a struct with auto_reconnect and reconnect_interval_ms.
 *   - Bound: what a listener holds while it is bound, default constructible.
 *   - id, label, name: the NetworkTransport, a log label and the backend name.
 *   - is_listener(info), bind(acceptor, info), release(bound).
 *   - connect_target(info), describe_accepted(socket, listener, accepted).
 *   - describe(info) for logs and peer_label(info) for the sender string of a receive.
 *
 * Endpoints are listeners or connections. A listener announces each accepted
 * connection through the state callback as a new endpoint whose parent_id is the
 * listener. Messages are delimited by EndpointInfo::framing, chosen per endpoint
 * and inherited by a listener's connections. AUTO and LENGTH_PREFIX are a four byte
 * big-endian length followed by the payload, where a zero length, or one above
 * 64 MiB, closes the connection. LINE delivers each LF terminated line as a message
 * and drops a line over 1 MiB without closing the connection. NONE is raw bytes: each
 * read is delivered as one message with no boundaries, and a send writes its payload
 * as it is. CHUNKED is refused.
 *
 * All I/O is async on the shared io_context.
 */
template <typename Transport>
class StreamBackend : public INetworkBackend {
public:
    using Protocol = typename Transport::Protocol;
    using Config = typename Transport::Config;

    /**
     * @brief Construct with config and a reference to the shared io_context
     * @param config  Transport specific configuration.
     * @param context The io_context owned by NetworkSubsystem.
     */
    StreamBackend(const Config& config, asio::io_context& context);
    ~StreamBackend() override;

    StreamBackend(const StreamBackend&) = delete;
    StreamBackend& operator=(const StreamBackend&) = delete;
    StreamBackend(StreamBackend&&) = delete;
    StreamBackend& operator=(StreamBackend&&) = delete;

    bool initialize() override;
    void start() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }

    [[nodiscard]] NetworkTransport get_transport() const override { return Transport::id; }
    [[nodiscard]] std::string get_name() const override { return std::string(Transport::name); }
    [[nodiscard]] std::string get_version() const override { return "1.0"; }

    uint64_t open_endpoint(const EndpointInfo& info) override;
    void close_endpoint(uint64_t endpoint_id) override;
    [[nodiscard]] EndpointState get_endpoint_state(uint64_t endpoint_id) const override;
    [[nodiscard]] std::vector<EndpointInfo> get_endpoints() const override;

    bool send(uint64_t endpoint_id, const uint8_t* data, size_t size) override;
    bool send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
        const std::string& address, uint16_t port) override;

    void set_receive_callback(NetworkReceiveCallback callback) override;
    void set_state_callback(EndpointStateCallback callback) override;

    /**
     * @brief Set the endpoint id allocator (called by NetworkSubsystem)
     *
     * Required for listener-accepted connections that need globally
     * unique ids. Set before start().
     */
    void set_endpoint_id_allocator(std::function<uint64_t()> allocator) override
    {
        m_allocate_endpoint_id = std::move(allocator);
    }

private:
    static constexpr size_t k_read_chunk = size_t { 64 } * 1024;

    using Bound = typename Transport::Bound;

    struct ConnectionState {
        EndpointInfo info;
        typename Protocol::socket socket;
        std::vector<uint8_t> read_buffer;
        detail::StreamFramer framer;
        std::unique_ptr<asio::steady_timer> reconnect_timer;
        bool is_outbound {};

        ConnectionState(asio::io_context& ctx, FramingPolicy framing)
            : socket(ctx)
            , read_buffer(k_read_chunk)
            , framer(detail::make_stream_framer(framing))
        {
        }
    };

    struct ListenerState {
        EndpointInfo info;
        typename Protocol::acceptor acceptor;
        typename Protocol::socket pending_socket;
        Bound bound;

        explicit ListenerState(asio::io_context& ctx)
            : acceptor(ctx)
            , pending_socket(ctx)
        {
        }
    };

    Config m_config;
    asio::io_context& m_context;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    mutable std::shared_mutex m_connections_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<ConnectionState>> m_connections;

    mutable std::shared_mutex m_listeners_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<ListenerState>> m_listeners;

    NetworkReceiveCallback m_receive_callback;
    EndpointStateCallback m_state_callback;
    std::function<uint64_t()> m_allocate_endpoint_id;

    void close_listener(ListenerState& listener);
    void start_connect(ConnectionState& conn);
    void start_accept(ListenerState& listener);
    void start_receive_chain(ConnectionState& conn);
    void on_bytes_received(ConnectionState& conn, const asio::error_code& ec, size_t bytes);
    void on_connection_error(ConnectionState& conn, const asio::error_code& ec);
    void schedule_reconnect(ConnectionState& conn);
    void transition_state(EndpointInfo& info, EndpointState new_state);
};

} // namespace MayaFlux::Core
