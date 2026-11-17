#pragma once

#include "StreamBackend.hpp"

#include <asio/local/stream_protocol.hpp>

namespace MayaFlux::Core {

/**
 * @struct UdsTransport
 * @brief StreamBackend policy for Unix domain sockets
 *
 * An endpoint with a remote_path connects to it. An endpoint with only a
 * local_path listens on it. A path holding a directory separator is used as
 * given, and a bare file name is placed in the user's runtime directory.
 *
 * A listener acquires its path before binding: a stale socket file that nothing
 * answers on is replaced, a live one or anything that is not a socket refuses the
 * endpoint. On POSIX the bound file is restricted to its owner, and the file is
 * removed when the listener closes.
 */
struct UdsTransport {
    using Protocol = asio::local::stream_protocol;
    using Config = UDSBackendInfo;

    struct Bound {
        std::filesystem::path path;
    };

    static constexpr NetworkTransport id = NetworkTransport::UDS;
    static constexpr std::string_view label = "UDS";
    static constexpr std::string_view name = "UDS (Asio)";

    static bool is_listener(const EndpointInfo& info);
    static std::expected<Bound, std::string> bind(Protocol::acceptor& acceptor, const EndpointInfo& info);
    static void release(const Bound& bound);
    static std::expected<Protocol::endpoint, std::string> connect_target(const EndpointInfo& info);
    static void describe_accepted(const Protocol::socket& socket, const EndpointInfo& listener, EndpointInfo& accepted);
    static std::string describe(const EndpointInfo& info);
    static std::string peer_label(const EndpointInfo& info);
};

/**
 * @class UDSBackend
 * @brief Reliable length-prefixed message transport over Unix domain sockets
 *
 * The same-machine counterpart of TCPBackend: one socket path is one endpoint,
 * with the same framing, listener and connection model, reconnection and
 * accepted-connection announcements. See StreamBackend for the shared behaviour
 * and UdsTransport for how a path is resolved and secured.
 *
 * @code
 * EndpointInfo server;
 * server.role = EndpointRole::RECEIVE;
 * server.local_path = "mayaflux-control.sock";
 * auto listen_id = uds.open_endpoint(server);
 *
 * EndpointInfo client;
 * client.role = EndpointRole::BIDIRECTIONAL;
 * client.remote_path = "mayaflux-control.sock";
 * auto ep_id = uds.open_endpoint(client);
 * @endcode
 */
using UDSBackend = StreamBackend<UdsTransport>;

#ifdef MAYAFLUX_EXPORTS
extern template class StreamBackend<UdsTransport>;
#else
extern template class MAYAFLUX_API StreamBackend<UdsTransport>;
#endif

} // namespace MayaFlux::Core
