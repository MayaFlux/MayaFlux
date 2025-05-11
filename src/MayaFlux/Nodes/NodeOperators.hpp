#pragma once

#include "MayaFlux/Nodes/NodeStructure.hpp"

namespace MayaFlux::Nodes {

/**
 * @class MixAdapter
 * @brief A Node adapter that mixes the output of two nodes with a specified blend ratio.
 *
 * MixAdapter provides a memory-safe alternative to BinaryOpNode when used in interactive
 * environments like Cling. It processes audio by routing the input through both a primary
 * and secondary node, then blending their outputs according to a mix parameter.
 *
 * The mix parameter ranges from 0.0 to 1.0, where:
 * - 0.0 means 100% of the primary node's output and 0% of the secondary node's output
 * - 1.0 means 0% of the primary node's output and 100% of the secondary node's output
 * - Values in between create a proportional blend of both outputs
 *
 * This adapter is particularly useful for creating crossfades between different signal
 * processing chains or for blending effects in a data-driven DSP workflow.
 *
 * @see ChainAdapter
 * @see NodeWrapper
 */
class MixAdapter : public Node {
public:
    /**
     * @brief Constructs a MixAdapter with specified nodes and mix ratio.
     *
     * @param primary The main node whose output will be mixed
     * @param secondary The secondary node whose output will be mixed
     * @param mix The mix ratio between 0.0 (100% primary) and 1.0 (100% secondary)
     */
    MixAdapter(std::shared_ptr<Node> primary, std::shared_ptr<Node> secondary, float mix);

    /**
     * @brief Processes a single sample through both nodes and mixes their outputs.
     *
     * The input sample is processed by both the primary and secondary nodes,
     * and their outputs are mixed according to the mix parameter.
     *
     * @param input The input sample to process
     * @return The mixed output sample
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once.
     *
     * @param num_samples The number of samples to process
     * @return A vector containing the processed samples
     */
    std::vector<double> processFull(unsigned int num_samples) override;

    /**
     * @brief Registers a callback to be executed on each processing tick.
     *
     * @param callback The function to call on each tick
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback to be executed on ticks when condition is met.
     *
     * @param callback The function to call when condition is true
     * @param condition The condition that determines when the callback is executed
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Removes a previously registered tick callback.
     *
     * @param callback The callback to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional tick callback.
     *
     * @param callback The conditional callback to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_conditional_hook(const NodeCondition& callback) override;

    /**
     * @brief Removes all registered callbacks.
     */
    void remove_all_hooks() override;

protected:
    inline void notify_tick(double value) override { }

    inline std::unique_ptr<NodeContext> create_context(double sample) override { return nullptr; }

private:
    std::shared_ptr<Node> m_primary;
    std::shared_ptr<Node> m_secondary;
    float m_mix;
};

/**
 * @class ChainAdapter
 * @brief A Node adapter that chains the output of one node to the input of another.
 *
 * ChainAdapter provides a memory-safe alternative to ChainNode when used in interactive
 * environments like Cling. It processes audio by routing the input through a source node,
 * then feeding that output as input to a target node if one is set.
 *
 * This adapter implements the signal flow pattern common in digital audio processing,
 * where nodes are connected in series to create a processing chain. The adapter maintains
 * references to both nodes and handles the proper routing of signals between them.
 *
 * Unlike the standard ChainNode, the ChainAdapter allows for dynamic reconfiguration
 * of the target node after construction, making it more flexible for interactive
 * DSP environments.
 *
 * @see MixAdapter
 * @see NodeWrapper
 */
class ChainAdapter : public Node {
public:
    /**
     * @brief Constructs a ChainAdapter with a specified source node.
     *
     * @param source The node that will receive the initial input
     */
    ChainAdapter(std::shared_ptr<Node> source);

    /**
     * @brief Sets the target node that will receive the output of the source node.
     *
     * This method allows for dynamic reconfiguration of the processing chain.
     *
     * @param target The node that will process the output from the source node
     */
    inline void set_target(std::shared_ptr<Node> target)
    {
        m_target = target;
    }

    /**
     * @brief Processes a single sample through the chain of nodes.
     *
     * The input is first processed by the source node. If a target node is set,
     * the output from the source is then passed as input to the target node.
     *
     * @param input The input sample to process
     * @return The processed output sample
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once through the chain.
     *
     * Each sample is processed through the source node and then through
     * the target node if one is set.
     *
     * @param num_samples The number of samples to process
     * @return A vector containing the processed samples
     */
    std::vector<double> processFull(unsigned int num_samples) override;

