#pragma once

#include "MayaFlux/Nodes/Generators/Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @enum LogicMode
 * @brief Defines the computational model for digital signal evaluation
 */
enum class LogicMode {
    DIRECT, ///< Stateless evaluation of current input only (combinational logic)
    SEQUENTIAL, ///< State-based evaluation using history of inputs (sequential logic)
    TEMPORAL, ///< Time-dependent evaluation with timing constraints
    MULTI_INPUT ///< Parallel evaluation of multiple input signals
};

/**
 * @enum LogicOperator
 * @brief Digital operators for boolean computation
 */
enum class LogicOperator {
    AND, ///< Logical AND - true only when all inputs are true
    OR, ///< Logical OR - true when any input is true
    XOR, ///< Logical XOR - true when odd number of inputs are true
    NOT, ///< Logical NOT - inverts the input
    NAND, ///< Logical NAND - inverted AND operation
    NOR, ///< Logical NOR - inverted OR operation
    IMPLIES, ///< Logical implication - false only when A is true and B is false
    THRESHOLD, ///< Binary quantization - true when input exceeds threshold
    HYSTERESIS, ///< Threshold with memory - prevents rapid oscillation at boundary
    EDGE, ///< Transition detector - identifies state changes
    CUSTOM ///< User-defined boolean function
};

/**
 * @enum EdgeType
 * @brief Digital transition patterns to detect
 */
enum class EdgeType {
    RISING, ///< Low-to-high transition (0→1)
    FALLING, ///< High-to-low transition (1→0)
    BOTH ///< Any state transition
};

/**
 * @enum LogicEventType
 * @brief Events that can trigger callbacks
 */
enum class LogicEventType {
    TICK, // Every sample
    CHANGE, // Any state change
    TRUE, // Change to true
    FALSE, // Change to false
    WHILE_TRUE, // Every tick while true
    WHILE_FALSE, // Every tick while false
    CONDITIONAL // Custom condition
};

class LogicContext : public NodeContext {
public:
    /**
     * @brief Constructs a LogicContext with complete state information
     * @param value Current output value (0.0 for false, 1.0 for true)
     * @param mode Current computational model
     * @param operator_type Current boolean operator
     * @param history Buffer of previous boolean states
     * @param threshold Decision boundary for binary quantization
     * @param edge_detected Whether a state transition was detected
     * @param edge_type Type of transition being monitored
     * @param inputs Current input values for multi-input evaluation
     */
    LogicContext(double value,
        LogicMode mode,
        LogicOperator operator_type,
        const std::deque<bool>& history,
        double threshold,
        bool edge_detected = false,
        EdgeType edge_type = EdgeType::BOTH,
        const std::vector<double>& inputs = {})
        : NodeContext(value, typeid(LogicContext).name())
        , m_mode(mode)
        , m_operator(operator_type)
        , m_history(history)
        , m_threshold(threshold)
        , m_edge_detected(edge_detected)
        , m_edge_type(edge_type)
        , m_inputs(inputs)
        , m_input(value)
    {
    }

    // Getters for context properties
    LogicMode get_mode() const { return m_mode; }
    LogicOperator get_operator() const { return m_operator; }
    const std::deque<bool>& get_history() const { return m_history; }
    double get_threshold() const { return m_threshold; }
    bool is_edge_detected() const { return m_edge_detected; }
    EdgeType get_edge_type() const { return m_edge_type; }
    const std::vector<double>& get_inputs() const { return m_inputs; }
    const double& get_value() const { return m_input; }

    // Boolean conversion of the current value
    bool as_bool() const { return get_value() > 0.5; }

private:
    LogicMode m_mode; ///< Current computational model
    LogicOperator m_operator; ///< Current boolean operator
    std::deque<bool> m_history; ///< History of boolean states
    double m_threshold; ///< Decision boundary for binary quantization
    bool m_edge_detected; ///< Whether a state transition was detected
    EdgeType m_edge_type; ///< Type of transition being monitored
    std::vector<double> m_inputs; ///< Current input values (for multi-input mode)
    double m_input; ///< Current input value for multi-input mode
};

