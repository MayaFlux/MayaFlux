#pragma once

#include "StreamBackend.hpp"

#include <asio/ip/tcp.hpp>

namespace MayaFlux::Core {

/**
 * @struct TcpTransport
 * @brief StreamBackend policy for TCP over IP
 *
 * An endpoint with a remote address connects to it. An endpoint with only a
 * local port listens on it. A peer is described as address:port.
 */
struct TcpTransport {
    using Protocol = asio::ip::tcp;
    using Stream = Protocol::socket;
    using Config = TCPBackendInfo;

    struct Bound { };

    static constexpr bool listens = true;
    static constexpr FramingPolicy automatic_framing = FramingPolicy::LENGTH_PREFIX;
    static constexpr NetworkTransport id = NetworkTransport::TCP;
    static constexpr std::string_view label = "TCP";
    static constexpr std::string_view name = "TCP (Asio)";

    static bool is_listener(const EndpointInfo& info);
    /**
     * @brief Open, bind and listen on the endpoint's local port
     *
     * A port with a live listener is refused. A restart can rebind over leftover
     * TIME_WAIT connections: elsewhere by SO_REUSEADDR, on Windows by the plain
     * bind, since SO_REUSEADDR there would let a second socket take a live port.
     */
    static std::expected<Bound, std::string> bind(Protocol::acceptor& acceptor, const EndpointInfo& info);
    static void release(const Bound& bound);
    static std::expected<Protocol::endpoint, std::string> connect_target(const EndpointInfo& info);
    static void describe_accepted(const Protocol::socket& socket, const EndpointInfo& listener, EndpointInfo& accepted);
    static std::string describe(const EndpointInfo& info);
    static std::string peer_label(const EndpointInfo& info);
};

/**
 * @class TCPBackend
 * @brief Reliable length-prefixed message transport over TCP via standalone Asio
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
 * All I/O is async on the shared io_context. See StreamBackend for the shared
 * behaviour.
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
using TCPBackend = StreamBackend<TcpTransport>;

#ifdef MAYAFLUX_EXPORTS
extern template class StreamBackend<TcpTransport>;
#else
extern template class MAYAFLUX_API StreamBackend<TcpTransport>;
#endif

} // namespace MayaFlux::Core
