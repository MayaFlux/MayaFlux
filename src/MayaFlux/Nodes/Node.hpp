#pragma once

#include "GpuContext.hpp"
#include "NodeOperators.hpp"

/**
 * @namespace MayaFlux::Nodes
 * @brief Contains the node-based computational processing system components
 *
 * The Nodes namespace provides a flexible, composable processing architecture
 * based on the concept of interconnected computational nodes. Each node represents a
 * discrete transformation unit that can be connected to other nodes to form
 * complex processing networks and computational graphs.
 */
namespace MayaFlux::Nodes {

/**
 * @class NodeContext
 * @brief Base context class for node callbacks
 *
 * Provides basic context information for callbacks and can be
 * extended by specific node types to include additional context.
 * The NodeContext serves as a container for node state information
 * that is passed to callback functions, allowing callbacks to
 * access relevant node data during execution.
 *
 * Node implementations can extend this class to provide type-specific
 * context information, which can be safely accessed using the as<T>() method.
 */
class MAYAFLUX_API NodeContext {
public:
    virtual ~NodeContext() = default;

    /**
     * @brief Current sample value
     *
     * The most recent output value produced by the node.
     * This is the primary data point that callbacks will typically use.
     */
    double value;

    /**
     * @brief Type identifier for runtime type checking
     *
     * String identifier that represents the concrete type of the context.
     * Used for safe type casting with the as<T>() method.
     */
    std::string type_id;

    /**
     * @brief Safely cast to a derived context type
     * @tparam T The derived context type to cast to
     * @return Pointer to the derived context or nullptr if types don't match
     *
     * Provides type-safe access to derived context classes. If the requested
     * type matches the actual type of this context, returns a properly cast
     * pointer. Otherwise, returns nullptr to prevent unsafe access.
     *
     * Example:
     * ```cpp
     * if (auto filter_ctx = ctx.as<FilterContext>()) {
     *     // Access filter-specific context members
     *     double gain = filter_ctx->gain;
     * }
     * ```
     */
    template <typename T>
    T* as()
    {
        if (typeid(T).name() == type_id) {
            return static_cast<T*>(this);
        }
        return nullptr;
    }

protected:
    /**
     * @brief Protected constructor for NodeContext
     * @param value The current sample value
     * @param type String identifier for the context type
     *
     * This constructor is protected to ensure that only derived classes
     * can create context objects, with proper type identification.
     */
    NodeContext(double value, std::string type)
        : value(value)
        , type_id(std::move(type))
    {
    }
};

/**
 * @class Node
 * @brief Base interface for all computational processing nodes
 *
 * The Node class defines the fundamental interface for all processing components
 * in the MayaFlux engine. Nodes are the basic building blocks of transformation chains
 * and can be connected together to create complex computational graphs.
 *
 * Each node processes data on a sample-by-sample basis, allowing for flexible
 * real-time processing. Nodes can be:
 * - Connected in series (output of one feeding into input of another)
 * - Combined in parallel (outputs mixed together)
 * - Multiplied (outputs multiplied together)
 *
 * The node system supports both single-sample processing for real-time applications
 * and batch processing for more efficient offline processing.
 */
class MAYAFLUX_API Node {

public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~Node() = default;

    /**
     * @brief Processes a single data sample
     * @param input The input sample value
     * @return The processed output sample value
     *
     * This is the core processing method that all nodes must implement.
     * It takes a single input value, applies the node's transformation algorithm,
     * and returns the resulting output value.
     *
     * For generator nodes that don't require input (like oscillators or stochastic generators),
     * the input parameter may be ignored.
     * Note: This method does NOT mark the node as processed. That responsibility
     * belongs to the caller, typically a chained parent node or the root node.
     */
    virtual double process_sample(double input = 0.) = 0;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to process
     * @return Vector containing the processed samples
     *
     * This method provides batch processing capability for more efficient
     * processing of multiple samples. The default implementation typically
     * calls process_sample() for each sample, but specialized nodes can
     * override this with more optimized batch processing algorithms.
     */
    virtual std::vector<double> process_batch(unsigned int num_samples) = 0;

