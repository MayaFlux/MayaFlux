#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#if defined(_WIN32) || defined(_WIN64)
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace MayaFlux::Core {

/**
 * @enum NetworkTransport
 * @brief Identifies the transport protocol a backend implements
 */
enum class NetworkTransport : uint8_t {
    UDP,
    TCP,
    SHARED_MEMORY,
    UDS
};

/**
 * @enum EndpointRole
 * @brief Whether an endpoint sends, receives, or both
 */
enum class EndpointRole : uint8_t {
    SEND,
    RECEIVE,
    BIDIRECTIONAL
};

/**
 * @enum FramingPolicy
 * @brief How messages are delimited on the wire
 *
 * NONE: raw bytes, no framing. A datagram on UDP. On TCP and UDS there are no message
 * boundaries: each read, up to 64 KiB, is delivered as one message and a send writes
 * its bytes as they are.
 * LENGTH_PREFIX: a four byte big-endian size before each payload. Carries any bytes.
 * CHUNKED: split large payloads into MTU-sized chunks with sequence numbers.
 * Not supported by any backend yet; stream endpoints refuse it.
 * LINE: TCP and UDS only. One LF terminated line per message, with one trailing
 * CR accepted on receive, for text peers such as nc and socat. A payload that
 * contains LF cannot be sent.
 * AUTO: the transport's own. NONE on UDP and LENGTH_PREFIX on TCP and UDS.
 */
enum class FramingPolicy : uint8_t {
    NONE,
    LENGTH_PREFIX,
    CHUNKED,
    LINE,
    AUTO
};

/**
 * @enum EndpointState
 * @brief Observable connection state for an endpoint
 */
enum class EndpointState : uint8_t {
    CLOSED,
    OPENING,
    OPEN,
    ERROR,
    RECONNECTING
};

/**
 * @struct EndpointInfo
 * @brief Describes one logical send/receive endpoint managed by a backend
 *
 * Analogous to InputDeviceInfo for InputSubsystem. An endpoint is the
 * network equivalent of a device: something you open, send/receive
 * through, and close.
 */
struct MAYAFLUX_API EndpointInfo {
    uint64_t id {};
    NetworkTransport transport { NetworkTransport::UDP };
    EndpointRole role { EndpointRole::BIDIRECTIONAL };
    EndpointState state { EndpointState::CLOSED };
    std::string label;
    std::string remote_address;
    uint16_t remote_port {};
    uint16_t local_port {};

    /// UDS: socket to listen on. A bare file name is placed in the user's runtime directory.
    std::string local_path;

    /// UDS: socket to connect to, given the same way as local_path.
    std::string remote_path;

    /// Listener that accepted this connection, or 0 for an endpoint opened directly.
    uint64_t parent_id {};

    /// TCP and UDS: how messages are delimited. Both ends must agree, and a listener's
    /// accepted connections use the listener's. UDP ignores it.
    FramingPolicy framing { FramingPolicy::AUTO };

    /// TCP and UDS: the largest message in bytes, 0 for the framing's own limit. For LENGTH_PREFIX
    /// it is the largest payload, 64 MiB by default, and a larger one closes the connection on
    /// receive and is refused on send. For LINE it is the longest line, 1 MiB by default, and a
    /// longer one is dropped on receive. NONE ignores it.
    size_t max_message_size {};
};

/**
 * @struct NetworkMessage
 * @brief A received datagram or framed message with sender metadata
 *
 * Passed to receive callbacks. Also the natural return type for a
 * future NetworkAwaiter (Kriya::NetworkReceive) if condition-driven
 * coroutine support is added later, following the EventAwaiter model.
 */
struct NetworkMessage {
    uint64_t endpoint_id {};
    std::vector<uint8_t> data;
    std::string sender_address;
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-backend configuration structs
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct UDPBackendInfo
 * @brief Configuration for the UDP transport backend
 *
 * UDP handles connectionless datagrams. OSC, simple parameter
 * broadcast, and low-latency control messages ride on top of this.
 */
struct MAYAFLUX_API UDPBackendInfo {
    bool enabled { false };
    uint16_t default_receive_port { 8000 };
    uint16_t default_send_port { 9000 };
    std::string default_send_address { "127.0.0.1" };
    size_t receive_buffer_size { 65536 };
    bool enable_broadcast { false };
    bool enable_multicast { false };
    std::string multicast_group;

};

MF_DESCRIBE(UDPBackendInfo)

/**
 * @struct TCPBackendInfo
 * @brief Configuration for the TCP transport backend
 *
 * TCP handles reliable, ordered byte streams. Bulk audio/video/tensor
 * data, model weight transfer, and any payload where completeness
 * matters more than latency.
 */
struct MAYAFLUX_API TCPBackendInfo {
    bool enabled { false };
    uint16_t listen_port { 0 };
    size_t receive_buffer_size { 1048576 };
    bool auto_reconnect { true };
    uint32_t reconnect_interval_ms { 2000 };
    uint32_t connect_timeout_ms { 5000 };

};

MF_DESCRIBE(TCPBackendInfo)

/**
 * @struct UDSBackendInfo
 * @brief Configuration for the Unix domain socket transport backend
 *
 * Reliable, ordered, length-prefixed messages between processes on the same
 * machine, addressed by a socket path instead of an IP address and port. A
 * listener is owner-only on POSIX. Windows relies on the socket directory's ACL.
 */
struct MAYAFLUX_API UDSBackendInfo {
    bool enabled { false };
    bool auto_reconnect { true };
    uint32_t reconnect_interval_ms { 2000 };
};

MF_DESCRIBE(UDSBackendInfo)

/**
 * @struct SharedMemoryBackendInfo
 * @brief Configuration for local inter-process shared memory transport
 *
 * Zero-copy data exchange between processes on the same machine.
 * Useful for multi-instance MayaFlux, bridging to Python inference
 * servers without serialization overhead, or DAW plugin hosting.
 */
struct MAYAFLUX_API SharedMemoryBackendInfo {
    bool enabled { false };
    std::string segment_name { "mayaflux_shm" };
    size_t segment_size { 16 * 1024 * 1024 };

};

MF_DESCRIBE(SharedMemoryBackendInfo)

// ─────────────────────────────────────────────────────────────────────────────
// Global configuration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct GlobalNetworkConfig
 * @brief Configuration for the NetworkSubsystem
 *
 * Passed to NetworkSubsystem during construction. Mirrors the pattern
 * established by GlobalInputConfig for InputSubsystem.
 *
 * @code
 * GlobalNetworkConfig net_config;
 * net_config.udp.enabled = true;
 * net_config.udp.default_receive_port = 7000;
 * net_config.tcp.enabled = true;
 * auto net_subsystem = std::make_unique<NetworkSubsystem>(net_config);
 * @endcode
 */
struct MAYAFLUX_API GlobalNetworkConfig {
    UDPBackendInfo udp;
    TCPBackendInfo tcp;
    SharedMemoryBackendInfo shared_memory;
    UDSBackendInfo uds;

    static GlobalNetworkConfig with_uds()
    {
        GlobalNetworkConfig config;
        config.uds.enabled = true;
        return config;
    }

    static GlobalNetworkConfig with_udp(uint16_t recv_port = 8000, uint16_t send_port = 9000)
    {
        GlobalNetworkConfig config;
        config.udp.enabled = true;
        config.udp.default_receive_port = recv_port;
        config.udp.default_send_port = send_port;
        return config;
    }

    static GlobalNetworkConfig with_tcp(uint16_t listen_port = 0)
    {
        GlobalNetworkConfig config;
        config.tcp.enabled = true;
        config.tcp.listen_port = listen_port;
        return config;
    }

    static GlobalNetworkConfig with_osc(uint16_t recv_port = 8000, uint16_t send_port = 9000)
    {
        return with_udp(recv_port, send_port);
    }

    [[nodiscard]] bool any_enabled() const
    {
        return udp.enabled || tcp.enabled || shared_memory.enabled || uds.enabled;
    }

};

MF_DESCRIBE(GlobalNetworkConfig)

} // namespace MayaFlux::Core
#include "GlobalNetworkConfig.describe.hpp"
