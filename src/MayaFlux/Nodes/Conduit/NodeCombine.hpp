#pragma once

#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {

class NodeGraphManager;

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
class MAYAFLUX_API BinaryOpContext : public NodeContext {
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
    BinaryOpContext(double value, double lhs_value, double rhs_value);

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
 * @class BinaryOpContextGpu
 * @brief GPU-compatible context for binary operation callbacks
 *
 * BinaryOpContextGpu extends BinaryOpContext and implements GpuVectorData
 * to provide GPU-compatible data for binary operation callbacks. It includes
 * the individual values from both the left and right nodes, as well as a
 * span of float data that can be uploaded to the GPU for efficient processing.
 *
 * This context enables GPU-accelerated callbacks to analyze and respond to
 * binary operations in high-performance scenarios, such as:
 * - Real-time audio processing on the GPU
 * - Complex signal interactions in visual effects
 * - High-throughput data transformations in compute shaders
 */
class MAYAFLUX_API BinaryOpContextGpu : public BinaryOpContext, public GpuVectorData {
public:
    BinaryOpContextGpu(double value, double lhs_value, double rhs_value, std::span<const float> gpu_data);
};

/**
 * @class CompositeOpContext
 * @brief Context for N-ary composite operation callbacks
 *
 * Provides the individual values from all input nodes alongside
 * the combined output value.
 */
class MAYAFLUX_API CompositeOpContext : public NodeContext {
public:
    CompositeOpContext(double value, std::vector<double> input_values);

    std::vector<double> input_values;
};

/**
 * @class CompositeOpContextGpu
 * @brief GPU-compatible context for composite operation callbacks
 */
class MAYAFLUX_API CompositeOpContextGpu : public CompositeOpContext, public GpuVectorData {
public:
    CompositeOpContextGpu(double value, std::vector<double> input_values, std::span<const float> gpu_data);
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
class MAYAFLUX_API BinaryOpNode : public Node, public std::enable_shared_from_this<BinaryOpNode> {
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
    BinaryOpNode(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs, CombineFunc func);

    /**
     * @brief Creates a new binary operation node (managed)
     * @param lhs The left-hand side node
     * @param rhs The right-hand side node
     * @param func The function to combine the outputs of both nodes
     * @param manager Graph manager for registration
     * @param token Processing domain for registration (default AUDIO_RATE)
     *
     * Common combine functions include:
     * - Addition: [](double a, double b) { return a + b; }
     * - Multiplication: [](double a, double b) { return a * b; }
     */
    BinaryOpNode(
        const std::shared_ptr<Node>& lhs,
        const std::shared_ptr<Node>& rhs,
        CombineFunc func,
        NodeGraphManager& manager,
        ProcessingToken token = ProcessingToken::AUDIO_RATE);

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

    void save_state() override;
    void restore_state() override;

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
     * @brief updates context object for callbacks
     * @param value The current combined output value
     *
     * This method updates the specialized context object containing
     * the combined output value and the individual values from both
     * input nodes, providing callbacks with rich information about
     * the operation's inputs and output.
     */
    void update_context(double value) override;

    /**
     * @brief Retrieves the last created context object
     * @return Reference to the last BinaryOpContext object
     *
     * This method provides access to the most recent BinaryOpContext object
     * created by the binary operation node. This context contains information
     * about both input values and the combined output value.
     */
    NodeContext& get_last_context() override;

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
     * @brief Reference to the node graph manager for registration and callback management
     *
     * This reference is used to register the binary operation node and its input nodes
     * with the graph manager, allowing them to be included in processing cycles and
     * for querying Config.
     */
    NodeGraphManager* m_manager {};

    /**
     * @brief The processing token indicating the domain in which this node operates
     */
    ProcessingToken m_token { ProcessingToken::AUDIO_RATE };

    /**
     * @brief The last output value from the left-hand side node
     *
     * This value is stored to provide context information to callbacks,
     * allowing them to access not just the combined result but also
     * the individual contributions from each input node.
     */
    double m_last_lhs_value {};

    /**
     * @brief The last output value from the right-hand side node
     *
     * This value is stored to provide context information to callbacks,
     * allowing them to access not just the combined result but also
     * the individual contributions from each input node.
     */
    double m_last_rhs_value {};