/**
 * @class Logic
 * @brief Digital signal processor implementing boolean logic operations
 *
 * The Logic node transforms continuous signals into discrete binary outputs
 * using configurable boolean operations. It supports multiple computational models:
 *
 * - Combinational logic: Stateless evaluation based solely on current inputs
 * - Sequential logic: State-based evaluation using history of previous inputs
 * - Temporal logic: Time-dependent evaluation with timing constraints
 * - Multi-input logic: Parallel evaluation of multiple input signals
 *
 * Applications include:
 * - Binary signal generation
 * - Event detection and triggering
 * - State machine implementation
 * - Digital pattern recognition
 * - Signal quantization and discretization
 * - Conditional processing chains
 */
class Logic : public Generator {
public:
    /**
     * @brief Function type for stateless boolean evaluation
     */
    using DirectFunction = std::function<bool(double)>;

    /**
     * @brief Function type for parallel input evaluation
     */
    using MultiInputFunction = std::function<bool(const std::vector<double>&)>;

    /**
     * @brief Function type for state-based evaluation
     */
    using SequentialFunction = std::function<bool(const std::deque<bool>&)>;

    /**
     * @brief Function type for time-dependent evaluation
     */
    using TemporalFunction = std::function<bool(double, double)>; // input, time

    /**
     * @brief Constructs a Logic node with threshold quantization
     * @param threshold Decision boundary for binary quantization
     *
     * Creates a basic binary quantizer that outputs 1.0 when input exceeds
     * the threshold and 0.0 otherwise.
     */
    explicit Logic(double threshold = 0.5);

    /**
     * @brief Constructs a Logic node with a specific boolean operator
     * @param op Boolean operator to apply
     * @param threshold Decision boundary for binary quantization
     *
     * Creates a Logic node configured with one of the standard boolean
     * operations (AND, OR, XOR, etc.) using the specified threshold
     * for binary quantization of inputs.
     */
    Logic(LogicOperator op, double threshold = 0.5);

    /**
     * @brief Constructs a Logic node with a custom combinational function
     * @param function Custom function implementing stateless boolean logic
     *
     * Creates a Logic node that applies a user-defined function to each input
     * sample, enabling custom combinational logic operations beyond the
     * standard operators.
     */
    explicit Logic(DirectFunction function);

    /**
     * @brief Constructs a Logic node for parallel input evaluation
     * @param function Function implementing multi-input boolean logic
     * @param input_count Number of parallel inputs to process
     *
     * Creates a Logic node that evaluates multiple inputs simultaneously,
     * enabling implementation of complex decision functions like voting
     * systems, majority logic, or custom multi-input boolean operations.
     */
    Logic(MultiInputFunction function, size_t input_count);

    /**
     * @brief Constructs a Logic node for state-based evaluation
     * @param function Function implementing sequential boolean logic
     * @param history_size Size of the state history buffer
     *
     * Creates a Logic node that maintains a history of previous states
     * and evaluates new inputs in the context of this history. This enables
     * implementation of sequential logic circuits, pattern detectors,
     * and finite state machines.
     */
    Logic(SequentialFunction function, size_t history_size);

    /**
     * @brief Constructs a Logic node for time-dependent evaluation
     * @param function Function implementing temporal boolean logic
     *
     * Creates a Logic node that evaluates inputs based on both their value
     * and the time at which they occur. This enables implementation of
     * timing-sensitive operations like pulse width detection, timeout
     * monitoring, and rate-of-change analysis.
     */
    explicit Logic(TemporalFunction function);

    virtual ~Logic() = default;

