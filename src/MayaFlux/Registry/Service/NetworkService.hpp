#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Registry::Service {

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
     * For serial: the port opens synchronously and 0 means it failed, unless the
     * backend reconnects, where a port that is not there yet returns an id and
     * the endpoint enters RECONNECTING. The settle time then runs with the
     * endpoint in OPENING.
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

    /**
     * @brief Register a per-endpoint state callback
     * @param endpoint_id Endpoint to observe.
     * @param callback    Called with the endpoint id, the state it left and the state it entered.
     *
     * Fires on the io thread at every transition, including RECONNECTING and back to OPEN.
     * A connection a listener accepted reports through the listener's callback with its own
     * id, as CLOSED to OPEN, unless it has a callback of its own. Overrides any previous
     * callback for this endpoint. Transitions before registration are not replayed, so read
     * get_endpoint_state after registering. Like the receive callback, it may send, close
     * or open endpoints from inside itself.
     */
    std::function<void(uint64_t endpoint_id,
        std::function<void(uint64_t, Core::EndpointState, Core::EndpointState)> callback)>
        set_endpoint_state_callback;

    // ─────────────────────────────────────────────────────────────────────────
    // Write backlog
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Bytes accepted by send and not yet written to the peer
     * @return 0 for an unknown endpoint or a transport that does not queue.
     *
     * A control rate writer can skip a write while this is high, and a stream
     * producer reads backpressure from it.
     */
    std::function<size_t(uint64_t endpoint_id)> get_write_backlog;

    // ─────────────────────────────────────────────────────────────────────────
    // Serial line control
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Set the DTR and RTS lines of an open serial endpoint
     * @param endpoint_id Serial endpoint.
     * @param dtr         New DTR level, or nothing to leave it.
     * @param rts         New RTS level, or nothing to leave it.
     * @return false for an endpoint that is not an open serial port.
     */
    std::function<bool(uint64_t endpoint_id, std::optional<bool> dtr, std::optional<bool> rts)>
        set_control_lines;

    /**
     * @brief Hold the transmit line of an open serial endpoint in break
     * @return false for an endpoint that is not an open serial port.
     */
    std::function<bool(uint64_t endpoint_id)> send_break;

    // ─────────────────────────────────────────────────────────────────────────
    // Timer
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Run a callable once after a delay, on the thread that runs the endpoint callbacks
     * @param delay    Time from now.
     * @param callback Called once, never concurrently with a receive or state callback.
     * @return A token for cancel_post, never 0.
     *
     * Serves idle-gap frame ends and response timeouts, which must not race a chunk.
     */
    std::function<uint64_t(std::chrono::milliseconds delay, std::function<void()> callback)> post_after;

    /**
     * @brief Cancel a timer from post_after
     * @param token The token post_after returned. No-op when the timer already ran.
     *
     * Once this returns the callable no longer runs, unless it has already started.
     */
    std::function<void(uint64_t token)> cancel_post;
};

} // namespace MayaFlux::Registry::Service
