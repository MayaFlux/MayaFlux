#pragma once

#include "NetworkBackend.hpp"

#include "MayaFlux/Transitive/Memory/FlatMap.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
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

    inline StreamFramer make_stream_framer(FramingPolicy framing, size_t max_message_size)
    {
        if (framing == FramingPolicy::LINE) {
            return StreamFramer { std::in_place_type<Transitive::Protocol::LineFramer>,
                max_message_size != 0 ? max_message_size : Transitive::Protocol::default_max_line };
        }
        if (framing == FramingPolicy::NONE) {
            return StreamFramer { std::in_place_type<RawChunkFramer> };
        }
        return StreamFramer { std::in_place_type<Transitive::Protocol::LengthPrefixFramer>,
            max_message_size != 0 ? max_message_size : Transitive::Protocol::default_max_frame, true };
    }

    /**
     * @brief A transport whose stream opens in one synchronous call instead of async_connect
     */
    template <typename Transport>
    concept SynchronousOpen = requires(typename Transport::Stream& stream, const EndpointInfo& info) {
        { Transport::open(stream, info) } -> std::same_as<asio::error_code>;
    };

    /**
     * @brief What a listener holds, which a transport without listeners does not have
     */
    template <typename Transport, bool = Transport::listens>
    struct AcceptSide {
        typename Transport::Protocol::acceptor acceptor;
        typename Transport::Protocol::socket pending_socket;
        typename Transport::Bound bound;

        explicit AcceptSide(asio::io_context& ctx)
            : acceptor(ctx)
            , pending_socket(ctx)
        {
        }
    };

    template <typename Transport>
    struct AcceptSide<Transport, false> {
        explicit AcceptSide(asio::io_context&) { }
    };

}

/**
 * @class StreamBackend
 * @brief Connection-oriented reliable message transport over an Asio stream protocol
 *
 * Everything that does not depend on the stream type lives here: listener and
 * connection bookkeeping, async accept, connect and reconnect, the framed receive
 * chain and the framed send. A Transport policy supplies what differs, so TCP and
 * Unix domain sockets are two instances of one backend.
 *
 * A Transport provides:
 *   - Stream: the connection's Asio stream, with async_read_some, close and
 *     asio::async_write (a protocol's socket, or asio::serial_port).
 *   - Config: a struct with auto_reconnect and reconnect_interval_ms.
 *   - id, label, name: the NetworkTransport, a log label and the backend name.
 *   - automatic_framing: what FramingPolicy::AUTO means.
 *   - describe(info) for logs and peer_label(info) for the sender string of a receive.
 *   - listens: whether the transport has listeners. If so, also Protocol (an Asio stream
 *     protocol), Bound (what a listener holds while bound, default constructible),
 *     is_listener(info), bind(acceptor, info), release(bound) and
 *     describe_accepted(socket, listener, accepted).
 *   - Either connect_target(info), which names where async_connect goes, or
 *     open(stream, info), a synchronous open returning an asio::error_code.
 *
 * With a synchronous open, open_endpoint returns 0 when the first open fails and
 * auto_reconnect is off. With it on, the endpoint enters RECONNECTING and retries.
 */
template <typename Transport>
class StreamBackend : public INetworkBackend {
public:
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

    [[nodiscard]] size_t get_write_backlog(uint64_t endpoint_id) const override;

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
    static constexpr size_t k_default_read_size = size_t { 64 } * 1024;

    using Stream = typename Transport::Stream;

    /**
     * One connection. info is fixed once the connection is published, and state, which the
     * io thread writes and any thread reads, is atomic. The socket, the read buffer and the
     * framer belong to the io thread. Handlers hold a shared_ptr, so a connection that
     * close_endpoint removes stays valid until the handlers that still hold it return.
     */
    struct ConnectionState {
        using Frame = std::shared_ptr<const std::vector<uint8_t>>;

