#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Memory.hpp"

namespace MayaFlux::Nodes::Input {
class InputNode;
}

namespace MayaFlux::Registry::Service {
struct InputService;
}

namespace MayaFlux::Core {

/**
 * @class InputManager
 * @brief Manages input processing thread and node dispatch
 *
 * InputManager is the core processing entity for input. It:
 * - Owns the input processing thread
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
     * Thread-safe. Can be called while processing is active.
     */
    void register_node(
        const std::shared_ptr<Nodes::Input::InputNode>& node,
        InputBinding binding);

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
    // Input Queue (lock-free)
    // ─────────────────────────────────────────────────────────────────────

    std::atomic<bool> m_queue_notify { false };
    static constexpr size_t MAX_QUEUE_SIZE = 4096;
    Memory::LockFreeRingBuffer<InputValue, MAX_QUEUE_SIZE> m_queue;

    // ─────────────────────────────────────────────────────────────────────
    // Node Registry
    // ─────────────────────────────────────────────────────────────────────

    struct NodeRegistration {
        std::weak_ptr<Nodes::Input::InputNode> node;
        InputBinding binding;
    };
    using RegistrationList = std::vector<NodeRegistration>;

    std::vector<std::shared_ptr<Nodes::Input::InputNode>> m_tracked_nodes; ///< To keep nodes alive

    std::mutex m_registry_mutex;

#ifdef MAYAFLUX_PLATFORM_MACOS
    // Apple's broken LLVM doesn't support std::atomic<std::shared_ptr<T>>
    std::atomic<const RegistrationList*> m_registrations { nullptr };

    // Hazard pointers for safe lock-free reads on macOS
    static constexpr size_t MAX_READERS = 16;
    mutable std::array<std::atomic<const RegistrationList*>, MAX_READERS> m_hazard_ptrs;
    mutable std::atomic<size_t> m_hazard_counter { 0 };

    void retire_list(const RegistrationList* list);
#else
    // Proper C++20 atomic shared_ptr on real toolchains
    std::atomic<std::shared_ptr<const RegistrationList>> m_registrations;
#endif

    Registry::Service::InputService* m_input_service { nullptr };

    // ─────────────────────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────────────────────

    std::atomic<uint64_t> m_events_processed { 0 };
};

} // namespace MayaFlux::Core