    /**
     * @brief Registers a callback to be executed on each processing tick.
     *
     * The callback is propagated to both source and target nodes if they exist.
     *
     * @param callback The function to call on each tick
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback to be executed on ticks when condition is met.
     *
     * The conditional callback is propagated to both source and target nodes if they exist.
     *
     * @param callback The function to call when condition is true
     * @param condition The condition that determines when the callback is executed
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Removes a previously registered tick callback from both nodes in the chain.
     *
     * @param callback The callback to remove
     * @return True if the callback was found and removed from either node, false otherwise
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional tick callback from both nodes in the chain.
     *
     * @param callback The conditional callback to remove
     * @return True if the callback was found and removed from either node, false otherwise
     */
    bool remove_conditional_hook(const NodeCondition& callback) override;

    /**
     * @brief Removes all registered callbacks from both nodes in the chain.
     */
    void remove_all_hooks() override;

protected:
    inline void notify_tick(double value) override { }
    inline std::unique_ptr<NodeContext> create_context(double value) override { return nullptr; }

private:
    std::shared_ptr<Node> m_source;
    std::shared_ptr<Node> m_target;
};

/**
 * @enum WrapMode
 * @brief Defines the operational mode of a NodeWrapper.
 *
 * This enumeration specifies how a NodeWrapper processes input signals
 * and interacts with its wrapped node(s).
 */
enum class WrapMode {
    /**
     * @brief Direct mode - input is passed directly to the wrapped node.
     *
     * In this mode, the wrapper simply forwards input to its wrapped node
     * and returns the output without any additional processing.
     */
    Direct,

    /**
     * @brief Chain mode - input is processed through an upstream node before the wrapped node.
     *
     * In this mode, the input is first processed by an upstream node, and the
     * result is then passed to the wrapped node for further processing.
     */
    Chain,

    /**
     * @brief Mix mode - input is processed by multiple nodes and their outputs are summed.
     *
     * In this mode, the input is processed by the wrapped node and all additional
     * mix sources, and their outputs are summed to produce the final result.
     */
    Mix
};

/**
 * @class NodeWrapper
 * @brief A flexible wrapper for Node objects that supports different processing modes.
 *
 * NodeWrapper provides a memory-safe way to manipulate and combine nodes in interactive
 * environments like Cling. It can operate in three different modes:
 *
 * 1. Direct mode: Simply forwards input to the wrapped node
 * 2. Chain mode: Processes input through an upstream node before the wrapped node
 * 3. Mix mode: Processes input through multiple nodes and sums their outputs
 *
 * This wrapper is particularly useful for creating complex signal processing chains
 * and networks in a data-driven DSP workflow, allowing for dynamic reconfiguration
 * of the processing graph at runtime.
 *
 * @see MixAdapter
 * @see ChainAdapter
 */
class NodeWrapper : public Node {
public:
    /**
     * @brief Constructs a NodeWrapper around the specified node.
     *
     * By default, the wrapper operates in Direct mode.
     *
     * @param wrapped The node to be wrapped
     */
    NodeWrapper(std::shared_ptr<Node> wrapped);

    /**
     * @brief Processes a single sample through the wrapped node(s) according to the current mode.
     *
     * The behavior depends on the current mode:
     * - Direct mode: Input is processed by the wrapped node
     * - Chain mode: Input is processed by the upstream node, then by the wrapped node
     * - Mix mode: Input is processed by the wrapped node and all mix sources, and outputs are summed
     *
     * @param input The input sample to process
     * @return The processed output sample
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once.
     *
     * @param num_samples The number of samples to process
     * @return A vector containing the processed samples
     */
    std::vector<double> processFull(unsigned int num_samples) override;

    /**
     * @brief Registers a callback to be executed on each processing tick.
     *
     * The callback is propagated to the wrapped node and any connected nodes.
     *
     * @param callback The function to call on each tick
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback to be executed on ticks when condition is met.
     *
     * The conditional callback is propagated to the wrapped node and any connected nodes.
     *
     * @param callback The function to call when condition is true
     * @param condition The condition that determines when the callback is executed
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Removes a previously registered tick callback.
     *
     * @param callback The callback to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional tick callback.
     *
     * @param callback The conditional callback to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_conditional_hook(const NodeCondition& callback) override;

    /**
     * @brief Removes all registered callbacks from the wrapped node and any connected nodes.
     */
    void remove_all_hooks() override;

    /**
     * @brief Sets an upstream node to process input before the wrapped node.
     *
     * This switches the wrapper to Chain mode.
     *
     * @param upstream The node that will process input before the wrapped node
     */
    void set_upstream(std::shared_ptr<Node> upstream);