    /**
     * @brief Flag indicating whether the binary operator has been properly initialized
     *
     * This flag is set to true when both the lhs and rhs nodes have been
     * registered for processing and the connector itself is registered. It's used
     * to ensure that the operator func doesn't attempt to process signals before all
     * components are ready, preventing potential null pointer issues or
     * processing inconsistencies.
     */
    bool m_is_initialized {};

    bool m_state_saved {};
    double m_saved_last_lhs_value {};
    double m_saved_last_rhs_value {};

    BinaryOpContext m_context;
    BinaryOpContextGpu m_context_gpu;

public:
    bool is_initialized() const;
};

// =============================================================================
// CompositeOpNode
// =============================================================================

namespace detail {
    void composite_initialize(
        const std::vector<std::shared_ptr<Node>>& inputs,
        NodeGraphManager& manager,
        ProcessingToken token,
        const std::shared_ptr<Node>& self);

    void composite_apply_semantics(
        const std::vector<std::shared_ptr<Node>>& inputs,
        NodeGraphManager& manager,
        ProcessingToken token);

    void composite_validate(const std::vector<std::shared_ptr<Node>>& inputs, size_t N);
}

/**
 * @class CompositeOpNode
 * @brief Combines the outputs of N nodes using a composite operation
 *
 * All input nodes process the same input sample. Their outputs are
 * collected and passed to a user-supplied combine function.
 * Generalizes BinaryOpNode to arbitrary arity.
 *
 * No operator overloads are provided since there is no natural N-ary
 * operator syntax in C++.
 *
 * @tparam N Optional compile-time arity. 0 (default) means runtime-determined.
 */
template <size_t N = 0>
class MAYAFLUX_API CompositeOpNode : public Node, public std::enable_shared_from_this<CompositeOpNode<N>> {
public:
    using CombineFunc = std::function<double(std::span<const double>)>;

    /**
     * @brief Creates a composite operation node (unmanaged)
     * @param inputs The input nodes whose outputs will be combined
     * @param func Function that receives all node outputs and returns combined result
     */
    CompositeOpNode(
        std::vector<std::shared_ptr<Node>> inputs,
        CombineFunc func)
        : m_inputs(std::move(inputs))
        , m_func(std::move(func))
        , m_input_values(m_inputs.size(), 0.0)
        , m_saved_input_values(m_inputs.size(), 0.0)
        , m_context(0.0, std::vector<double>(m_inputs.size(), 0.0))
        , m_context_gpu(0.0, std::vector<double>(m_inputs.size(), 0.0), Node::get_gpu_data_buffer())
    {
        detail::composite_validate(m_inputs, N);
    }

    /**
     * @brief Creates a composite operation node (managed)
     * @param inputs The input nodes whose outputs will be combined
     * @param func Function that receives all node outputs and returns combined result
     * @param manager Graph manager for registration
     * @param token Processing domain for registration (default AUDIO_RATE)
     */
    CompositeOpNode(
        std::vector<std::shared_ptr<Node>> inputs,
        CombineFunc func,
        NodeGraphManager& manager,
        ProcessingToken token = ProcessingToken::AUDIO_RATE)
        : m_inputs(std::move(inputs))
        , m_func(std::move(func))
        , m_manager(&manager)
        , m_token(token)
        , m_input_values(m_inputs.size(), 0.0)
        , m_saved_input_values(m_inputs.size(), 0.0)
        , m_context(0.0, std::vector<double>(m_inputs.size(), 0.0))
        , m_context_gpu(0.0, std::vector<double>(m_inputs.size(), 0.0), Node::get_gpu_data_buffer())
    {
        detail::composite_validate(m_inputs, N);
    }

    /**
     * @brief Creates a composite operation node with derived node types (managed)
     * @tparam DerivedNode A type derived from Node
     * @param inputs The input nodes whose outputs will be combined
     * @param func Function that receives all node outputs and returns combined result
     * @param manager Graph manager for registration
     * @param token Processing domain for registration (default AUDIO_RATE)
     *
     * This constructor allows for creating a CompositeOpNode using a vector of shared pointers to a type derived from Node.
     * It performs an implicit conversion to the base Node type for internal storage, while still allowing the caller to use their specific node types.
     */
    template <typename DerivedNode>
        requires std::derived_from<DerivedNode, Node>
    CompositeOpNode(
        std::vector<std::shared_ptr<DerivedNode>> inputs,
        CombineFunc func,
        NodeGraphManager& manager,
        ProcessingToken token = ProcessingToken::AUDIO_RATE)
        : CompositeOpNode(
              std::vector<std::shared_ptr<Node>>(inputs.begin(), inputs.end()),
              std::move(func), manager, token)
    {
    }

