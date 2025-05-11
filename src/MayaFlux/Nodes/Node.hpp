#pragma once

#include "MayaFlux/Utils.hpp"

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
 * @typedef NodeHook
 * @brief Callback function type for node processing events
 *
 * A NodeHook is a function that receives a NodeContext object containing
 * information about the node's current state. These callbacks are triggered
 * during node processing to notify external components about node activity.
 *
 * Example:
 * ```cpp
 * node->on_tick([](const NodeContext& ctx) {
 *     std::cout << "Node produced value: " << ctx.value << std::endl;
 * });
 * ```
 */
using NodeHook = std::function<void(const NodeContext&)>;

/**
 * @typedef NodeCondition
 * @brief Predicate function type for conditional callbacks
 *
 * A NodeCondition is a function that evaluates whether a callback should
 * be triggered based on the node's current state. It receives a NodeContext
 * object and returns true if the condition is met, false otherwise.
 *
 * Example:
 * ```cpp
 * node->on_tick_if(
 *     [](const NodeContext& ctx) { std::cout << "Threshold exceeded!" << std::endl; },
 *     [](const NodeContext& ctx) { return ctx.value > 0.8; }
 * );
 * ```
 */
using NodeCondition = std::function<bool(const NodeContext&)>;

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
    virtual std::vector<double> processFull(unsigned int num_samples) = 0;

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

/**
 * @brief Connects two nodes in series (pipeline operator)
 * @param lhs Source node
 * @param rhs Target node
 * @return The target node for further chaining
 *
 * Creates a connection where the output of the left-hand node
 * becomes the input to the right-hand node. This allows for
 * intuitive creation of processing chains:
 *
 * ```cpp
 * auto chain = generator >> transformer >> output;
 * ```
 *
 * The returned node is the right-hand node, allowing for
 * further chaining of operations.
 */
std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

/**
 * @brief Combines two nodes in parallel (addition operator)
 * @param lhs First node
 * @param rhs Second node
 * @return A new node that outputs the sum of both nodes' outputs
 *
 * Creates a new node that processes both input nodes and sums their outputs.
 * This allows for mixing multiple data sources or transformations:
 *
 * ```cpp
 * auto combined = primary_source + secondary_source;
 * ```
 *
 * The resulting node takes an input, passes it to both source nodes,
 * and returns the sum of their outputs.
 */
std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

/**
 * @brief Multiplies the outputs of two nodes (multiplication operator)
 * @param lhs First node
 * @param rhs Second node
 * @return A new node that outputs the product of both nodes' outputs
 *
 * Creates a new node that processes both input nodes and multiplies their outputs.
 * This is useful for amplitude modulation, scaling operations, and other
 * multiplicative transformations:
 *
 * ```cpp
 * auto modulated = carrier * modulator;
 * ```
 *
 * The resulting node takes an input, passes it to both source nodes,
 * and returns the product of their outputs.
 */
std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
}