    /**
     * @brief Registers a callback to be called on each tick
     * @param callback Function to call with the current node context
     *
     * Registers a callback function that will be called each time the node
     * produces a new output value. The callback receives a NodeContext object
     * containing information about the node's current state.
     *
     * This mechanism enables external components to monitor and react to
     * the node's activity without interrupting the processing flow.
     *
     * Example:
     * ```cpp
     * node->on_tick([](NodeContext& ctx) {
     *     std::cout << "Node produced value: " << ctx.value << std::endl;
     * });
     * ```
     */
    virtual void on_tick(const NodeHook& callback);

    /**
     * @brief Registers a conditional callback
     * @param condition Predicate that determines when callback should be triggered
     * @param callback Function to call when condition is met
     *
     * Registers a callback function that will be called only when the specified
     * condition is met. The condition is evaluated each time the node produces
     * a new output value, and the callback is triggered only if the condition
     * returns true.
     *
     * This mechanism enables selective monitoring and reaction to specific
     * node states or events, such as threshold crossings or pattern detection.
     *
     * Example:
     * ```cpp
     * node->on_tick_if(
     *     [](NodeContext& ctx) { return ctx.value > 0.8; },
     *     [](NodeContext& ctx) { std::cout << "Threshold exceeded!" << std::endl; }
     * );
     * ```
     */
    virtual void on_tick_if(const NodeCondition& condition, const NodeHook& callback);

    /**
     * @brief Removes a previously registered callback
     * @param callback The callback to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a callback that was previously registered with on_tick().
     * After removal, the callback will no longer be triggered when the node
     * produces new output values.
     *
     * This method is useful for cleaning up callbacks when they are no longer
     * needed, preventing memory leaks and unnecessary processing.
     */
    virtual bool remove_hook(const NodeHook& callback);

    /**
     * @brief Removes a previously registered conditional callback
     * @param callback The callback part of the conditional callback to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a conditional callback that was previously registered with
     * on_tick_if(). After removal, the callback will no longer be triggered
     * even when its condition is met.
     *
     * This method is useful for cleaning up conditional callbacks when they
     * are no longer needed, preventing memory leaks and unnecessary processing.
     */
    virtual bool remove_conditional_hook(const NodeCondition& callback);

    /**
     * @brief Removes all registered callbacks
     *
     * Unregisters all callbacks that were previously registered with on_tick()
     * and on_tick_if(). After calling this method, no callbacks will be triggered
     * when the node produces new output values.
     *
     * This method is useful for completely resetting the node's callback system,
     * such as when repurposing a node or preparing for cleanup.
     */
    virtual void remove_all_hooks();

    /**
     * @brief Resets the processed state of the node and any attached input nodes
     *
     * This method is used by the processing system to reset the processed state
     * of the node at the end of each processing cycle. This ensures that
     * all nodes are marked as unprocessed before the cycle next begins, allowing
     * the system to correctly identify which nodes need to be processed.
     */
    virtual void reset_processed_state();

    /**
     * @brief Retrieves the most recent output value produced by the node
     * @return The last output sample value
     *
     * This method provides access to the node's most recent output without
     * triggering additional processing. It's useful for monitoring node state,
     * debugging, and for implementing feedback loops where a node needs to
     * access its previous output.
     *
     * The returned value represents the last sample that was produced by
     * the node's process_sample() method.
     */
    inline virtual double get_last_output() { return m_last_output; }

    /**
     * @brief Mark the specificed channel as a processor/user
     * @param channel_id The ID of the channel to register
     *
     * This method uses a bitmask to track which channels are currently using this node.
     * It allows the node to manage its state based on channel usage, which is important
     * for the audio engine's processing lifecycle. When a channel registers usage,
     * the node can adjust its processing state accordingly, such as preventing state resets
     * within the same cycle, or use the same output for multiple channels
     */
    void register_channel_usage(uint32_t channel_id);

    /**
     * @brief Removes the specified channel from the usage tracking
     * @param channel_id The ID of the channel to unregister
     */
    void unregister_channel_usage(uint32_t channel_id);

