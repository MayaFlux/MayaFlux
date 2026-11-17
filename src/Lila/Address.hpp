#pragma once

namespace Lila {

/**
 * @struct TcpAddress
 * @brief A TCP host and port.
 *
 * The host is loopback by default, so only this machine can connect. Use
 * "0.0.0.0" to accept other machines. There is no authentication: anyone who
 * can connect can run code.
 */
struct TcpAddress {
    std::string host { "127.0.0.1" };
    int port { 9090 };
};

/**
 * @struct UnixAddress
 * @brief A Unix domain socket: a socket file that only its owner can use.
 *
 * path is where the socket file goes. When empty, the default socket in the
 * user's runtime directory is used.
 */
struct UnixAddress {
    std::optional<std::filesystem::path> path;
};

/**
 * @brief Where a Lila listens. A Lila listens at one address at most.
 */
using Address = std::variant<TcpAddress, UnixAddress>;

} // namespace Lila
