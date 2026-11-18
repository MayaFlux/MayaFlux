#include "TCPBackend.hpp"

#include "StreamBackend.inl"

namespace MayaFlux::Core {

bool TcpTransport::is_listener(const EndpointInfo& info)
{
    return info.remote_address.empty() && info.local_port > 0;
}

std::expected<TcpTransport::Bound, std::string> TcpTransport::bind(
    Protocol::acceptor& acceptor, const EndpointInfo& info)
{
    asio::error_code ec;
    const auto endpoint = Protocol::endpoint(Protocol::v4(), info.local_port);

    if (acceptor.open(endpoint.protocol(), ec)) {
        return std::unexpected("failed to open acceptor: " + ec.message());
    }
    if (acceptor.set_option(asio::socket_base::reuse_address(true), ec)) {
        return std::unexpected("failed to set SO_REUSEADDR: " + ec.message());
    }
    if (acceptor.bind(endpoint, ec)) {
        return std::unexpected("failed to bind port " + std::to_string(info.local_port) + ": " + ec.message());
    }
    if (acceptor.listen(asio::socket_base::max_listen_connections, ec)) {
        return std::unexpected("failed to listen on port " + std::to_string(info.local_port) + ": " + ec.message());
    }

    return Bound {};
}

void TcpTransport::release(const Bound&) { }

std::expected<TcpTransport::Protocol::endpoint, std::string> TcpTransport::connect_target(const EndpointInfo& info)
{
    asio::error_code ec;
    const auto address = asio::ip::make_address(info.remote_address, ec);
    if (ec) {
        return std::unexpected("invalid remote address '" + info.remote_address + "': " + ec.message());
    }
    return Protocol::endpoint(address, info.remote_port);
}

void TcpTransport::describe_accepted(const Protocol::socket& socket,
    const EndpointInfo& listener, EndpointInfo& accepted)
{
    asio::error_code ec;
    const auto peer = socket.remote_endpoint(ec);
    if (!ec) {
        accepted.remote_address = peer.address().to_string();
        accepted.remote_port = peer.port();
    }
    accepted.local_port = listener.local_port;
}

std::string TcpTransport::describe(const EndpointInfo& info)
{
    if (is_listener(info)) {
        return "port " + std::to_string(info.local_port);
    }
    return peer_label(info);
}

std::string TcpTransport::peer_label(const EndpointInfo& info)
{
    return info.remote_address + ":" + std::to_string(info.remote_port);
}

template class MAYAFLUX_API StreamBackend<TcpTransport>;

} // namespace MayaFlux::Core