    /**
     * @brief Checks if the node is currently used by a specific channel
     * @param channel_id The ID of the channel to check
     */
    [[nodiscard]] bool is_used_by_channel(uint32_t channel_id) const;

    /**
     * @brief Requests a reset of the processed state from a specific channel
     * @param channel_id The ID of the channel requesting the reset
     *
     * This method is called by channels to signal that they have completed their
     * processing and that the node's processed state should be reset. It uses a bitmask
     * to track pending resets and ensures that all channels have completed before
     * actually resetting the node's state.
     */
    void request_reset_from_channel(uint32_t channel_id);

    /**
     * @brief Retrieves the current bitmask of active channels using this node
     * @return Bitmask where each bit represents an active channel
     *
     * This method returns the current bitmask that tracks which channels
     * are actively using this node. Each bit in the mask corresponds to a
     * specific channel ID, allowing the node to manage its state based on
     * channel usage.
     */
    [[nodiscard]] const inline std::atomic<uint32_t>& get_channel_mask() const { return m_active_channels_mask; }

    /**
     * @brief Updates the context object with the current node state
     * @param value The current sample value
     *
     * This method is responsible for updating the NodeContext object
     * with the latest state information from the node. It is called
     * internally whenever a new output value is produced, ensuring that
     * the context reflects the current state of the node for use in callbacks.
     */
    virtual void update_context(double value) = 0;

    /**
     * @brief Retrieves the last created context object
     * @return Reference to the last NodeContext object
     *
     * This method provides access to the most recent NodeContext object
     * created by the node. This context contains information about the
     * node's state at the time of the last output generation.
     */
    virtual NodeContext& get_last_context() = 0;

    /**
     * @brief Sets whether the node is compatible with GPU processing
     * @param compatible True if the node supports GPU processing, false otherwise
     */
    void set_gpu_compatible(bool compatible) { m_gpu_compatible = compatible; }

    /**
     * @brief Checks if the node supports GPU processing
     * @return True if the node is GPU compatible, false otherwise
     */
    [[nodiscard]] bool is_gpu_compatible() const { return m_gpu_compatible; }

    /**
     * @brief Provides access to the GPU data buffer
     * @return Span of floats representing the GPU data buffer
     * This method returns a span of floats that represents the GPU data buffer
     * associated with this node. The buffer contains data that can be uploaded to the GPU
     * for processing, enabling efficient execution in GPU-accelerated pipelines.
     */
    [[nodiscard]] std::span<const float> get_gpu_data_buffer() const;

protected:
    /**
     * @brief Notifies all registered callbacks with the current context
     * @param value The current sample value
     *
     * This method is called by the node implementation when a new output value
     * is produced. It creates a context object using create_context(), then
     * calls all registered callbacks with that context.
     *
     * For unconditional callbacks (registered with on_tick()), the callback
     * is always called. For conditional callbacks (registered with on_tick_if()),
     * the callback is called only if its condition returns true.
     *
     * Node implementations should call this method at appropriate points in their
     * processing flow to trigger callbacks.
     */
    virtual void notify_tick(double value) = 0;

    /**
     * @brief Resets the processed state of the node directly
     *
     * Unlike reset_processed_state(), this method is called internally
     * and does not perform any checks or state transitions.
     */
    virtual void reset_processed_state_internal();

    /**
     * @brief The most recent sample value generated by this oscillator
     *
     * This value is updated each time process_sample() is called and can be
     * accessed via get_last_output() without triggering additional processing.
     * It's useful for monitoring the oscillator's state and for implementing
     * feedback loops.
     */
    double m_last_output { 0 };

    /**
     * @brief Flag indicating if the node supports GPU processing
     * This flag is set by derived classes to indicate whether
     * the node can be processed on the GPU. Nodes that support GPU
     * processing can provide GPU-compatible context data for
     * efficient execution in GPU-accelerated pipelines.
     */
    bool m_gpu_compatible {};

    /**
     * @brief GPU data buffer for context objects
     *
     * This buffer is used to store float data that can be uploaded
     * to the GPU for nodes that support GPU processing. It provides
     * a contiguous array of floats that can be bound to GPU descriptors,
     * enabling efficient data transfer and processing on the GPU.
     */
    std::vector<float> m_gpu_data_buffer;

