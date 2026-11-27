#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"
#include "MayaFlux/Transitive/Memory/PendingQueue.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
#include "MayaFlux/Transitive/Parallel/Notifier.hpp"

namespace MayaFlux::Nodes::Input {
class InputNode;
}

namespace MayaFlux::Registry::Service {
struct InputService;
struct NetworkService;
}

namespace MayaFlux::Core {

/**
 * @class InputManager
 * @brief Manages input processing thread and node dispatch
 *
 * InputManager is the core processing entity for input. It:
 * - Owns the input processing thread
 * - Holds its routing table as an immutable snapshot: registration replaces it, dispatch reads it without locking
 * - Maintains device→node routing table
 * - Receives InputValues from backends via thread-safe queue
 * - Dispatches input to registered nodes by calling process_input()
 *
 * Threading model:
 * - Backends push to queue from their threads (thread-safe)
 * - Single processing thread dispatches to nodes
 * - Node callbacks fire on the processing thread
 *
 * Owned by InputSubsystem, which handles lifecycle coordination.
 */
class MAYAFLUX_API InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable, non-movable
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Start the processing thread
     */
    void start();

    /**
     * @brief Stop the processing thread
     *
     * Waits for thread to finish processing current queue before returning.
     */
    void stop();

    /**
     * @brief Check if processing thread is running
     */
    [[nodiscard]] bool is_running() const { return m_running.load(); }

    // ─────────────────────────────────────────────────────────────────────
    // Input Enqueueing (called by backends)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Enqueue an input value for processing
     * @param value The input value from a backend
     *
     * Thread-safe. Called from backend threads.
     * Wakes processing thread if sleeping.
     */
    void enqueue(const InputValue& value);

    /**
     * @brief Enqueue multiple input values
     * @param values Vector of input values
     */
    void enqueue_batch(const std::vector<InputValue>& values);

    // ─────────────────────────────────────────────────────────────────────
    // Node Registration
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Register a node to receive input
     * @param node The InputNode to register
     * @param binding Specifies what input the node wants
     *
     * A binding by vendor and product with no matching device is kept, and
     * takes effect when one appears. Thread-safe. Can be called while
     * processing is active.
     */
    void register_node(
        const std::shared_ptr<Nodes::Input::InputNode>& node,
        InputBinding binding);

    /**
     * @brief Follow a device that appears or disappears
     * @param info The device
     * @param connected True when it appeared, false when it went away
     *
     * A node registered by vendor and product keeps that pattern. When its
     * device goes away it receives nothing, and when a matching device appears
     * it is bound to that device's id and the device is opened, so a device that
     * returns under a new id is followed. A node registered by id is untouched.
     * Thread-safe. Called from backend threads after the backend released its lock.
     */
    void on_device_change(const InputDeviceInfo& info, bool connected);

    /**
     * @brief Unregister a node
     * @param node The node to unregister
     *
     * Removes the node from all bindings.
     */
    void unregister_node(const std::shared_ptr<Nodes::Input::InputNode>& node);

    /**
     * @brief Unregister all nodes
     */
    void unregister_all_nodes();

    /**
     * @brief Get count of registered nodes
     */
    [[nodiscard]] size_t get_registered_node_count() const;

    /**
     * @brief Returns a snapshot of all currently tracked input nodes.
     * @return Vector of active InputNode instances.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Nodes::Input::InputNode>> get_nodes() const;

    // ─────────────────────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get number of events processed since start
     */
    [[nodiscard]] uint64_t get_events_processed() const
    {
        return m_events_processed.load();
    }

    /**
     * @brief Get current queue depth
     */
    [[nodiscard]] size_t get_queue_depth() const;

    /**
     * @brief Setup OSC bridge if enabled in config
     */
    void setup_osc_bridge(const OSCConfigInfo& osc_config);

private:
    // ─────────────────────────────────────────────────────────────────────
    // Processing Thread
    // ─────────────────────────────────────────────────────────────────────

    void processing_loop();
    void dispatch_to_nodes(const InputValue& value);
    bool matches_binding(const InputValue& value, const InputBinding& binding) const;
    std::optional<InputBinding> resolve_vid_pid(const InputBinding& binding, const std::vector<InputDeviceInfo>& devices) const;

    std::thread m_processing_thread;
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_stop_requested { false };

    // ─────────────────────────────────────────────────────────────────────
    // Input Queue (lock-free, any number of producers)
    // ─────────────────────────────────────────────────────────────────────

    static constexpr size_t MAX_QUEUE_SIZE = 4096;
    Memory::PendingQueue<InputValue, MAX_QUEUE_SIZE> m_queue;
    std::atomic<size_t> m_queue_depth { 0 };
    Parallel::Notifier m_wake;

    bool push_value(InputValue value);

    // ─────────────────────────────────────────────────────────────────────
    // Node Registry
    // ─────────────────────────────────────────────────────────────────────

    static constexpr uint32_t k_unbound_device = std::numeric_limits<uint32_t>::max();

    struct NodeRegistration {
        std::shared_ptr<Nodes::Input::InputNode> node;
        InputBinding binding;
        std::optional<InputBinding> pattern;
    };
    using RegistrationList = std::vector<NodeRegistration>;

    Memory::SnapshotCell<RegistrationList> m_registrations;

    Registry::Service::InputService* m_input_service { nullptr };
    Registry::Service::NetworkService* m_network_service { nullptr };

    void teardown_osc_bridge();

    uint64_t m_osc_endpoint_id { 0 };

    // ─────────────────────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────────────────────

    std::atomic<uint64_t> m_events_processed { 0 };
};

} // namespace MayaFlux::Core
