#pragma once

#include "MayaFlux/Nodes/Generators/Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @enum PolynomialMode
 * @brief Defines how the polynomial function processes input values
 */
enum class PolynomialMode {
    DIRECT, ///< Evaluates f(x) where x is the current phase/input
    RECURSIVE, ///< Evaluates using current and previous outputs: y[n] = f(y[n-1], y[n-2], ...)
    FEEDFORWARD ///< Evaluates using current and previous inputs: y[n] = f(x[n], x[n-1], ...)
};

class PolynomialContext : public NodeContext {
public:
    /**
     * @brief Constructs a PolynomialContext
     * @param value Current output value
     * @param mode Current polynomial mode
     * @param buffer_size Size of the input/output buffers
     * @param input_buffer Current input buffer contents
     * @param output_buffer Current output buffer contents
     * @param coefficients Current polynomial coefficients
     */
    PolynomialContext(double value,
        PolynomialMode mode,
        size_t buffer_size,
        const std::deque<double>& input_buffer,
        const std::deque<double>& output_buffer,
        const std::vector<double>& coefficients)
        : NodeContext(value, typeid(PolynomialContext).name())
        , m_mode(mode)
        , m_buffer_size(buffer_size)
        , m_input_buffer(input_buffer)
        , m_output_buffer(output_buffer)
        , m_coefficients(coefficients)
    {
    }

    /**
     * @brief Gets the current polynomial mode
     * @return Current polynomial mode
     */
    PolynomialMode get_mode() const { return m_mode; }

    /**
     * @brief Gets the buffer size
     * @return Current buffer size
     */
    size_t get_buffer_size() const { return m_buffer_size; }

    /**
     * @brief Gets the input buffer
     * @return Reference to the input buffer
     */
    const std::deque<double>& get_input_buffer() const { return m_input_buffer; }

    /**
     * @brief Gets the output buffer
     * @return Reference to the output buffer
     */
    const std::deque<double>& get_output_buffer() const { return m_output_buffer; }

    /**
     * @brief Gets the polynomial coefficients
     * @return Reference to the coefficient vector
     */
    const std::vector<double>& get_coefficients() const { return m_coefficients; }

private:
    PolynomialMode m_mode; ///< Current processing mode
    size_t m_buffer_size; ///< Size of the buffers
    std::deque<double> m_input_buffer; ///< Copy of input buffer
    std::deque<double> m_output_buffer; ///< Copy of output buffer
    std::vector<double> m_coefficients; ///< Copy of polynomial coefficients
    std::unordered_map<std::string, std::any> m_attributes; ///< Named attributes for callbacks
};

/**
 * @class Polynomial
 * @brief Generator that produces values based on polynomial functions
 *
 * The Polynomial generator creates a signal based on mathematical functions
 * that can operate in different modes:
 * - Direct mode: evaluates f(x) where x is the current phase or input
 * - Recursive mode: evaluates using current and previous outputs
 * - Feedforward mode: evaluates using current and previous inputs
 *
 * This flexible approach allows implementing various types of polynomial
 * equations, difference equations, and recurrence relations.
 */
class Polynomial : public Generator {
public:
    /**
     * @brief Function type for direct polynomial evaluation
     *
     * Takes a single input value and returns the computed output.
     */
    using DirectFunction = std::function<double(double)>;

    /**
     * @brief Function type for recursive/feedforward polynomial evaluation
     *
     * Takes a reference to a buffer of values and returns the computed output.
     * For recursive mode, the buffer contains previous outputs.
     * For feedforward mode, the buffer contains current and previous inputs.
     */
    using BufferFunction = std::function<double(const std::deque<double>&)>;

    /**
     * @class PolynomialContext
     * @brief Context object for polynomial node callbacks
     *
     * Extends the base NodeContext with polynomial-specific information
     * such as mode, buffer contents, and coefficients.
     */
    ;

    /**
     * @brief Constructs a Polynomial generator in direct mode with coefficient-based definition
     * @param coefficients Vector of polynomial coefficients (highest power first)
     *
     * Creates a polynomial generator that evaluates a standard polynomial function
     * defined by the provided coefficients. The coefficients are ordered from
     * highest power to lowest (e.g., [2, 3, 1] represents 2x² + 3x + 1).
     */
    explicit Polynomial(const std::vector<double>& coefficients);

    /**
     * @brief Constructs a Polynomial generator in direct mode with a custom function
     * @param function Custom function that maps input to output value
     *
     * Creates a polynomial generator that evaluates a custom mathematical function
     * provided by the user.
     */
    explicit Polynomial(DirectFunction function);

    /**
     * @brief Constructs a Polynomial generator in recursive or feedforward mode
     * @param function Function that processes a buffer of values
     * @param mode Processing mode (RECURSIVE or FEEDFORWARD)
     * @param buffer_size Number of previous values to maintain
     *
     * Creates a polynomial generator that evaluates using current and previous values.
     * In recursive mode, the buffer contains previous outputs.
     * In feedforward mode, the buffer contains current and previous inputs.
     */
    Polynomial(BufferFunction function, PolynomialMode mode, size_t buffer_size);