    /**
     * @brief Processes a single input sample through the logic function
     * @param input Input value to evaluate
     * @return 1.0 for true result, 0.0 for false result
     *
     * Evaluates the input according to the configured logic operation
     * and computational model, producing a binary output (0.0 or 1.0).
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples in batch mode
     * @param num_samples Number of samples to generate
     * @return Vector of binary outputs (1.0 for true, 0.0 for false)
     *
     * Generates a sequence of binary values by repeatedly applying
     * the configured logic operation. In stateful modes, each output
     * depends on previous results.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Processes multiple parallel inputs
     * @param inputs Vector of input values to evaluate together
     * @return 1.0 for true result, 0.0 for false result
     *
     * Evaluates multiple inputs simultaneously according to the configured
     * multi-input logic function, producing a single binary output.
     */
    double process_multi_input(const std::vector<double>& inputs);

    /**
     * @brief Resets internal state to initial conditions
     *
     * Clears history buffers, resets state variables, and returns
     * the node to its initial configuration. Essential for restarting
     * sequential processing or clearing accumulated state.
     */
    void reset();

    /**
     * @brief Sets the decision boundary for binary quantization
     * @param threshold Value above which input is considered true
     * @param create_default_direct_function Whether to create a default direct function if none is set
     *
     * Configures the threshold used to convert continuous input values
     * to binary states (true/false). Critical for accurate digital
     * interpretation of analog-like signals.
     */
    void set_threshold(double threshold, bool create_default_direct_function = false);

    /**
     * @brief Configures noise-resistant binary quantization with memory
     * @param low_threshold Value below which input becomes false
     * @param high_threshold Value above which input becomes true
     * @param create_default_direct_function Whether to create a default direct function if none is set
     *
     * Implements a Schmitt trigger with separate thresholds for rising and falling
     * transitions, preventing rapid oscillation when input hovers near the threshold.
     * Essential for stable binary quantization of noisy signals.
     */
    void set_hysteresis(double low_threshold, double high_threshold, bool create_default_direct_function = false);

    /**
     * @brief Configures digital transition detection
     * @param type Type of transition to detect (rising, falling, or both)
     * @param threshold Decision boundary for state changes
     *
     * Sets up the node to detect specific types of state transitions
     * (edges) in the input signal. Useful for event detection, trigger
     * generation, and synchronization with external events.
     */
    void set_edge_detection(EdgeType type, double threshold = 0.5);

    /**
     * @brief Sets the boolean operation to perform
     * @param op Boolean operator to apply
     * @param create_default_direct_function Whether to create a default direct function if none is set
     *
     * Configures the node to use one of the standard boolean operations
     * (AND, OR, XOR, etc.) for evaluating inputs.
     */
    void set_operator(LogicOperator op, bool create_default_direct_function = false);

    /**
     * @brief Sets a custom combinational logic function
     * @param function Custom function implementing stateless boolean logic
     *
     * Configures the node to use a user-defined function for stateless
     * evaluation of inputs, enabling custom combinational logic beyond
     * the standard operators.
     */
    void set_direct_function(DirectFunction function);

    /**
     * @brief Sets a custom parallel input evaluation function
     * @param function Function implementing multi-input boolean logic
     * @param input_count Number of parallel inputs to process
     *
     * Configures the node to evaluate multiple inputs simultaneously
     * using a user-defined function, enabling implementation of complex
     * decision functions like voting systems or custom multi-input operations.
     */
    void set_multi_input_function(MultiInputFunction function, size_t input_count);

    /**
     * @brief Sets a custom state-based evaluation function
     * @param function Function implementing sequential boolean logic
     * @param history_size Size of the state history buffer
     *
     * Configures the node to maintain a history of previous states and
     * evaluate new inputs in this context using a user-defined function.
     * Enables implementation of sequential logic, pattern detectors, and
     * finite state machines.
     */
    void set_sequential_function(SequentialFunction function, size_t history_size);

    /**
     * @brief Sets a custom time-dependent evaluation function
     * @param function Function implementing temporal boolean logic
     *
     * Configures the node to evaluate inputs based on both their value
     * and timing using a user-defined function. Enables implementation of
     * timing-sensitive operations like pulse width detection and rate analysis.
     */
    void set_temporal_function(TemporalFunction function);

