#pragma once
#include "Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class ChainNode
 * @brief Connects two nodes in series to form a processing chain
 *
 * The ChainNode implements the Node interface and represents a connection
 * between two nodes where the output of the source node becomes the input
 * to the target node. This is the implementation behind the '>>' operator
 * for nodes.
 *
 * When processed, the ChainNode:
 * 1. Passes the input to the source node
 * 2. Takes the output from the source node
 * 3. Passes that as input to the target node
 * 4. Returns the output from the target node
 */
class ChainNode : public Node, public std::enable_shared_from_this<ChainNode> {
public:
    /**
     * @brief Creates a new chain connecting source to target
     * @param source The upstream node that processes input first
     * @param target The downstream node that processes the source's output
     */
    ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target);

    /**
     * @brief Initializes the chain node
     *
     * This method performs necessary setup for the chain node, ensuring both
     * the source and target nodes are properly initialized and registered.
     * It should be called before the chain is used for processing to ensure
     * correct signal flow through the connected nodes.
     *
     * Initialization is particularly important for chains to ensure that the
     * signal path is properly established before processing begins.
     */
    void initialize();

    /**
     * @brief Processes a single sample through the chain
     * @param input The input sample
     * @return The output after processing through both nodes
     *
     * The input is first processed by the source node, and the result
     * is then processed by the target node.
     */
    double process_sample(double input = 0.) override;

    /**
     * @brief Processes multiple samples through the chain
     * @param num_samples Number of samples to process
     * @return Vector of processed samples
     *
     * Each sample is processed through the source node and then
     * through the target node.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Registers a callback for every output sample
     * @param callback Function to call when a new sample is output
     *
     * This method delegates to the target node's on_tick method,
     * ensuring that callbacks are triggered based on the final
     * output of the chain rather than intermediate values.
     * The callback receives the context from the target node.
     */
    inline void on_tick(NodeHook callback) override
    {
        m_Target->on_tick(callback);
    }

    /**
     * @brief Registers a conditional callback for output samples
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback is triggered
     *
     * This method delegates to the target node's on_tick_if method,
     * ensuring that conditional callbacks are evaluated based on the
     * final output of the chain. The callback and condition both
     * receive the context from the target node.
     */
    inline void on_tick_if(NodeHook callback, NodeCondition condition) override
    {
        m_Target->on_tick_if(callback, condition);
    }

    /**
     * @brief Removes a previously registered callback
     * @param callback The callback function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * This method delegates to the target node's remove_hook method,
     * removing the callback from the target node's notification system.
     */
    inline bool remove_hook(const NodeHook& callback) override
    {
        return m_Target->remove_hook(callback);
    }

    /**
     * @brief Removes a previously registered conditional callback
     * @param callback The condition function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * This method delegates to the target node's remove_conditional_hook method,
     * removing the conditional callback from the target node's notification system.
     */
    inline bool remove_conditional_hook(const NodeCondition& callback) override
    {
        return m_Target->remove_conditional_hook(callback);
    }

    /**
     * @brief Removes all registered callbacks
     *
     * This method delegates to the target node's remove_all_hooks method,
     * clearing all callbacks from the target node's notification system.
     * After calling this method, no callbacks will be triggered for this chain.
     */
    inline void remove_all_hooks() override
    {
        m_Target->remove_all_hooks();
    }

    /**
     * @brief Resets the processed state of the node and any attached input nodes
     *
     * This method is used by the processing system to reset the processed state
     * of the node at the end of each processing cycle. This ensures that
     * all nodes are marked as unprocessed before the cycle next begins, allowing
     * the system to correctly identify which nodes need to be processed.
     */
    void reset_processed_state() override;