        EndpointInfo info;
        std::atomic<EndpointState> state { EndpointState::CLOSED };
        std::atomic<bool> closed { false };
        std::atomic<size_t> pending_bytes { 0 };
        Stream socket;
        std::vector<uint8_t> read_buffer;
        detail::StreamFramer framer;
        std::unique_ptr<asio::steady_timer> reconnect_timer;
        std::vector<Frame> outbox;
        size_t outbox_head {};
        bool writing {};
        bool is_outbound {};

        ConnectionState(asio::io_context& ctx, FramingPolicy framing, size_t max_message_size, size_t read_size)
            : socket(ctx)
            , read_buffer(read_size != 0 ? read_size : k_default_read_size)
            , framer(detail::make_stream_framer(framing, max_message_size))
        {
        }

        [[nodiscard]] EndpointInfo snapshot() const
        {
            EndpointInfo copy = info;
            copy.state = state.load();
            return copy;
        }

        /**
         * The outbox is a vector with a head index, not a deque: it is touched only by the io
         * thread, it rarely holds more than a frame or two, and a vector reuses its storage
         * where a deque allocates a block per few elements, which on MSVC is one per frame.
         */
        void push(Frame frame) { outbox.push_back(std::move(frame)); }

        [[nodiscard]] bool has_queued() const { return outbox_head < outbox.size(); }

        [[nodiscard]] const Frame& front() const { return outbox[outbox_head]; }

        void pop()
        {
            outbox[outbox_head++].reset();
            if (outbox_head == outbox.size()) {
                outbox.clear();
                outbox_head = 0;
            } else if (outbox_head >= 64 && outbox_head * 2 >= outbox.size()) {
                outbox.erase(outbox.begin(), outbox.begin() + static_cast<std::ptrdiff_t>(outbox_head));
                outbox_head = 0;
            }
        }
    };

    struct ListenerState {
        EndpointInfo info;
        std::atomic<bool> closed { false };
        detail::AcceptSide<Transport> accept;

        explicit ListenerState(asio::io_context& ctx)
            : accept(ctx)
        {
        }
    };

    using ConnectionPtr = std::shared_ptr<ConnectionState>;
    using ListenerPtr = std::shared_ptr<ListenerState>;
    using ConnectionTable = Memory::FlatMap<uint64_t, ConnectionPtr>;
    using ListenerTable = Memory::FlatMap<uint64_t, ListenerPtr>;

    Config m_config;
    asio::io_context& m_context;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    Memory::SnapshotCell<ConnectionTable> m_connections;
    Memory::SnapshotCell<ListenerTable> m_listeners;

    NetworkReceiveCallback m_receive_callback;
    EndpointStateCallback m_state_callback;
    std::function<uint64_t()> m_allocate_endpoint_id;

    [[nodiscard]] static constexpr FramingPolicy resolve_framing(FramingPolicy framing)
    {
        return framing == FramingPolicy::AUTO ? Transport::automatic_framing : framing;
    }

    [[nodiscard]] ConnectionPtr find_connection(uint64_t endpoint_id) const;
    [[nodiscard]] ListenerPtr find_listener(uint64_t endpoint_id) const;

    void close_listener(ListenerState& listener);
    void enqueue_write(const ConnectionPtr& conn, ConnectionState::Frame frame);
    void start_write(const ConnectionPtr& conn);
    void drop_queued_writes(ConnectionState& conn);
    void start_connect(const ConnectionPtr& conn);
    void on_connect_complete(const ConnectionPtr& conn, const asio::error_code& ec);
    void start_accept(const ListenerPtr& listener);
    void start_receive_chain(const ConnectionPtr& conn);
    void on_bytes_received(const ConnectionPtr& conn, const asio::error_code& ec, size_t bytes);
    void on_connection_error(const ConnectionPtr& conn, const asio::error_code& ec);
    void schedule_reconnect(const ConnectionPtr& conn);
    void transition_state(ConnectionState& conn, EndpointState new_state);
};

} // namespace MayaFlux::Core
