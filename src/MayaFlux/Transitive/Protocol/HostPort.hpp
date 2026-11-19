#pragma once

namespace MayaFlux::Transitive::Protocol {

//=============================================================================
// Network addresses written as text
//=============================================================================

/**
 * @struct HostPort
 * @brief A host and a port, as written in "host:port".
 */
struct HostPort {
    std::string host;
    uint16_t port {};
};

/**
 * @brief Read a port number, 1 to 65535.
 *
 * Digits only, with no sign, no spaces and no leading or trailing text. Zero is
 * refused, since it means "any port" to the system and names nothing to connect to.
 *
 * @param text Candidate text.
 * @return The port, or nothing when the text is not one.
 */
[[nodiscard]] constexpr std::optional<uint16_t> parse_port(std::string_view text) noexcept
{
    if (text.empty() || text.size() > 5) {
        return std::nullopt;
    }

    uint32_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10U + static_cast<uint32_t>(c - '0');
    }

    if (value == 0 || value > 65535U) {
        return std::nullopt;
    }
    return static_cast<uint16_t>(value);
}

/**
 * @brief Read "host:port", where the host is a name, an IPv4 address or a bracketed IPv6 address.
 *
 * The split is at the last colon, so an IPv6 address must be bracketed: "[::1]:9000".
 * The host is returned without its brackets. A host with a space or a control
 * character, an empty host, or a missing or invalid port refuses the whole text.
 *
 * @param text Candidate text.
 * @return The host and port, or nothing when the text is not one.
 */
[[nodiscard]] inline std::optional<HostPort> parse_host_port(std::string_view text)
{
    const size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view host = text.substr(0, colon);
    const auto port = parse_port(text.substr(colon + 1));
    if (!port) {
        return std::nullopt;
    }

    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
        host = host.substr(1, host.size() - 2);
    } else if (host.find(':') != std::string_view::npos) {
        return std::nullopt;
    }

    if (host.empty()) {
        return std::nullopt;
    }
    for (const char c : host) {
        if (static_cast<unsigned char>(c) <= ' ' || c == 0x7F) {
            return std::nullopt;
        }
    }

    return HostPort { .host = std::string(host), .port = *port };
}

} // namespace MayaFlux::Transitive::Protocol