    /**
     * @brief Preloads the state history buffer
     * @param initial_values Vector of initial boolean states
     *
     * Initializes the history buffer with specified values, allowing
     * sequential logic to begin operation with a predefined state sequence.
     * Useful for testing specific patterns or starting from known states.
     */
    void set_initial_conditions(const std::vector<bool>& initial_values);

    /**
     * @brief Sets the input node to generate logic values from
     * @param input_node Node providing the input values
     *
     * Configures the node to receive input from another node
     */
    inline void set_input_node(std::shared_ptr<Node> input_node) { m_input_node = input_node; }

    /**
     * @brief Gets the current computational model
     * @return Current logic mode
     */
    LogicMode get_mode() const { return m_mode; }

    /**
     * @brief Gets the current boolean operator
     * @return Current logic operator
     */
    LogicOperator get_operator() const { return m_operator; }

    /**
     * @brief Gets the decision boundary for binary quantization
     * @return Current threshold value
     */
    double get_threshold() const { return m_threshold; }

    /**
     * @brief Gets the state history buffer capacity
     * @return Current history buffer size
     */
    size_t get_history_size() const { return m_history_size; }

    /**
     * @brief Gets the current state history
     * @return Reference to the history buffer
     */
    const std::deque<bool>& get_history() const { return m_history; }

    /**
     * @brief Gets the number of parallel inputs expected
     * @return Number of expected inputs
     */
    size_t get_input_count() const { return m_input_count; }

    /**
     * @brief Checks if a state transition was detected
     * @return True if a transition matching the configured edge type was detected
     */
    bool was_edge_detected() const { return m_edge_detected; }

    /**
     * @brief Gets the type of transitions being monitored
     * @return Current edge detection type
     */
    EdgeType get_edge_type() const { return m_edge_type; }

    /**
     * @brief Sets the amplitude (not applicable for logic nodes)
     */
    inline void set_amplitude(double amplitude) override { }

    /**
     * @brief Prints a visual representation of the logic function
     */
    inline void printGraph() override { }

    /**
     * @brief Prints the current state and parameters
     */
    inline void printCurrent() override { }

    /**
     * @brief Registers a callback for every generated sample
     * @param callback Function to call when a new sample is generated
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback for generated samples
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback is triggered
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Registers a callback that executes continuously while output is true
     * @param callback Function to call on each tick when output is true (1.0)
     */
    void while_true(NodeHook callback);

    /**
     * @brief Registers a callback that executes continuously while output is false
     * @param callback Function to call on each tick when output is false (0.0)
     */
    void while_false(NodeHook callback);

    /**
     * @brief Registers a callback for when output changes to a specific state
     * @param callback Function to call when state changes to target_state
     * @param target_state The state to detect (true for 1.0, false for 0.0)
     */
    void on_change_to(NodeHook callback, bool target_state);

    /**
     * @brief Registers a callback for any state change (true↔false)
     * @param callback Function to call when output changes state
     */
    void on_change(NodeHook callback);

    /**
     * @brief Removes a previously registered callback
     * @param callback The callback function to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional callback
     * @param callback The condition function to remove
     * @return True if the callback was found and removed, false otherwise
     */
    bool remove_conditional_hook(const NodeCondition& callback) override;

    /**
     * @brief Removes all registered callbacks
     *
     * Clears all standard and conditional callbacks, effectively
     * disconnecting all external components from this oscillator's
     * notification system. Useful when reconfiguring the processing
     * graph or shutting down components.
     */
    inline void remove_all_hooks() override
    {
        m_all_callbacks.clear();
    }

    void remove_hooks_of_type(LogicEventType type);

    /**
     * @brief Allows RootNode to process the Generator without using the processed sample
     * @param bMock_process True to mock process, false to process normally
     *
     * NOTE: This has no effect on the behaviour of process_sample (or process_batch).
     * This is ONLY used by the RootNode when processing the node graph.
     * If the output of the Generator needs to be ignored elsewhere, simply discard the return value.
     */
    inline void enable_mock_process(bool mock_process) override
    {
        if (mock_process) {
            atomic_add_flag(m_state, Utils::NodeState::MOCK_PROCESS);
        } else {
            atomic_remove_flag(m_state, Utils::NodeState::MOCK_PROCESS);
        }
    }