    void initialize()
    {
        if (!m_is_initialized) {
            if (m_manager)
                detail::composite_initialize(m_inputs, *m_manager, m_token, this->shared_from_this());
            m_is_initialized = true;
        }
        if (m_manager)
            detail::composite_apply_semantics(m_inputs, *m_manager, m_token);
    }

    double process_sample(double input = 0.) override
    {
        if (!is_initialized())
            initialize();

        for (auto& node : m_inputs) {
            atomic_inc_modulator_count(node->m_modulator_count, 1);
        }

        for (size_t i = 0; i < m_inputs.size(); ++i) {
            uint32_t state = m_inputs[i]->m_state.load();
            if (state & NodeState::PROCESSED) {
                m_input_values[i] = input + m_inputs[i]->get_last_output();
            } else {
                m_input_values[i] = m_inputs[i]->process_sample(input);
                atomic_add_flag(m_inputs[i]->m_state, NodeState::PROCESSED);
            }
        }

        m_last_output = m_func(std::span<const double>(m_input_values));

        if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
            notify_tick(m_last_output);

        for (auto& node : m_inputs) {
            atomic_dec_modulator_count(node->m_modulator_count, 1);
        }

        for (auto& node : m_inputs) {
            try_reset_processed_state(node);
        }

        return m_last_output;
    }

    std::vector<double> process_batch(unsigned int num_samples) override
    {
        std::vector<double> output(num_samples);
        for (unsigned int i = 0; i < num_samples; ++i) {
            output[i] = process_sample(0.0);
        }
        return output;
    }

    void reset_processed_state() override
    {
        atomic_remove_flag(m_state, NodeState::PROCESSED);
        for (auto& node : m_inputs) {
            if (node)
                node->reset_processed_state();
        }
    }

    void save_state() override
    {
        m_saved_input_values = m_input_values;
        for (auto& node : m_inputs) {
            if (node)
                node->save_state();
        }
        m_state_saved = true;
    }

    void restore_state() override
    {
        m_input_values = m_saved_input_values;
        for (auto& node : m_inputs) {
            if (node)
                node->restore_state();
        }
        m_state_saved = false;
    }

    [[nodiscard]] size_t input_count() const { return m_inputs.size(); }
    [[nodiscard]] const std::vector<std::shared_ptr<Node>>& inputs() const { return m_inputs; }

    [[nodiscard]] bool is_initialized() const
    {
        for (auto& node : m_inputs) {
            auto state = node->m_state.load();
            if (state & NodeState::ACTIVE)
                return false;
        }
        return m_is_initialized;
    }

protected:
    void notify_tick(double value) override
    {
        update_context(value);
        auto& ctx = get_last_context();

        for (const auto& callback : m_callbacks) {
            callback(ctx);
        }

        for (const auto& [callback, condition] : m_conditional_callbacks) {
            if (condition(ctx)) {
                callback(ctx);
            }
        }
    }

    void update_context(double) override
    {
        if (m_gpu_compatible) {
            m_context_gpu.value = m_last_output;
            m_context_gpu.input_values = m_input_values;
        } else {
            m_context.value = m_last_output;
            m_context.input_values = m_input_values;
        }
    }

    NodeContext& get_last_context() override
    {
        if (m_gpu_compatible)
            return m_context_gpu;
        return m_context;
    }

private:
    std::vector<std::shared_ptr<Node>> m_inputs;
    CombineFunc m_func;
    NodeGraphManager* m_manager {};
    ProcessingToken m_token { ProcessingToken::AUDIO_RATE };
    std::vector<double> m_input_values;
    std::vector<double> m_saved_input_values;
    bool m_is_initialized {};
    bool m_state_saved {};

    CompositeOpContext m_context;
    CompositeOpContextGpu m_context_gpu;
};

}