    virtual ~Polynomial() = default;

    /**
     * @brief Processes a single sample
     * @param input Input value
     * @return The next sample value from the polynomial function
     *
     * Computes the next output value based on the polynomial function and current mode.
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to generate
     * @return Vector of generated samples
     *
     * This method is more efficient than calling process_sample() repeatedly
     * when generating multiple samples at once.
     */
    virtual std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Resets the generator to its initial state
     *
     * Clears the internal buffers and resets to initial conditions.
     */
    void reset();

    /**
     * @brief Sets the polynomial coefficients (for direct mode)
     * @param coefficients Vector of polynomial coefficients (highest power first)
     *
     * Updates the polynomial function to use the specified coefficients.
     * This allows dynamically changing the polynomial during operation.
     */
    void set_coefficients(const std::vector<double>& coefficients);

    /**
     * @brief Sets a custom direct function
     * @param function Custom function that maps input to output value
     *
     * Updates the generator to use the specified custom function.
     */
    void set_direct_function(DirectFunction function);

    /**
     * @brief Sets a custom buffer function
     * @param function Function that processes a buffer of values
     * @param mode Processing mode (RECURSIVE or FEEDFORWARD)
     * @param buffer_size Number of previous values to maintain
     *
     * Updates the generator to use the specified buffer function and mode.
     */
    void set_buffer_function(BufferFunction function, PolynomialMode mode, size_t buffer_size);

    /**
     * @brief Sets initial conditions for recursive mode
     * @param initial_values Vector of initial output values
     *
     * Sets the initial values for the output buffer in recursive mode.
     * The values are ordered from newest to oldest.
     */
    void set_initial_conditions(const std::vector<double>& initial_values);

    /**
     * @brief Sets the input node to generate polynomial values from
     * @param input_node Node providing the input values
     *
     * Configures the node to receive input from another node
     */
    inline void set_input_node(std::shared_ptr<Node> input_node) { m_input_node = input_node; }

    /**
     * @brief Gets the current polynomial mode
     * @return Current polynomial mode
     */
    PolynomialMode get_mode() const { return m_mode; }

    /**
     * @brief Gets the buffer size
     * @return Current buffer size
     */
    size_t get_buffer_size() const { return m_buffer_size; }

    /**
     * @brief Gets the polynomial coefficients
     * @return Reference to the coefficient vector
     */
    const std::vector<double>& get_coefficients() const { return m_coefficients; }

    /**
     * @brief Gets the input buffer
     * @return Reference to the input buffer
     */
    const std::deque<double>& get_input_buffer() const { return m_input_buffer; }

    /**
     * @brief Gets the output buffer
     * @return Reference to the output buffer
     */
    const std::deque<double>& get_output_buffer() const { return m_output_buffer; }

    /**
     * @brief Prints a visual representation of the polynomial function
     *
     * Outputs a text-based graph of the polynomial function over a range of inputs.
     */
    inline void printGraph() override { }

    /**
     * @brief Prints the current state and parameters
     *
     * Outputs the current mode, buffer contents, and other relevant information.
     */
    inline void printCurrent() override { }

    /**
     * @brief Sets the scaling factor for the output values
     */
    inline void set_amplitude(double amplitude) override
    {
        m_scale_factor = amplitude;
    }

    /**
     * @brief Registers a callback for every generated sample
     * @param callback Function to call when a new sample is generated
     *
     * This method allows external components to monitor or react to
     * every sample produced by the sine oscillator. The callback
     * receives a GeneratorContext containing the generated value and
     * oscillator parameters like frequency, amplitude, and phase.
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback for generated samples
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback is triggered
     *
     * This method enables selective monitoring of the oscillator,
     * where callbacks are only triggered when specific conditions
     * are met. This is useful for detecting zero crossings, amplitude
     * thresholds, or other specific points in the waveform cycle.
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Removes a previously registered callback
     * @param callback The callback function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a callback previously added with on_tick(), stopping
     * it from receiving further notifications about generated samples.
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional callback
     * @param callback The condition function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a conditional callback previously added with on_tick_if(),
     * stopping it from receiving further notifications about generated samples.
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
        m_callbacks.clear();
        m_conditional_callbacks.clear();
    }

    /**
     * @brief Marks the node as registered or unregistered for processing
     * @param is_registered True to mark as registered, false to mark as unregistered
     *
     * This method is used by the node graph manager to track which nodes are currently
     * part of the active processing graph. When a node is registered, it means it's
     * included in the current processing cycle. When unregistered, it may be excluded
     * from processing to save computational resources.
     */
    inline void mark_registered_for_processing(bool is_registered) override { m_is_registered = is_registered; }

