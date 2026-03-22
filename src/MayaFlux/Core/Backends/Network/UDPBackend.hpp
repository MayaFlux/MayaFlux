#pragma once

#include "NetworkBackend.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

namespace MayaFlux::Core {

/**
 * @class UDPBackend
 * @brief Connectionless datagram transport over UDP via standalone Asio
 *
 * Each endpoint maps to a (socket, optional default remote) pair.
 * Endpoints that share a local port share the underlying socket.
 *
 * All I/O is async on the io_context provided by NetworkSubsystem.
 * No dedicated threads, no mutexes on the data path.
 *
 *   async_receive_from -> completion fires receive callback -> resubmit
 *   async_send_to      -> fire-and-forget (UDP is unreliable anyway)
 *
 * OSC, Art-Net, or any other datagram protocol is a serialisation
 * concern above this layer. This backend knows nothing about message
 * formats: bytes in, bytes out.
 *
 * @code
 * // Created and started by NetworkSubsystem. Direct use for testing:
 * asio::io_context ctx;
 * UDPBackend udp(config, ctx);
 * udp.initialize();
 * udp.set_receive_callback([](uint64_t id, const uint8_t* d, size_t s, std::string_view a) {
 *     // handle datagram
 * });
 * udp.start();
 *
 * EndpointInfo ep;
 * ep.role = EndpointRole::BIDIRECTIONAL;
 * ep.local_port = 8000;
 * ep.remote_address = "127.0.0.1";
 * ep.remote_port = 9000;
 * auto id = udp.open_endpoint(ep);
 *
 * udp.send(id, payload.data(), payload.size());
 *
 * ctx.run();  // normally driven by NetworkSubsystem's jthread
 * @endcode
 */
class MAYAFLUX_API UDPBackend : public INetworkBackend {
public:
    /**
     * @brief Construct with config and a reference to the shared io_context
     * @param config  UDP-specific configuration.
     * @param context The io_context owned by NetworkSubsystem. All async
     *                operations are posted here.
     */
    UDPBackend(const UDPBackendInfo& config, asio::io_context& context);
    ~UDPBackend() override;

    UDPBackend(const UDPBackend&) = delete;
    UDPBackend& operator=(const UDPBackend&) = delete;
    UDPBackend(UDPBackend&&) = delete;
    UDPBackend& operator=(UDPBackend&&) = delete;

    // ─── INetworkBackend lifecycle ──────────────────────────────────────────

    bool initialize() override;
    void start() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }

    [[nodiscard]] NetworkTransport get_transport() const override { return NetworkTransport::UDP; }
    [[nodiscard]] std::string get_name() const override { return "UDP (Asio)"; }
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
     * @brief Per-bound-port socket state
     *
     * Multiple endpoints can share a socket (same local port, different
     * remote targets). The socket owns the async_receive_from loop.
     */
    struct SocketState {
        asio::ip::udp::socket socket;
        std::array<uint8_t, 65536> recv_buffer {};
        asio::ip::udp::endpoint sender_endpoint;
        uint16_t local_port {};
        uint32_t ref_count {};

        explicit SocketState(asio::io_context& ctx)
            : socket(ctx)
        {
        }
    };

    /**
     * @brief Per-endpoint state
     *
     * Maps an endpoint id to its socket and optional default remote target.
     */
    struct EndpointRecord {
        EndpointInfo info;
        SocketState* socket_state {};
        asio::ip::udp::endpoint default_remote;
        bool has_default_remote {};
    };

    UDPBackendInfo m_config;
    asio::io_context& m_context;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    mutable std::shared_mutex m_endpoints_mutex;
    std::unordered_map<uint64_t, EndpointRecord> m_endpoints;

    mutable std::shared_mutex m_sockets_mutex;
    std::unordered_map<uint16_t, std::unique_ptr<SocketState>> m_sockets;

    NetworkReceiveCallback m_receive_callback;
    EndpointStateCallback m_state_callback;

    /**
     * @brief Get or create a socket bound to local_port.
     *
     * If a socket for this port already exists, increments ref_count
     * and returns it. Otherwise binds a new socket and starts the
     * async receive loop.
     */
    SocketState* acquire_socket(uint16_t local_port);

    /**
     * @brief Decrement ref_count for a socket. Closes if zero.
     */
    void release_socket(uint16_t local_port);

    /**
     * @brief Post the first async_receive_from for a newly bound socket.
     */
    void start_receive_loop(SocketState& state);

    /**
     * @brief Completion handler for async_receive_from.
     *
     * Fires the receive callback with the endpoint id resolved from
     * the sender address, then resubmits async_receive_from.
     */
    void on_receive(SocketState& state, const asio::error_code& ec, size_t bytes_received);

    /**
     * @brief Resolve which endpoint id a received datagram belongs to.
     *
     * For a given socket (local port), checks if any endpoint has a
     * default_remote matching the sender. If none match, falls back to
     * the first RECEIVE or BIDIRECTIONAL endpoint on that port.
     */
    uint64_t resolve_endpoint_for_sender(
        uint16_t local_port, const asio::ip::udp::endpoint& sender) const;

    void transition_state(EndpointRecord& record, EndpointState new_state);
};

} // namespace MayaFlux::Core