    /**
     * @brief Adds a node whose output will be mixed with the wrapped node's output.
     *
     * This switches the wrapper to Mix mode.
     *
     * @param source The node whose output will be added to the mix
     */
    void add_mix_source(std::shared_ptr<Node> source);

    /**
     * @brief Gets the wrapped node.
     *
     * @return A shared pointer to the wrapped node
     */
    inline std::shared_ptr<Node> get_wrapped() const { return m_wrapped; }

protected:
    inline void notify_tick(double value) override { }
    inline std::unique_ptr<NodeContext> create_context(double value) override { return nullptr; }

private:
    std::shared_ptr<Node> m_wrapped;
    std::shared_ptr<Node> m_upstream;
    std::vector<std::shared_ptr<Node>> m_mix_sources;
    WrapMode m_mode;
};

/**
 * @brief Determines if the code is executing within a Cling interactive C++ interpreter context.
 *
 * This function is used to adapt the behavior of node operations when running in an
 * interactive environment like Cling, where memory management and object lifetimes
 * require special handling.
 *
 * @return True if executing in a Cling context, false otherwise
 */
inline bool in_cling_context()
{
    static thread_local bool is_cling = false;
    return is_cling;
}

/**
 * @brief Ensures a node is wrapped in a NodeWrapper if needed for safe interactive use.
 *
 * This utility function checks if a node is already wrapped, and if not, wraps it
 * in a NodeWrapper when running in a Cling context. This ensures proper memory
 * management and enables dynamic reconfiguration of the node graph.
 *
 * @param node The node to ensure is wrappable
 * @return The original node if already wrapped or not in Cling context, otherwise a new NodeWrapper
 */
inline std::shared_ptr<Node> ensure_wrappable(std::shared_ptr<Node> node)
{
    if (auto wrapper = std::dynamic_pointer_cast<NodeWrapper>(node)) {
        return wrapper;
    }
    if (in_cling_context()) {
        return std::make_shared<NodeWrapper>(node);
    }
    return node;
}

/**
 * @brief Chains two nodes together, connecting the output of the left node to the input of the right node.
 *
 * This operator overload provides a convenient syntax for creating signal processing chains.
 * When used in a Cling context, it uses NodeWrapper for safe memory management.
 * Otherwise, it creates a ChainNode.
 *
 * Example usage: auto chain = oscillator >> filter >> reverb;
 *
 * @param lhs The source node whose output will be connected
 * @param rhs The target node that will receive the output from the source
 * @return A node representing the chained operation
 */
inline std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    if (in_cling_context()) {
        auto wrapped_rhs = ensure_wrappable(rhs);
        if (auto wrapper = std::dynamic_pointer_cast<NodeWrapper>(wrapped_rhs)) {
            wrapper->set_upstream(lhs);
            return wrapped_rhs;
        }
    }

    return std::make_shared<ChainNode>(lhs, rhs);
}

/**
 * @brief Adds the outputs of two nodes together.
 *
 * This operator overload provides a convenient syntax for mixing signals.
 * When used in a Cling context, it uses NodeWrapper for safe memory management.
 * Otherwise, it creates a BinaryOpNode that performs addition.
 *
 * Example usage: auto mix = drum_beat + bass_line;
 *
 * @param lhs The first node whose output will be added
 * @param rhs The second node whose output will be added
 * @return A node representing the addition operation
 */
inline std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    if (in_cling_context()) {
        auto wrapped_lhs = ensure_wrappable(lhs);
        if (auto wrapper = std::dynamic_pointer_cast<NodeWrapper>(wrapped_lhs)) {
            wrapper->add_mix_source(rhs);
            return wrapped_lhs;
        }
    }

    return std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a + b; });
}

/**
 * @brief Multiplies the outputs of two nodes together.
 *
 * This operator overload provides a convenient syntax for amplitude modulation
 * and other multiplication-based effects. When used in a Cling context, it uses
 * MixAdapter with a mix value of 0.0 for safe memory management. Otherwise, it
 * creates a BinaryOpNode that performs multiplication.
 *
 * Example usage: auto amplitude_modulation = carrier * modulator;
 *
 * @param lhs The first node whose output will be multiplied
 * @param rhs The second node whose output will be multiplied
 * @return A node representing the multiplication operation
 */
inline std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    if (in_cling_context()) {
        return std::make_shared<MixAdapter>(lhs, rhs, 0.0f); // 0.0 mix = multiply
    }

    return std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a * b; });
}
}
