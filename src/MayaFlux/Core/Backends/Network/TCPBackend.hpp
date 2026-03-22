#pragma once

#include "NetworkBackend.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

namespace MayaFlux::Core {

/**
 * @class TCPBackend
 * @brief Connection-oriented reliable stream transport over TCP via standalone Asio
 *
 * Two endpoint modes:
 *   - Outbound (remote_address set): async_connect to a remote host.
 *   - Listener (local_port set, no remote_address): async_accept on a port.
 *     Accepted connections become new endpoints, announced via state callback.
 *
 * Framing: every message on the wire is [uint32_t network-order length][payload].
 * The backend handles framing transparently. Callers send and receive complete
 * messages, never partial streams.
 *
 * Reconnection: if auto_reconnect is enabled and a connection drops, the
 * backend transitions to RECONNECTING and schedules async_connect retries
 * via asio::steady_timer.
 *
 * All I/O is async on the shared io_context. No dedicated threads per
 * connection. No mutexes on the data path. One io_context thread handles
 * all sockets: accept, connect, read, write, reconnect timers.
 *
 * Receive chain per connection:
 *   async_read(4 bytes header) -> parse length
 *   -> async_read(length bytes payload) -> fire callback -> resubmit header read
 *
 * Send:
 *   async_write(4 bytes header + payload). Serialised per-connection via
 *   Asio strand (implicit: all operations on one io_context thread).
 *
 * @code
 * asio::io_context ctx;
 * TCPBackend tcp(config, ctx);
 * tcp.initialize();
 * tcp.set_receive_callback([](uint64_t id, const uint8_t* d, size_t s, std::string_view a) {
 *     // handle complete framed message
 * });
 * tcp.set_state_callback([](const EndpointInfo& ep, EndpointState prev, EndpointState cur) {
 *     // track connection/disconnection
 * });
 * tcp.start();
 *
 * // Outbound connection
 * EndpointInfo client_ep;
 * client_ep.role = EndpointRole::BIDIRECTIONAL;
 * client_ep.remote_address = "192.168.1.50";
 * client_ep.remote_port = 7000;
 * auto ep_id = tcp.open_endpoint(client_ep);
 *
 * // Listener
 * EndpointInfo server_ep;
 * server_ep.role = EndpointRole::RECEIVE;
 * server_ep.local_port = 7000;
 * auto listen_id = tcp.open_endpoint(server_ep);
 *
 * ctx.run();
 * @endcode
 */
class MAYAFLUX_API TCPBackend : public INetworkBackend {
public:
    /**
     * @brief Construct with config and a reference to the shared io_context
     * @param config  TCP-specific configuration.
     * @param context The io_context owned by NetworkSubsystem.
     */
    TCPBackend(const TCPBackendInfo& config, asio::io_context& context);
    ~TCPBackend() override;

    TCPBackend(const TCPBackend&) = delete;
    TCPBackend& operator=(const TCPBackend&) = delete;
    TCPBackend(TCPBackend&&) = delete;
    TCPBackend& operator=(TCPBackend&&) = delete;

    // ─── INetworkBackend lifecycle ──────────────────────────────────────────

    bool initialize() override;
    void start() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }

    [[nodiscard]] NetworkTransport get_transport() const override { return NetworkTransport::TCP; }
    [[nodiscard]] std::string get_name() const override { return "TCP (Asio)"; }
    [[nodiscard]] std::string get_version() const override { return "1.0"; }

    // ─── INetworkBackend endpoints ──────────────────────────────────────────

    uint64_t open_endpoint(const EndpointInfo& info) override;
    void close_endpoint(uint64_t endpoint_id) override;
    [[nodiscard]] EndpointState get_endpoint_state(uint64_t endpoint_id) const override;
    [[nodiscard]] std::vector<EndpointInfo> get_endpoints() const override;

    // ─── INetworkBackend data ───────────────────────────────────────────────