    /**
     * @brief Collection of standard callback functions
     *
     * Stores the registered callback functions that will be notified
     * whenever the binary operation produces a new output value. These callbacks
     * enable external components to monitor and react to the combined output
     * without interrupting the processing flow.
     */
    std::vector<NodeHook> m_callbacks;

    /**
     * @brief Collection of conditional callback functions with their predicates
     *
     * Stores pairs of callback functions and their associated condition predicates.
     * These callbacks are only invoked when their condition evaluates to true
     * for a combined output value, enabling selective monitoring of specific
     * conditions or patterns in the combined signal.
     */
    std::vector<std::pair<NodeHook, NodeCondition>> m_conditional_callbacks;

    /**
     * @brief Flag indicating if the node is part of a NodeNetwork
     * This flag is used to disable event firing when the node is
     * managed within a NodeNetwork, preventing redundant or conflicting
     * event notifications.
     */
    bool m_networked_node {};

    /**
    @brief tracks if the node's state has been saved by a snapshot operation
    */
    bool m_state_saved {};

public:
    /**
     * @brief Saves the node's current state for later restoration
     * Recursively cascades through all connected modulator nodes
     * Protected - only NodeSourceProcessor and NodeBuffer can call
     */
    virtual void save_state() = 0;

    /**
     * @brief Restores the node's state from the last save
     * Recursively cascades through all connected modulator nodes
     * Protected - only NodeSourceProcessor and NodeBuffer can call
     */
    virtual void restore_state() = 0;

    /**
     * @brief Internal flag controlling whether notify_tick fires during state snapshots
     * Default: false (events don't fire during isolated buffer processing)
     * Can be exposed in future if needed via concrete implementation in parent
     */
    bool m_fire_events_during_snapshot = false;

    /**
     * @brief Atomic state flag tracking the node's processing status
     *
     * This atomic state variable tracks the node's current operational status using
     * bit flags defined in NodeState. It indicates whether the node is:
     * - ACTIVE: Currently part of the processing graph
     * - PROCESSED: Has been processed in the current cycle
     * - PENDING_REMOVAL: Marked for removal from the processing graph
     * - MOCK_PROCESS: Should be processed but output ignored
     *
     * The atomic nature ensures thread-safe state transitions, allowing the audio
     * engine to safely coordinate processing across multiple threads without data races.
     */
    std::atomic<NodeState> m_state { NodeState::INACTIVE };

    /**
     * @brief Counter tracking how many other nodes are using this node as a modulator
     *
     * This counter is incremented when another node begins using this node as a
     * modulation source, and decremented when that relationship ends. It's critical
     * for the node lifecycle management system, as it prevents premature state resets
     * when a node's output is still needed by downstream nodes.
     *
     * When this counter is non-zero, the node's processed state will not be reset
     * automatically after processing, ensuring that all dependent nodes can access
     * its output before it's cleared.
     */
    std::atomic<uint32_t> m_modulator_count { 0 };

    /**
     * @brief Attempt to claim snapshot context for this processing cycle
     * @param context_id Unique context identifier for this buffer processing
     * @return true if this caller claimed the context (should call save_state)
     *
     * This method enables multiple NodeBuffers referencing the same node to
     * coordinate save/restore state operations. Only the first caller per
     * processing context will claim the snapshot, preventing nested state saves.
     */
    bool try_claim_snapshot_context(uint64_t context_id);

    /**
     * @brief Check if currently in a snapshot context
     * @param context_id Context to check
     * @return true if this context is active
     *
     * Used by secondary callers to detect when the primary snapshot holder
     * has completed processing and released the context.
     */
    [[nodiscard]] bool is_in_snapshot_context(uint64_t context_id) const;

    /**
     * @brief Release snapshot context
     * @param context_id Context to release
     *
     * Called by the snapshot owner after restore_state() completes,
     * allowing other buffers to proceed with their own snapshots.
     */
    void release_snapshot_context(uint64_t context_id);