protected:
    /**
     * @brief Empty implementation of notify_tick
     * @param value The output value
     *
     * ChainNode doesn't implement its own notification system,
     * instead delegating all callback handling to the target node.
     * This method is a placeholder to satisfy the Node interface.
     */
    inline void notify_tick(double) override { }

    /**
     * @brief Empty implementation of create_context
     * @param value The output value
     * @return nullptr as this node doesn't create contexts
     *
     * ChainNode doesn't create its own contexts for callbacks,
     * instead relying on the target node to provide appropriate contexts.
     * This method is a placeholder to satisfy the Node interface.
     */
    inline std::unique_ptr<NodeContext> create_context(double) override { return nullptr; }

private:
    /**
     * @brief The upstream node that processes input first
     */
    std::shared_ptr<Node> m_Source;

    /**
     * @brief The downstream node that processes the source's output
     */
    std::shared_ptr<Node> m_Target;

    /**
     * @brief Flag indicating whether the chain has been properly initialized
     *
     * This flag is set to true when both the source and target nodes have been
     * registered for processing and the chain itself is registered. It's used
     * to ensure that the chain doesn't attempt to process signals before all
     * components are ready, preventing potential null pointer issues or
     * processing inconsistencies.
     */
    bool m_is_initialized;

public:
    inline bool is_initialized() const
    {
        auto sState = m_Source->m_state.load();
        auto tState = m_Target->m_state.load();

        bool is_source_registered = m_Source ? (sState & Utils::NodeState::ACTIVE) : false;
        bool is_target_registered = m_Target ? (tState & Utils::NodeState::ACTIVE) : false;
        return !is_source_registered && !is_target_registered && (m_state.load() & Utils::NodeState::ACTIVE);
    }
};

/**
 * @class BinaryOpContext
 * @brief Specialized context for binary operation callbacks
 *
 * BinaryOpContext extends the base NodeContext to provide detailed information
 * about a binary operation's inputs and output to callbacks. It includes the
 * individual values from both the left and right nodes that were combined to
 * produce the final output value.
 *
 * This rich context enables callbacks to perform sophisticated analysis and
 * monitoring of signal combinations, such as:
 * - Tracking the relative contributions of each input signal
 * - Implementing adaptive responses based on input relationships
 * - Detecting specific interaction patterns between signals
 * - Creating visualizations that show both inputs and their combination
 */
class BinaryOpContext : public NodeContext {
public:
    /**
     * @brief Constructs a BinaryOpContext with the current operation state
     * @param value The combined output value
     * @param lhs_value The value from the left-hand side node
     * @param rhs_value The value from the right-hand side node
     *
     * Creates a context object that provides a complete snapshot of the
     * binary operation's current state, including both input values and
     * the resulting output value after combination.
     */
    BinaryOpContext(double value, double lhs_value, double rhs_value)
        : NodeContext(value, typeid(BinaryOpContext).name())
        , lhs_value(lhs_value)
        , rhs_value(rhs_value)
    {
    }

    /**
     * @brief The value from the left-hand side node
     *
     * This is the output value from the left node before combination.
     * It allows callbacks to analyze the individual contribution of
     * the left node to the final combined output.
     */
    double lhs_value;

    /**
     * @brief The value from the right-hand side node
     *
     * This is the output value from the right node before combination.
     * It allows callbacks to analyze the individual contribution of
     * the right node to the final combined output.
     */
    double rhs_value;
};

/**
 * @class BinaryOpNode
 * @brief Combines the outputs of two nodes using a binary operation
 *
 * The BinaryOpNode implements the Node interface and represents a combination
 * of two nodes where both nodes process the same input, and their outputs
 * are combined using a specified binary operation (like addition or multiplication).
 * This is the implementation behind the '+' and '*' operators for nodes.
 *
 * When processed, the BinaryOpNode:
 * 1. Passes the input to both the left and right nodes
 * 2. Takes the outputs from both nodes
 * 3. Combines the outputs using the specified function
 * 4. Returns the combined result
 */
class BinaryOpNode : public Node, public std::enable_shared_from_this<BinaryOpNode> {
public:
    /**
     * @typedef CombineFunc
     * @brief Function type for combining two node outputs
     *
     * A function that takes two double values (the outputs from the left
     * and right nodes) and returns a single double value (the combined result).
     */
    using CombineFunc = std::function<double(double, double)>;

