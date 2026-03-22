#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Registry::Service {

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

/**
 * @brief Backend network transport service interface
 *
 * Registered into BackendRegistry by NetworkSubsystem. Follows the same
 * service pattern as BufferService, ComputeService, DisplayService, and
 * InputService. Enables any component (nodes, buffer processors,
 * InputManager) to send and receive data over the network without
 * coupling to NetworkSubsystem internals.
 *
 * Consumption model: callbacks. The receive callback fires on the
 * backend's receive thread. Consumers that need to integrate with
 * coroutine-based workflows can bridge via Kriya::ProcessingGate
 * (poll a flag the callback sets) or, when implemented, via a
 * dedicated Kriya::NetworkReceive awaiter modelled on EventAwaiter.
 *
 * The coroutine surface is deliberately absent from this initial
 * version. The pattern exists (EventAwaiter), the integration point
 * is clear (set_endpoint_receive_callback resumes a suspended handle),
 * but the awaiter should be added when a concrete use case demands it,
 * not speculatively.
 *
 * @code
 * auto* svc = BackendRegistry::instance().get_service<NetworkService>();
 * if (!svc) return;
 *
 * auto ep = svc->open_endpoint({
 *     .transport = NetworkTransport::UDP,
 *     .role = EndpointRole::BIDIRECTIONAL,
 *     .local_port = 8000,
 *     .remote_address = "127.0.0.1",
 *     .remote_port = 9000
 * });
 *
 * svc->set_endpoint_receive_callback(ep,
 *     [](uint64_t id, const uint8_t* d, size_t s, std::string_view addr) {
 *         // fires on backend receive thread
 *     });
 *
 * svc->send(ep, payload.data(), payload.size());
 * svc->close_endpoint(ep);
 * @endcode
 */
struct MAYAFLUX_API NetworkService {

    // ─────────────────────────────────────────────────────────────────────────
    // Endpoint lifecycle
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Open a network endpoint on the appropriate backend
     * @param info Endpoint configuration. transport field selects the backend.
     * @return Globally unique endpoint id, or 0 on failure.
     *
     * For UDP: non-blocking (bind is instant).
     * For TCP with EndpointRole::RECEIVE (listener): non-blocking.
     * For TCP with outbound connection: blocks until connected or timeout.
     */
    std::function<uint64_t(const Core::EndpointInfo& info)> open_endpoint;

    /**
     * @brief Close an endpoint
     * @param endpoint_id Endpoint to close.
     */
    std::function<void(uint64_t endpoint_id)> close_endpoint;

    /**
     * @brief Query endpoint state
     * @param endpoint_id Endpoint to query.
     * @return Current state, or CLOSED if unknown.
     */
    std::function<Core::EndpointState(uint64_t endpoint_id)> get_endpoint_state;

    /**
     * @brief List all open endpoints across all backends
     */
    std::function<std::vector<Core::EndpointInfo>()> get_all_endpoints;

    // ─────────────────────────────────────────────────────────────────────────
    // Data transfer
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Send data through an endpoint
     * @param endpoint_id Target endpoint.
     * @param data        Payload bytes.
     * @param size        Payload size.
     * @return true if the send was accepted.
     *
     * For UDP: non-blocking sendto().
     * For TCP: may block briefly if kernel send buffer is full.
     */
    std::function<bool(uint64_t endpoint_id, const uint8_t* data, size_t size)> send;

    /**
     * @brief Send data to a specific address through an endpoint
     * @param endpoint_id Source endpoint.
     * @param data        Payload bytes.
     * @param size        Payload size.
     * @param address     Target address.
     * @param port        Target port.
     * @return true if the send was accepted.
     *
     * Primary use: UDP. TCP ignores address/port, uses connected peer.
     */
    std::function<bool(uint64_t endpoint_id, const uint8_t* data, size_t size,
        const std::string& address, uint16_t port)>
        send_to;

    // ─────────────────────────────────────────────────────────────────────────
    // Receive callbacks
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Register a per-endpoint receive callback
     * @param endpoint_id Endpoint to listen on.
     * @param callback    Called on the backend's receive thread.
     *
     * Overrides any previously registered callback for this endpoint.
     * For endpoints opened with EndpointRole::SEND this is a no-op.
     *
     * Thread safety: the callback fires on the backend's receive thread.
     * If the consumer needs to bridge to a different thread (audio callback,
     * coroutine scheduler), it must handle that in the callback body
     * (e.g., push into a lock-free queue, set an atomic flag).
     */
    std::function<void(uint64_t endpoint_id,
        std::function<void(uint64_t, const uint8_t*, size_t, std::string_view)> callback)>
        set_endpoint_receive_callback;
};

} // namespace MayaFlux::Registry::Service
