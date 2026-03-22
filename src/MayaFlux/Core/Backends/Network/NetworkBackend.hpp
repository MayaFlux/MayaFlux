#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Core {

/**
 * @brief Callback signature for inbound data on an endpoint
 *
 * @param endpoint_id  The endpoint that received the data.
 * @param data         Pointer to received bytes. Valid only for the
 *                     duration of the callback.
 * @param size         Number of bytes received.
 * @param sender_addr  Sender address string (IP:port for UDP, peer
 *                     address for TCP, segment name for SHM).
 */
using NetworkReceiveCallback = std::function<void(
    uint64_t endpoint_id,
    const uint8_t* data,
    size_t size,
    std::string_view sender_addr)>;

/**
 * @brief Callback signature for endpoint state changes
 */
using EndpointStateCallback = std::function<void(
    const EndpointInfo& info,
    EndpointState previous,
    EndpointState current)>;

/**
 * @class INetworkBackend
 * @brief Abstract interface for network transport backends
 *
 * Follows the same lifecycle and structural pattern as IInputBackend:
 *   initialize() -> start() -> [running] -> stop() -> shutdown()
 *
 * Each concrete implementation (UDP, TCP, SharedMemory) owns sockets
 * or shared segments, manages background receive threads, and exposes
 * a uniform endpoint model. The NetworkSubsystem owns backends and
 * routes high-level operations through this interface.
 *
 * Bidirectional: sends and receives, manages persistent endpoints
 * rather than devices. This is the core structural difference from
 * IInputBackend.
 *
 * Backends are deliberately synchronous and callback-based. They know
 * nothing about coroutines. The coroutine surface lives in NetworkService
 * and is implemented by NetworkSubsystem using callbacks internally to
 * resume suspended coroutines. This keeps backends simple (pure OS socket
 * wrappers) and avoids coupling transport code to the Vruta/Kriya stack.
 *
 * Endpoint lifecycle:
 *   open_endpoint(info) -> endpoint_id
 *   send(endpoint_id, data, size)
 *   [receive via callback]
 *   close_endpoint(endpoint_id)
 *
 * Future transport backends (WebRTC, GStreamer, etc.) implement this same
 * interface. Protocol-level orchestration above raw transport (codec
 * pipelines, session management, jitter buffering) belongs in a future
 * Portal::Network layer, not here.
 */
class MAYAFLUX_API INetworkBackend {
public:
    virtual ~INetworkBackend() = default;

    // ─────────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialise backend resources (sockets, SHM segments, etc.)
     * @return true if initialisation succeeded
     *
     * Allocates platform resources but does not start receive threads.
     */
    virtual bool initialize() = 0;

    /**
     * @brief Start receive threads and accept connections
     *
     * After this call the backend actively delivers received data via
     * the registered callback and accepts new inbound connections (TCP).
     */
    virtual void start() = 0;

    /**
     * @brief Stop receive threads without releasing resources
     *
     * Endpoints remain valid. Pending sends complete. Receive callbacks
     * cease until start() is called again.
     */
    virtual void stop() = 0;

    /**
     * @brief Release all resources, close all endpoints
     *
     * After this call initialize() must be called again to reuse the backend.
     */
    virtual void shutdown() = 0;

    [[nodiscard]] virtual bool is_initialized() const = 0;
    [[nodiscard]] virtual bool is_running() const = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // Transport identity
    // ─────────────────────────────────────────────────────────────────────────

    [[nodiscard]] virtual NetworkTransport get_transport() const = 0;
    [[nodiscard]] virtual std::string get_name() const = 0;
    [[nodiscard]] virtual std::string get_version() const = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // Endpoint management
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Open a new endpoint
     * @param info Endpoint configuration with id pre-assigned by subsystem.
     * @return The endpoint id on success, or 0 on failure.
     *
     * The subsystem assigns the id before calling this. The backend stores
     * and uses it for subsequent operations.
     *
     * For UDP: binds a local port and/or stores a default remote target.
     * For TCP: connects to a remote host, or begins listening on a port.
     *          Outbound connect may block. Use co_open_endpoint on the
     *          service for non-blocking connect.
     * For SHM: opens or creates a named shared memory segment.
     */
    virtual uint64_t open_endpoint(const EndpointInfo& info) = 0;

    /**
     * @brief Close an endpoint and release its resources
     * @param endpoint_id Endpoint to close. No-op if already closed.
     */
    virtual void close_endpoint(uint64_t endpoint_id) = 0;

    /**
     * @brief Query the current state of an endpoint
     */
    [[nodiscard]] virtual EndpointState get_endpoint_state(uint64_t endpoint_id) const = 0;

    /**
     * @brief List all endpoints currently managed by this backend
     */
    [[nodiscard]] virtual std::vector<EndpointInfo> get_endpoints() const = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // Data transfer
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Send data through an endpoint
     * @param endpoint_id Target endpoint.
     * @param data        Pointer to payload bytes.
     * @param size        Payload size in bytes.
     * @return true if the send was accepted (queued or completed).
     *
     * For UDP: sends a single datagram via sendto(). Non-blocking.
     * For TCP: writes framed message. May block briefly if kernel
     *          send buffer is full.
     * For SHM: writes into the shared segment.
     */
    virtual bool send(uint64_t endpoint_id, const uint8_t* data, size_t size) = 0;

    /**
     * @brief Send data to a specific address through an endpoint
     * @param endpoint_id Source endpoint (must be open).
     * @param data        Pointer to payload bytes.
     * @param size        Payload size in bytes.
     * @param address     Target address string.
     * @param port        Target port.
     * @return true if the send was accepted.
     *
     * Primary use: UDP (send to arbitrary peer via bound socket).
     * TCP: ignored, uses connected peer.
     * SHM: ignored.
     */
    virtual bool send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
        const std::string& address, uint16_t port) = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // Callbacks
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Register the receive callback
     *
     * Called from the backend's receive thread. The subsystem wires this
     * to route data to per-endpoint callbacks and to resume suspended
     * coroutines. Backends fire this for every received datagram (UDP)
     * or framed message (TCP).
     */
    virtual void set_receive_callback(NetworkReceiveCallback callback) = 0;

    /**
     * @brief Register the endpoint state change callback
     *
     * Fired when an endpoint transitions state (OPENING -> OPEN,
     * OPEN -> ERROR, ERROR -> RECONNECTING, etc.). The subsystem uses
     * this to resume co_open_endpoint awaitables and to notify consumers.
     */
    virtual void set_state_callback(EndpointStateCallback callback) = 0;
};

} // namespace MayaFlux::Core
