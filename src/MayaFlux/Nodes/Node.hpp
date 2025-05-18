#pragma once

#include "MayaFlux/Utils.hpp"
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
class NodeContext {
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
    NodeContext(double value, const std::string& type)
        : value(value)
        , type_id(type)
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
class Node {

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
    virtual double process_sample(double input) = 0;

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
     * node->on_tick([](const NodeContext& ctx) {
     *     std::cout << "Node produced value: " << ctx.value << std::endl;
     * });
     * ```
     */
    virtual void on_tick(NodeHook callback) = 0;

    /**
     * @brief Registers a conditional callback
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback should be triggered
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
     *     [](const NodeContext& ctx) { std::cout << "Threshold exceeded!" << std::endl; },
     *     [](const NodeContext& ctx) { return ctx.value > 0.8; }
     * );
     * ```
     */
    virtual void on_tick_if(NodeHook callback, NodeCondition condition) = 0;

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
    virtual bool remove_hook(const NodeHook& callback) = 0;

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
    virtual bool remove_conditional_hook(const NodeCondition& callback) = 0;

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
    virtual void remove_all_hooks() = 0;

    /**
     * @brief Marks the node as registered or unregistered for processing
     * @param is_registered True to mark as registered, false to mark as unregistered
     *
     * This method is used by the node graph manager to track which nodes are currently
     * part of the active processing graph. When a node is registered, it means it's
     * included in the current processing cycle. When unregistered, it may be excluded
     * from processing to save computational resources.
     *
     * This status flag helps optimize processing by allowing the system to skip
     * nodes that aren't currently needed in the signal flow.
     */
    virtual void mark_registered_for_processing(bool is_registered) = 0;

    /**
     * @brief Checks if the node is currently registered for processing
     * @return True if the node is registered, false otherwise
     *
     * This method allows checking whether the node is currently part of the
     * active processing graph. Registered nodes are included in the processing
     * cycle, while unregistered nodes may be skipped.
     *
     * This status check is useful for conditional processing logic and for
     * debugging the state of the node graph.
     */
    virtual bool is_registered_for_processing() const = 0;

    /**
     * @brief Marks the node as processed or unprocessed in the current cycle
     * @param is_processed True to mark as processed, false to mark as unprocessed
     *
     * This method is used by the processing system to track which nodes have already
     * been processed in the current cycle. This is particularly important for nodes
     * that may have multiple incoming connections, to ensure they are only processed
     * once per cycle regardless of how many nodes feed into them.
     *
     * The processed flag is typically reset at the beginning of each processing cycle
     * and set after the node has been processed.
     */
    virtual void mark_processed(bool is_processed) = 0;

    /**
     * @brief Resets the processed state of the node
     *
     * This method is used by the processing system to reset the processed state
     * of the node at the end of each processing cycle. This ensures that
     * all nodes are marked as unprocessed before the next cycle begins, allowing
     * the system to correctly identify which nodes need to be processed.
     */
    virtual void reset_processed_state() = 0;

    /**
     * @brief Checks if the node has been processed in the current cycle
     * @return True if the node has been processed, false otherwise
     *
     * This method allows checking whether the node has already been processed
     * in the current cycle. This is used by the processing system to avoid
     * redundant processing of nodes with multiple incoming connections.
     *
     * This status check is essential for maintaining the integrity of the
     * processing flow and preventing duplicate processing.
     */
    virtual bool is_processed() const = 0;

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
    virtual double get_last_output() = 0;

protected:
    /**
     * @brief Creates an appropriate context object for this node type
     * @param value The current sample value
     * @return A context object containing node-specific information
     *
     * This method is responsible for creating a NodeContext object (or a derived
     * class) that contains information about the node's current state. The context
     * object is passed to callbacks and conditions to provide them with the
     * information they need to execute properly.
     *
     * Node implementations should override this method to create a context object
     * that includes all relevant node-specific information.
     */
    virtual std::unique_ptr<NodeContext> create_context(double value) = 0;

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

private:
    /**
     * @brief Specifies the processing behavior of the node
     *
     * Determines how the node processes input and generates output
     * (e.g., generator, processor, transformer, etc.)
     */
    MayaFlux::Utils::NodeProcessType node_type;

    /**
     * @brief Unique identifier for the node
     *
     * Used for node lookup and connection management in the node graph
     */
    std::string m_Name;
};
}