    /**
     * @brief Checks if the node is currently registered for processing
     * @return True if the node is registered, false otherwise
     *
     * This method allows checking whether the oscillator is currently part of the
     * active processing graph. Registered oscillators are included in the processing
     * cycle, while unregistered ones may be skipped.
     */
    inline bool is_registered_for_processing() const override { return m_is_registered; }

    /**
     * @brief Marks the node as processed or unprocessed in the current cycle
     * @param is_processed True to mark as processed, false to mark as unprocessed
     *
     * This method is used by the processing system to track which nodes have already
     * been processed in the current cycle. This is particularly important for oscillators
     * that may be used as modulators for multiple other nodes, to ensure they are only
     * processed once per cycle regardless of how many nodes they feed into.
     */
    inline void mark_processed(bool is_processed) override { m_is_processed = is_processed; }

    /**
     * @brief Resets the processed state of the node and any attached modulators
     *
     * This method is used by the processing system to reset the processed state
     * of the node at the end of each processing cycle. This ensures that
     * all nodes are marked as unprocessed before the next cycle begins, allowing
     * the system to correctly identify which nodes need to be processed.
     */
    void reset_processed_state() override;

    /**
     * @brief Checks if the node has been processed in the current cycle
     * @return True if the node has been processed, false otherwise
     *
     * This method allows checking whether the oscillator has already been processed
     * in the current cycle. This is used by the processing system to avoid
     * redundant processing of oscillators with multiple outgoing connections.
     */
    inline bool is_processed() const override { return m_is_processed; }

    /**
     * @brief Allows RootNode to process the Generator without using the processed sample
     * @param bMock_process True to mock process, false to process normally
     *
     * NOTE: This has no effect on the behaviour of process_sample (or process_batch).
     * This is ONLY used by the RootNode when processing the node graph.
     * If the output of the Generator needs to be ignored elsewhere, simply discard the return value.
     */
    inline void enable_mock_process(bool mock_process) override { m_mock_process = mock_process; }

    /**
     * @brief Checks if the node should mock process
     * @return True if the node should mock process, false otherwise
     */
    inline bool should_mock_process() const override { return m_mock_process; }

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
    /**
     * @brief Converts coefficient vector to a polynomial function
     * @param coefficients Vector of polynomial coefficients (highest power first)
     * @return Function that evaluates the polynomial
     *
     * Creates a function that evaluates the polynomial defined by the
     * specified coefficients.
     */
    // DirectFunction create_polynomial_function(const std::vector<double>& coefficients);

    PolynomialMode m_mode; ///< Current processing mode
    DirectFunction m_direct_function; ///< Function for direct mode
    BufferFunction m_buffer_function; ///< Function for recursive/feedforward mode
    std::vector<double> m_coefficients; ///< Polynomial coefficients (if using coefficient-based definition)
    std::deque<double> m_input_buffer; ///< Buffer of input values for feedforward mode
    std::deque<double> m_output_buffer; ///< Buffer of output values for recursive mode
    size_t m_buffer_size; ///< Maximum size of the buffers
    double m_last_output; ///< Most recent output value
    double m_scale_factor; ///< Scaling factor for output
    std::shared_ptr<Node> m_input_node; ///< Input node for processing

    /**
     * @brief Collection of standard callback functions
     *
     * Stores the registered callback functions that will be notified
     * whenever the oscillator produces a new sample. These callbacks
     * enable external components to monitor and react to the oscillator's
     * output without interrupting the generation process.
     */
    std::vector<NodeHook> m_callbacks;

    /**
     * @brief Collection of conditional callback functions with their predicates
     *
     * Stores pairs of callback functions and their associated condition predicates.
     * These callbacks are only invoked when their condition evaluates to true
     * for a generated sample, enabling selective monitoring of specific
     * waveform characteristics or sample values.
     */
    std::vector<std::pair<NodeHook, NodeCondition>> m_conditional_callbacks;

    /**
     * @brief Flag indicating whether this oscillator is registered with the node graph manager
     *
     * When true, the oscillator is part of the active processing graph and will be
     * included in the processing cycle. When false, it may be excluded from processing
     * to save computational resources.
     */
    bool m_is_registered;

    /**
     * @brief Flag indicating whether this oscillator has been processed in the current cycle
     *
     * This flag is used by the processing system to track which nodes have already
     * been processed in the current cycle, preventing redundant processing of
     * oscillators with multiple outgoing connections.
     */
    bool m_is_processed;

    /**
     * @brief Flag indicating whether this oscillator should mock process
     *
     * This flag is used by the processing system to determine whether the oscillator
     * should actually process or just mock process. This is useful for when the
     * oscillator is being used as a modulator for another node, but the output
     * of the oscillator is not needed.
     */
    bool m_mock_process;
};

} // namespace MayaFlux::Nodes::Generator
