#pragma once

namespace MayaFlux::Portal::Network {

//=============================================================================
// Transport identity
//=============================================================================

/**
 * @enum NetworkTransportHint
 * @brief Portal-level transport hint, independent of Core::NetworkTransport.
 *
 * Consumers express intent here. NetworkFoundry resolves the hint to an
 * actual backend at endpoint-open time, allowing future transports (WebRTC,
 * QUIC, GStreamer) to slot in without any Portal API change.
 */
enum class NetworkTransportHint : uint8_t {
    UDP, ///< Low-latency, unordered datagrams. OSC, control, sparse streams.
    TCP, ///< Reliable ordered byte stream. Bulk data, session-oriented.
    SHARED_MEMORY, ///< Zero-copy IPC on the same machine.
    AUTO ///< Let NetworkFoundry pick based on StreamProfile.
};

//=============================================================================
// Stream characteristics
//=============================================================================

/**
 * @enum StreamProfile
 * @brief Data characteristics that drive transport and framing selection.
 *
 * Domain-agnostic. The same profile applies whether the payload is
 * a float buffer, a matrix, a point cloud, or a raw byte sequence.
 * NetworkFoundry uses this when TransportHint::AUTO is specified and
 * StreamForge uses it to select wire framing.
 *
 * The axes are: ordering requirement × loss tolerance × payload size.
 */
enum class StreamProfile : uint8_t {
    REALTIME_SMALL, ///< Small payloads, loss-tolerant, latency-sensitive.
                    ///< UDP default. Single datagram fits MTU.
                    ///< Example: sparse parameter updates, OSC-style events.

    REALTIME_BULK, ///< Large payloads, loss-tolerant, latency-sensitive.
                   ///< UDP with chunking. Dropped chunk = dropped frame, acceptable.
                   ///< Example: per-cycle matrix snapshots, live geometry streams.

    ORDERED_BULK, ///< Large payloads, loss-intolerant, ordering required.
                  ///< TCP or SHM. Completeness matters more than latency.
                  ///< Example: tensor transfers, model weights, file-like payloads.

    ARBITRARY ///< Raw bytes. Caller owns framing entirely.
              ///< No policy applied beyond opening the endpoint.
};

/**
 * @enum FramingPolicy
 * @brief How outbound payloads are framed on the wire.
 *
 * NONE: raw bytes, no framing (UDP default).
 * LENGTH_PREFIX: 4-byte big-endian size header before each message (TCP default).
 * CHUNKED: split large payloads into MTU-sized chunks with sequence numbers.
 */
enum class FramingPolicy : uint8_t {
    NONE,
    LENGTH_PREFIX,
    CHUNKED
};

//=============================================================================
// Stream handle type
//=============================================================================

/**
 * @typedef StreamID
 * @brief Opaque handle returned by NetworkFoundry when a stream is opened.
 *
 * Analogous to ShaderID / ComputePipelineID in Portal::Graphics.
 * Zero is always invalid.
 */
using StreamID = uint64_t;

constexpr StreamID INVALID_STREAM = 0;

//=============================================================================
// Endpoint description
//=============================================================================

/**
 * @struct StreamEndpoint
 * @brief Describes the remote (or local) side of a network stream.
 *
 * Analogous to Core::EndpointInfo but expressed in Portal vocabulary.
 * NetworkFoundry translates this to Core::EndpointInfo when opening
 * the underlying transport endpoint.
 */
struct MAYAFLUX_API StreamEndpoint {
    std::string address; ///< Remote address. Empty = listen only.
    uint16_t port { 0 }; ///< Remote port (send) or local port (receive).
    uint16_t local_port { 0 }; ///< Explicit local bind port. 0 = OS-assigned.
};

//=============================================================================
// Stream configuration
//=============================================================================

/**
 * @struct StreamConfig
 * @brief Full configuration for an outbound or bidirectional stream.
 *
 * Passed to NetworkFoundry::open_stream(). Mirrors the pattern of
 * ShaderConfig / RenderPipelineConfig in Portal::Graphics.
 */
struct MAYAFLUX_API StreamConfig {
    std::string name;
    StreamEndpoint endpoint;
    StreamProfile profile { StreamProfile::ARBITRARY };
    NetworkTransportHint transport { NetworkTransportHint::AUTO };
    FramingPolicy framing { FramingPolicy::NONE };
};

//=============================================================================
// Payload helpers
//=============================================================================

/**
 * @brief Convenience alias for a read-only byte view.
 *
 * All Portal::Network send functions accept this. Callers may pass
 * std::vector<uint8_t>, std::array, raw pointer+size via std::span,
 * or any contiguous range.
 */
using ByteView = std::span<const uint8_t>;

} // namespace MayaFlux::Portal::Network
