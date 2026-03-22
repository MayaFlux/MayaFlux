#pragma once

#include "Subsystem.hpp"

#include "MayaFlux/Core/Backends/Network/NetworkBackend.hpp"

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>

namespace MayaFlux::Registry::Service {
struct NetworkService;
}

namespace MayaFlux::Core {

/**
 * @class NetworkSubsystem
 * @brief General-purpose network transport subsystem
 *
 * Owns multiple INetworkBackend instances (UDP, TCP, SharedMemory) and
 * registers a NetworkService into BackendRegistry for decoupled access
 * by any engine component.
 *
 * Structural parallel to InputSubsystem:
 *   - InputSubsystem owns IInputBackend instances (HID, MIDI).
 *   - NetworkSubsystem owns INetworkBackend instances (UDP, TCP, SHM).
 *   - InputSubsystem registers InputService.
 *   - NetworkSubsystem registers NetworkService.
 *
 * Key difference: InputSubsystem is receive-only (InputCallback delivers
 * events to InputManager). NetworkSubsystem is bidirectional. It provides
 * send and receive through NetworkService, and manages persistent
 * endpoints rather than device handles.
 *
 * Relationship to InputSubsystem's OSC/Serial stubs:
 *   InputSubsystem::initialize_osc_backend() will create an OSCBackend
 *   (IInputBackend) that internally obtains a UDP endpoint through
 *   NetworkService. The OSCBackend handles OSC address parsing and
 *   produces InputValue events. The actual socket is managed here.
 *   Same pattern for Serial if it ever rides over TCP or SHM.
 *
 * Endpoint IDs are globally unique across all backends. The subsystem
 * maintains a transport-to-backend routing map so callers never need
 * to know which backend handles their endpoint.
 *
 * Tokens:
 *   Buffer  = NETWORK_BACKEND (new ProcessingToken)
 *   Node    = EVENT_RATE (shared with InputSubsystem, async arrival)
 *   Task    = EVENT_DRIVEN (shared with InputSubsystem)
 *
 * @code
 * GlobalNetworkConfig config;
 * config.udp.enabled = true;
 * config.tcp.enabled = true;
 *
 * auto subsystem = std::make_unique<NetworkSubsystem>(config);
 * subsystem_manager->add_subsystem(SubsystemType::NETWORK, subsystem);
 * @endcode
 */
class MAYAFLUX_API NetworkSubsystem : public ISubsystem {
public:
    explicit NetworkSubsystem(const GlobalNetworkConfig& config);
    ~NetworkSubsystem() override;

    NetworkSubsystem(const NetworkSubsystem&) = delete;
    NetworkSubsystem& operator=(const NetworkSubsystem&) = delete;
    NetworkSubsystem(NetworkSubsystem&&) = delete;
    NetworkSubsystem& operator=(NetworkSubsystem&&) = delete;

    // ─────────────────────────────────────────────────────────────────────────
    // ISubsystem implementation
    // ─────────────────────────────────────────────────────────────────────────

    void register_callbacks() override;
    void initialize(SubsystemProcessingHandle& handle) override;
    void start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] SubsystemTokens get_tokens() const override { return m_tokens; }
    [[nodiscard]] bool is_ready() const override { return m_ready.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }
    [[nodiscard]] SubsystemType get_type() const override { return SubsystemType::NETWORK; }
    SubsystemProcessingHandle* get_processing_context_handle() override { return m_handle; }

    // ─────────────────────────────────────────────────────────────────────────
    // Backend management (mirrors InputSubsystem::add_backend pattern)
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Add a network backend
     * @param backend Backend instance (takes ownership)
     * @return true if added successfully
     *
     * Only one backend per NetworkTransport type is permitted.
     */
    bool add_backend(std::unique_ptr<INetworkBackend> backend);

    /**
     * @brief Get a backend by transport type
     */
    [[nodiscard]] INetworkBackend* get_backend(NetworkTransport transport) const;

    /**
     * @brief Get all active backends
     */
    [[nodiscard]] std::vector<INetworkBackend*> get_backends() const;