    bool send(uint64_t endpoint_id, const uint8_t* data, size_t size) override;
    bool send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
        const std::string& address, uint16_t port) override;

    // ─── INetworkBackend callbacks ──────────────────────────────────────────

    void set_receive_callback(NetworkReceiveCallback callback) override;
    void set_state_callback(EndpointStateCallback callback) override;

private:
    /**
     * @brief State for a connected TCP peer (inbound or outbound)
     */
    struct ConnectionState {
        EndpointInfo info;
        asio::ip::tcp::socket socket;
        std::array<uint8_t, 4> header_buf {};
        std::vector<uint8_t> payload_buf;
        std::unique_ptr<asio::steady_timer> reconnect_timer;
        bool is_outbound {};

        explicit ConnectionState(asio::io_context& ctx)
            : socket(ctx)
        {
        }
    };

    /**
     * @brief State for a listening acceptor
     */
    struct ListenerState {
        EndpointInfo info;
        asio::ip::tcp::acceptor acceptor;
        asio::ip::tcp::socket pending_socket;

        explicit ListenerState(asio::io_context& ctx)
            : acceptor(ctx)
            , pending_socket(ctx)
        {
        }
    };

    TCPBackendInfo m_config;
    asio::io_context& m_context;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    mutable std::shared_mutex m_connections_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<ConnectionState>> m_connections;

    mutable std::shared_mutex m_listeners_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<ListenerState>> m_listeners;

    NetworkReceiveCallback m_receive_callback;
    EndpointStateCallback m_state_callback;

    /**
     * @brief Pointer to subsystem's endpoint id allocator.
     *
     * When a listener accepts an inbound connection, the new connection
     * needs a globally unique endpoint id. The subsystem provides this
     * via a callback set during add_backend().
     */
    std::function<uint64_t()> m_allocate_endpoint_id;

    // ─── Async operation chains ─────────────────────────────────────────────

    /**
     * @brief Initiate async_connect for an outbound endpoint.
     *
     * On success: transitions to OPEN, starts receive chain.
     * On failure: transitions to ERROR or RECONNECTING.
     */
    void start_connect(ConnectionState& conn);

    /**
     * @brief Initiate async_accept loop for a listener.
     *
     * On accept: creates ConnectionState for the new peer, assigns
     * endpoint id via m_allocate_endpoint_id, starts receive chain,
     * fires state callback. Then resubmits async_accept.
     */
    void start_accept(ListenerState& listener);

    /**
     * @brief Start the framed message receive chain on a connection.
     *
     * Posts async_read for 4-byte header. Completion parses length,
     * posts async_read for payload. Completion fires receive callback,
     * then resubmits header read.
     */
    void start_receive_chain(ConnectionState& conn);

    /**
     * @brief Header read completion handler.
     */
    void on_header_received(ConnectionState& conn,
        const asio::error_code& ec, size_t bytes);

    /**
     * @brief Payload read completion handler.
     */
    void on_payload_received(ConnectionState& conn,
        const asio::error_code& ec, size_t bytes);

    /**
     * @brief Handle a connection error (read/write failure).
     *
     * Transitions to ERROR. If auto_reconnect and outbound, schedules
     * reconnect via steady_timer.
     */
    void on_connection_error(ConnectionState& conn, const asio::error_code& ec);

    /**
     * @brief Schedule a reconnect attempt after the configured interval.
     */
    void schedule_reconnect(ConnectionState& conn);

    void transition_state(EndpointInfo& info, EndpointState new_state);

public:
    /**
     * @brief Set the endpoint id allocator (called by NetworkSubsystem)
     *
     * Required for listener-accepted connections that need globally
     * unique ids. Set before start().
     */
    void set_endpoint_id_allocator(std::function<uint64_t()> allocator)
    {
        m_allocate_endpoint_id = std::move(allocator);
    }
};

} // namespace MayaFlux::Core