    /**
     * @brief Creates a new binary operation node
     * @param lhs The left-hand side node
     * @param rhs The right-hand side node
     * @param func The function to combine the outputs of both nodes
     *
     * Common combine functions include:
     * - Addition: [](double a, double b) { return a + b; }
     * - Multiplication: [](double a, double b) { return a * b; }
     */
    BinaryOpNode(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, CombineFunc func);

    /**
     * @brief Initializes the binary operation node
     *
     * This method performs any necessary setup for the binary operation node,
     * such as ensuring both input nodes are properly initialized and registered.
     * It should be called before the node is used for processing to ensure
     * correct operation.
     */
    void initialize();

    /**
     * @brief Processes a single sample through both nodes and combines the results
     * @param input The input sample
     * @return The combined output after processing through both nodes
     *
     * The input is processed by both the left and right nodes, and their
     * outputs are combined using the specified function.
     */
    double process_sample(double input = 0.) override;

    /**
     * @brief Processes multiple samples through both nodes and combines the results
     * @param num_samples Number of samples to process
     * @return Vector of combined processed samples
     *
     * Each sample is processed through both the left and right nodes, and
     * their outputs are combined using the specified function.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Resets the processed state of the node and any attached input nodes
     *
     * This method is used by the processing system to reset the processed state
     * of the node at the end of each processing cycle. This ensures that
     * all nodes are marked as unprocessed before the cycle next begins, allowing
     * the system to correctly identify which nodes need to be processed.
     */
    void reset_processed_state() override;

protected:
    /**
     * @brief Notifies all registered callbacks about a new output value
     * @param value The newly combined output value
     *
     * This method is called internally whenever a new value is produced,
     * creating the appropriate context with both input values and the output,
     * and invoking all registered callbacks that should receive notification.
     */
    void notify_tick(double value) override;

    /**
     * @brief Creates a context object for callbacks
     * @param value The current combined output value
     * @return A unique pointer to a BinaryOpContext object
     *
     * This method creates a specialized context object containing
     * the combined output value and the individual values from both
     * input nodes, providing callbacks with rich information about
     * the operation's inputs and output.
     */
    inline std::unique_ptr<NodeContext> create_context(double value) override
    {
        return std::make_unique<BinaryOpContext>(value, m_last_lhs_value, m_last_rhs_value);
    }

private:
    /**
     * @brief The left-hand side node
     */
    std::shared_ptr<Node> m_lhs;

    /**
     * @brief The right-hand side node
     */
    std::shared_ptr<Node> m_rhs;

    /**
     * @brief The function used to combine the outputs of both nodes
     */
    CombineFunc m_func;

    /**
     * @brief The last output value from the left-hand side node
     *
     * This value is stored to provide context information to callbacks,
     * allowing them to access not just the combined result but also
     * the individual contributions from each input node.
     */
    double m_last_lhs_value = 0.0;

    /**
     * @brief The last output value from the right-hand side node
     *
     * This value is stored to provide context information to callbacks,
     * allowing them to access not just the combined result but also
     * the individual contributions from each input node.
     */
    double m_last_rhs_value = 0.0;

    /**
     * @brief Flag indicating whether the binary operator has been properly initialized
     *
     * This flag is set to true when both the lhs and rhs nodes have been
     * registered for processing and the connector itself is registered. It's used
     * to ensure that the operator func doesn't attempt to process signals before all
     * components are ready, preventing potential null pointer issues or
     * processing inconsistencies.
     */
    bool m_is_initialized;

public:
    inline bool is_initialized() const
    {
        auto lstate = m_lhs->m_state.load();
        auto rstate = m_lhs->m_state.load();
        bool is_lhs_registered = m_lhs ? (lstate & Utils::NodeState::ACTIVE) : false;
        bool is_rhs_registered = m_rhs ? (rstate & Utils::NodeState::ACTIVE) : false;
        return !is_lhs_registered && !is_rhs_registered;
    }
};
}
