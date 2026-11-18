#include "UDSBackend.hpp"

#include "StreamBackend.inl"

#include "MayaFlux/Transitive/Platform/UnixSocket.hpp"

namespace MayaFlux::Core {

namespace {

    bool socket_answers(const std::filesystem::path& path)
    {
        using Stream = asio::local::stream_protocol;
        try {
            asio::io_context context;
            Stream::socket probe(context);
            probe.open(Stream());
            probe.non_blocking(true);

            asio::error_code ec;
            (void)probe.connect(Stream::endpoint(path.string()), ec);

            return !(ec == asio::error::connection_refused
                || ec == std::errc::no_such_file_or_directory);
        } catch (const std::exception&) {
            return true;
        }
    }

    Platform::UnixSocketSource source_of(const std::string& text)
    {
        if (text.find_first_of("/\\") != std::string::npos) {
            return Platform::UnixSocketFromPath { .path = text };
        }
        return Platform::UnixSocketFromName { .file_name = text };
    }

    std::string refusal(const Platform::AcquireFailure& failure)
    {
        using Reason = Platform::AcquireFailure::Reason;

        std::string text;
        switch (failure.reason) {
        case Reason::EmptyRequest:
            text = "empty request";
            break;
        case Reason::InvalidName:
            text = "invalid name";
            break;
        case Reason::Missing:
            text = "not found";
            break;
        case Reason::NotDirectory:
            text = "not a directory";
            break;
        case Reason::NotRegularFile:
            text = "not a regular file";
            break;
        case Reason::MarkerMissing:
            text = "marker file missing";
            break;
        case Reason::TooLong:
            text = "path too long";
            break;
        case Reason::Unsafe:
            text = "unsafe location";
            break;
        case Reason::NotSocket:
            text = "not a socket";
            break;
        case Reason::InUse:
            text = "already in use";
            break;
        case Reason::StaleNotRemoved:
            text = "stale socket could not be removed";
            break;
        }
        if (!failure.request.empty()) {
            text += ": " + failure.request;
        }
        return "unix socket refused, " + text;
    }

} // namespace

bool UdsTransport::is_listener(const EndpointInfo& info)
{
    return info.remote_path.empty() && !info.local_path.empty();
}

std::expected<UdsTransport::Bound, std::string> UdsTransport::bind(
    Protocol::acceptor& acceptor, const EndpointInfo& info)
{
    const auto acquired = Platform::acquire_unix_socket(source_of(info.local_path), socket_answers);
    if (!acquired) {
        return std::unexpected(refusal(acquired.error()));
    }

    Bound bound { .path = acquired->path };
    const Protocol::endpoint endpoint(bound.path.string());
    asio::error_code ec;

    if (acceptor.open(endpoint.protocol(), ec)) {
        return std::unexpected("failed to open acceptor: " + ec.message());
    }
    if (acceptor.bind(endpoint, ec)) {
        return std::unexpected("failed to bind " + bound.path.string() + ": " + ec.message());
    }

    const auto fail = [&](const std::string& message) -> std::unexpected<std::string> {
        asio::error_code close_ec;
        (void)acceptor.close(close_ec);
        release(bound);
        return std::unexpected(message);
    };

    if (acceptor.listen(asio::socket_base::max_listen_connections, ec)) {
        return fail("failed to listen on " + bound.path.string() + ": " + ec.message());
    }
    if (const auto restricted = Platform::restrict_unix_socket_to_owner(bound.path); !restricted) {
        return fail(refusal(restricted.error()));
    }

    return bound;
}

void UdsTransport::release(const Bound& bound)
{
    if (bound.path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(bound.path, ec);
}

std::expected<UdsTransport::Protocol::endpoint, std::string> UdsTransport::connect_target(const EndpointInfo& info)
{
    const auto resolved = Platform::resolve_unix_socket_path(source_of(info.remote_path));
    if (!resolved) {
        return std::unexpected(refusal(resolved.error()));
    }
    if (resolved->u8string().size() > Platform::unix_socket_path_limit) {
        return std::unexpected("socket path too long: " + resolved->string());
    }
    return Protocol::endpoint(resolved->string());
}

void UdsTransport::describe_accepted(const Protocol::socket&,
    const EndpointInfo& listener, EndpointInfo& accepted)
{
    accepted.local_path = listener.local_path;
}

std::string UdsTransport::describe(const EndpointInfo& info)
{
    return info.remote_path.empty() ? info.local_path : info.remote_path;
}

std::string UdsTransport::peer_label(const EndpointInfo& info)
{
    return describe(info);
}

template class MAYAFLUX_API StreamBackend<UdsTransport>;

} // namespace MayaFlux::Core