    // ─────────────────────────────────────────────────────────────────────────
    // Endpoint management (routed through to correct backend)
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Open an endpoint on the backend matching info.transport
     * @param info Endpoint configuration.
     * @return Globally unique endpoint id, or 0 on failure.
     */
    uint64_t open_endpoint(const EndpointInfo& info);

    /**
     * @brief Close an endpoint
     */
    void close_endpoint(uint64_t endpoint_id);

    /**
     * @brief Send data through an endpoint
     */
    bool send(uint64_t endpoint_id, const uint8_t* data, size_t size);

    /**
     * @brief Send data to a specific address
     */
    bool send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
        const std::string& address, uint16_t port);

    /**
     * @brief Query endpoint state
     */
    [[nodiscard]] EndpointState get_endpoint_state(uint64_t endpoint_id) const;

    /**
     * @brief Register a per-endpoint receive callback
     *
     * Stored locally and invoked from the backend's receive callback
     * after endpoint routing.
     */
    void set_endpoint_receive_callback(uint64_t endpoint_id,
        NetworkReceiveCallback callback);

    /**
     * @brief List all open endpoints across all backends
     */
    [[nodiscard]] std::vector<EndpointInfo> get_all_endpoints() const;

private:
    GlobalNetworkConfig m_config;
    SubsystemProcessingHandle* m_handle { nullptr };
    SubsystemTokens m_tokens;

    std::atomic<bool> m_ready { false };
    std::atomic<bool> m_running { false };

    // ─── Asio event loop ────────────────────────────────────────────────────
    //
    // Single io_context shared by all backends. One jthread runs
    // io_context::run(). All async operations (send, receive, accept,
    // connect, reconnect timers) complete on this thread. No per-socket
    // threads, no per-connection threads, no mutexes on the data path.
    //
    // work_guard prevents io_context::run() from returning when there
    // are no pending operations (idle between endpoint opens).
    // ────────────────────────────────────────────────────────────────────────

    std::unique_ptr<asio::io_context> m_io_context;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> m_work_guard;

#if MAYAFLUX_USE_JTHREAD
    std::jthread m_io_thread;
#else
    std::thread m_io_thread;
    std::atomic<bool> m_io_stop_requested { false };
#endif

    mutable std::shared_mutex m_backends_mutex;
    std::unordered_map<NetworkTransport, std::unique_ptr<INetworkBackend>> m_backends;

    std::shared_ptr<Registry::Service::NetworkService> m_network_service;

    /**
     * @brief Maps endpoint_id -> transport for routing send/close/query
     *        to the correct backend without scanning all backends.
     */
    mutable std::shared_mutex m_routing_mutex;
    std::unordered_map<uint64_t, NetworkTransport> m_endpoint_routing;

    /**
     * @brief Per-endpoint receive callbacks set by consumers via
     *        set_endpoint_receive_callback(). Backend-level receive
     *        callbacks route here.
     */
    mutable std::shared_mutex m_callbacks_mutex;
    std::unordered_map<uint64_t, NetworkReceiveCallback> m_endpoint_callbacks;

    /**
     * @brief Global endpoint id counter shared across all backends.
     *        Backends do not assign their own ids. The subsystem assigns
     *        and maps them.
     */
    std::atomic<uint64_t> m_next_endpoint_id { 1 };

    void initialize_udp_backend();
    void initialize_tcp_backend();
    void initialize_shm_backend();

    void register_backend_service();

    /**
     * @brief Wired as the receive callback on each backend. Routes
     *        to per-endpoint callbacks in m_endpoint_callbacks.
     */
    void on_backend_receive(uint64_t endpoint_id, const uint8_t* data,
        size_t size, std::string_view sender_addr);

    /**
     * @brief Wired as the state callback on each backend. Forwards
     *        to any registered state observers.
     */
    void on_backend_state_change(const EndpointInfo& info,
        EndpointState previous, EndpointState current);

    /**
     * @brief Resolve endpoint_id to its owning backend. Returns nullptr
     *        if the id is unknown.
     */
    INetworkBackend* resolve_backend(uint64_t endpoint_id) const;
};

} // namespace MayaFlux::Core