    /**
     * @brief Checks if the node should mock process
     * @return True if the node should mock process, false otherwise
     */
    inline bool should_mock_process() const override
    {
        return m_state.load() & Utils::NodeState::MOCK_PROCESS;
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

    /**
     * @brief Retrieves the most recent output value produced by the oscillator
     * @return The last generated sine wave sample
     *
     * This method provides access to the oscillator's most recent output without
     * triggering additional processing. It's useful for monitoring the oscillator's state,
     * debugging, and for implementing feedback loops where a node needs to
     * access the oscillator's previous output.
     */
    inline double get_last_output() override { return m_last_output; }

    struct LogicCallback {
        NodeHook callback;
        LogicEventType event_type;
        std::optional<NodeCondition> condition; // Only used for CONDITIONAL type

        LogicCallback(NodeHook cb, LogicEventType type, std::optional<NodeCondition> cond = std::nullopt)
            : callback(cb)
            , event_type(type)
            , condition(cond)
        {
        }
    };

protected:
    /**
     * @brief Creates a context object for callbacks
     * @param value The current generated sample
     * @return A unique pointer to a GeneratorContext object
     *
     * This method creates a specialized context object containing
     * the current sample value and all oscillator parameters, providing
     * callbacks with rich information about the oscillator's state.
     */
    std::unique_ptr<NodeContext> create_context(double value) override;

    /**
     * @brief Notifies all registered callbacks about a new sample
     * @param value The newly generated sample
     *
     * This method is called internally whenever a new sample is generated,
     * creating the appropriate context and invoking all registered callbacks
     * that should receive notification about this sample.
     */
    void notify_tick(double value) override;

private:
    LogicMode m_mode; ///< Current processing mode
    LogicOperator m_operator; ///< Current logic operator
    DirectFunction m_direct_function; ///< Function for direct mode
    MultiInputFunction m_multi_input_function; ///< Function for recursive/feedforward mode
    SequentialFunction m_sequential_function; ///< Function for sequential mode
    TemporalFunction m_temporal_function; ///< Function for temporal mode
    std::deque<bool> m_history; ///< Buffer of input values for feedforward mode
    size_t m_history_size; ///< Maximum size of the history buffer
    size_t m_input_count; ///< Expected number of inputs for multi-input mode
    double m_threshold; ///< Threshold for boolean conversion
    double m_low_threshold; ///< Low threshold for hysteresis
    double m_high_threshold; ///< High threshold for hysteresis
    EdgeType m_edge_type; ///< Type of edge to detect
    bool m_edge_detected; ///< Whether an edge was detected in the last processing
    double m_last_output; ///< Most recent output value
    bool m_hysteresis_state; ///< State for hysteresis operator
    double m_temporal_time; ///< Time tracking for temporal mode
    std::vector<double> m_input_buffer; // Buffer for multi-input mode
    double m_input; ///< Current input value for multi-input mode
    std::shared_ptr<Node> m_input_node; ///< Input node for processing

    // Helper method for multi-input mode
    void add_input(double input, size_t index);

    /**
     * @brief Adds a callback to the list of all callbacks
     * @param callback The callback function to add
     * @param type The type of event to trigger the callback
     * @param condition Optional condition for conditional callbacks
     */
    void add_callback(NodeHook callback, LogicEventType type, std::optional<NodeCondition> condition = std::nullopt)
    {
        m_all_callbacks.emplace_back(callback, type, condition);
    }

    /**
     * @brief Collection of all callback functions
     *
     * Stores the registered callback functions that will be notified
     * whenever the oscillator produces a new sample. These callbacks
     * enable external components to monitor and react to the oscillator's
     * output without interrupting the generation process.
     */
    std::vector<LogicCallback> m_all_callbacks;
};

} // namespace MayaFlux::Nodes::Generator
