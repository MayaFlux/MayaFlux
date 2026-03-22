#pragma once

namespace MayaFlux::Core {

/**
 * @enum NetworkTransport
 * @brief Identifies the transport protocol a backend implements
 */
enum class NetworkTransport : uint8_t {
    UDP,
    TCP,
    SHARED_MEMORY
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
        return udp.enabled || tcp.enabled || shared_memory.enabled;
    }
};

} // namespace MayaFlux::Core