    /**
     * @brief Check if node is currently being snapshotted by any context
     * @return true if a snapshot is in progress
     */
    [[nodiscard]] bool has_active_snapshot() const;

    /**
     * @brief Get the active snapshot context ID
     * @return The current active snapshot context ID, or 0 if none
     */
    [[nodiscard]] inline uint64_t get_active_snapshot_context() const
    {
        return m_snapshot_context_id.load(std::memory_order_acquire);
    }

    /**
     * @brief Increments the buffer reference count
     * This method is called when a new buffer starts using this node
     * to ensure proper lifecycle management.
     */
    void add_buffer_reference();

    /**
     * @brief Decrements the buffer reference count
     * This method is called when a buffer stops using this node
     * to ensure proper lifecycle management.
     */
    void remove_buffer_reference();

    /**
     * @brief Marks the node as having been processed by a buffer
     * @return true if the buffer was successfully marked as processed
     *
     * This method checks if the node can be marked as processed based on
     * the current buffer count and node state. If conditions are met,
     * it updates the processed flag and increments the reset counter.
     */
    bool mark_buffer_processed();

    /**
     * @brief Requests a reset of the buffer state
     *
     * This method is called to signal that the buffer's processed state
     * should be reset. It increments the reset counter, which is used to
     * determine when it's safe to clear the processed state.
     */
    void request_buffer_reset();

    /**
     * @brief Checks if the buffer has been processed
     * @return true if the buffer is marked as processed
     */
    [[nodiscard]] inline bool is_buffer_processed() const
    {
        return m_buffer_processed.load(std::memory_order_acquire);
    }

    /**
     * @brief Sets whether the node is part of a NodeNetwork
     * @param networked True if the node is managed within a NodeNetwork
     *
     * This method sets a flag indicating whether the node is part of a
     * NodeNetwork. When set, certain behaviors such as event firing
     * may be disabled to prevent redundant or conflicting notifications.
     */
    [[nodiscard]] inline bool is_in_network() const { return m_networked_node; }

    /**
     * @brief Marks the node as being part of a NodeNetwork
     * @param networked True if the node is managed within a NodeNetwork
     *
     * This method sets a flag indicating whether the node is part of a
     * NodeNetwork. When set, certain behaviors such as event firing
     * may be disabled to prevent redundant or conflicting notifications.
     */
    void set_in_network(bool networked) { m_networked_node = networked; }

private:
    /**
     * @brief Bitmask tracking which channels are currently using this node
     */
    std::atomic<uint32_t> m_active_channels_mask { 0 };

    /**
     * @brief Bitmask tracking which channels have requested a reset
     *
     * This mask is used to track which channels have requested the node's processed
     * state to be reset. When all channels that are currently using the node have
     * requested a reset, the node can safely clear its processed state.
     */
    std::atomic<uint32_t> m_pending_reset_mask { 0 };

    /**
     * @brief Unique identifier for the current snapshot context
     *
     * This atomic variable holds the unique identifier of the current
     * snapshot context that has claimed ownership of this node's state.
     * It ensures that only one processing context can perform save/restore
     * operations at a time, preventing nested snapshots and ensuring
     * consistent state management.
     */
    std::atomic<uint64_t> m_snapshot_context_id { 0 };

    /**
     * @brief Counter tracking how many buffers are using this node
     * This counter is incremented when a buffer starts using this node
     * and decremented when the buffer stops using it. It helps manage
     * the node's lifecycle in relation to buffer usage.
     */
    std::atomic<uint32_t> m_buffer_count { 0 };

    /**
     * @brief Flag indicating whether the buffer has been processed
     * This atomic flag is set when the buffer has been successfully
     * processed and is used to prevent redundant processing.
     */
    std::atomic<bool> m_buffer_processed { false };

    /**
     * @brief Counter tracking how many buffers have requested a reset
     *
     * When all buffers using this node have requested a reset, the node's
     * processed state can be safely cleared. This counter helps coordinate
     * that process.
     */
    std::atomic<uint32_t> m_buffer_reset_count { 0 };
};
}
