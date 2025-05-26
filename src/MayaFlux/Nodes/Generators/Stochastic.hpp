#pragma once
#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

/**
 * @class StochasticContext
 * @brief Specialized context for stochastic generator callbacks
 *
 * StochasticContext extends the base NodeContext to provide detailed information
 * about a stochastic generator's current state to callbacks. It includes the
 * current distribution type, amplitude scaling, range parameters, and statistical
 * configuration values.
 *
 * This rich context enables callbacks to perform sophisticated analysis and
 * monitoring of stochastic behavior, such as:
 * - Tracking statistical properties of generated sequences
 * - Implementing adaptive responses to emergent patterns
 * - Visualizing probability distributions in real-time
 * - Creating cross-domain mappings based on stochastic properties
 * - Detecting and responding to specific statistical conditions
 */
class StochasticContext : public NodeContext {
public:
    /**
     * @brief Constructs a StochasticContext with the current generator state
     * @param value Current output sample value
     * @param type Current probability distribution algorithm
     * @param amplitude Current scaling factor for output values
     * @param range_start Lower bound of the current output range
     * @param range_end Upper bound of the current output range
     * @param normal_spread Variance parameter for normal distribution
     *
     * Creates a context object that provides a complete snapshot of the
     * stochastic generator's current state, including its most recent output
     * value and all parameters that define its statistical behavior.
     */
    StochasticContext(double value, Utils::distribution type, double amplitude,
        double range_start, double range_end, double normal_spread)
        : NodeContext(value, typeid(StochasticContext).name())
        , distribution_type(type)
        , amplitude(amplitude)
        , range_start(range_start)
        , range_end(range_end)
        , normal_spread(normal_spread)
    {
    }

    /**
     * @brief Current distribution type
     *
     * Identifies which probability algorithm is currently active in the
     * generator (uniform, normal, exponential, etc.). This determines the
     * fundamental statistical properties of the generated sequence.
     */
    Utils::distribution distribution_type;

    /**
     * @brief Current amplitude scaling factor
     */
    double amplitude;

    /**
     * @brief Current lower bound of the range
     */
    double range_start;

    /**
     * @brief Current upper bound of the range
     */
    double range_end;

    /**
     * @brief Current variance parameter for normal distribution
     */
    double normal_spread;
};

/**
 * @class NoiseEngine
 * @brief Computational stochastic signal generator with multiple probability distributions
 *
 * The NoiseEngine generates algorithmic signals based on mathematical probability
 * distributions, serving as a foundational component for generative composition,
 * procedural sound design, and data-driven audio transformation. Unlike deterministic
 * processes, stochastic generators introduce controlled mathematical randomness
 * into computational signal paths.
 *
 * Stochastic processes are fundamental in computational audio for:
 * - Procedural generation of complex timbral structures
 * - Algorithmic composition and generative music systems
 * - Data-driven environmental simulations
 * - Creating emergent sonic behaviors through probability fields
 * - Cross-domain control signal generation (audio influencing visual, haptic, etc.)
 *
 * This implementation supports multiple probability distributions:
 * - Uniform: Equal probability across the entire range
 * - Normal (Gaussian): Bell-shaped distribution centered around the midpoint
 * - Exponential: Higher probability near the start, decreasing exponentially
 *
 * The NoiseEngine can function at any rate - from audio-rate signal generation to
 * control-rate parameter modulation, to event-level algorithmic decision making.
 * It can be integrated with other computational domains (graphics, physics, data)
 * to create cross-domain generative systems.
 */
class NoiseEngine : public Generator {
public:
    /**
     * @brief Constructor for the stochastic generator
     * @param type Distribution type to use (default: uniform distribution)
     *
     * Creates a stochastic generator with the specified probability distribution.
     * The generator is initialized with entropy from the system's
     * random device for non-deterministic behavior across program executions.
     */
    NoiseEngine(Utils::distribution type = Utils::distribution::UNIFORM);

    /**
     * @brief Virtual destructor
     */
    ~NoiseEngine() = default;

    /**
     * @brief Changes the probability distribution type
     * @param type New distribution type to use
     *
     * This allows dynamic reconfiguration of the stochastic behavior,
     * enabling real-time transformation of the generator's mathematical properties.
     */
    inline void set_type(Utils::distribution type)
    {
        m_type = type;
    }

    /**
     * @brief Generates a single stochastic value
     * @param input Input value (can be used for distribution modulation)
     * @return Generated stochastic value
     *
     * This method generates a single value according to the current
     * distribution settings. The input parameter can be used to modulate
     * or influence the distribution in advanced applications.
     */
    virtual double process_sample(double input) override;

    /**
     * @brief Generates a stochastic value within a specified range
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @return Value within the specified range based on current distribution
     *
     * This method provides precise control over the output range while
     * maintaining the statistical properties of the selected distribution.
     */
    double random_sample(double start, double end);

    /**
     * @brief Generates multiple stochastic values at once
     * @param num_samples Number of values to generate
     * @return Vector of generated values
     *
     * This method efficiently generates multiple values in a single operation,
     * useful for batch processing or filling buffers.
     */
    virtual std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Generates an array of stochastic values within a specified range
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @param num_samples Number of values to generate
     * @return Vector of values within the specified range
     *
     * Generates a collection of values following the current distribution,
     * mapped to the specified numerical range.
     */
    std::vector<double> random_array(double start, double end, unsigned int num_samples);

    /**
     * @brief Visualizes the distribution characteristics
     *
     * Outputs a data visualization of the distribution's statistical properties,
     * useful for understanding the mathematical behavior of the generator.
     */
    virtual void printGraph() override;

    /**
     * @brief Outputs the current configuration parameters
     *
     * Displays the current distribution type, scaling factor, and other
     * mathematical parameters of the generator.
     */
    virtual void printCurrent() override;

    /**
     * @brief Sets the scaling factor for the output values
     * @param amplitude New scaling value
     *
     * This controls the overall magnitude of the generated values.
     * Higher values produce larger numerical outputs, while lower values
     * produce smaller numerical outputs.
     */
    inline void set_amplitude(double amplitude) override
    {
        m_amplitude = amplitude;
    }

    /**
     * @brief Sets the variance parameter for normal distribution
     * @param spread New variance value
     *
     * For normal (Gaussian) distribution, this controls the statistical
     * variance of the distribution. Higher values create greater entropy
     * with more values in the extremes, while lower values concentrate
     * values toward the center of the distribution.
     */
    inline void set_normal_spread(double spread)
    {
        m_normal_spread = spread;
    }

    /**
     * @brief Gets the current amplitude scaling factor
     * @return Current amplitude value
     *
     * Retrieves the current amplitude setting that controls the overall
     * magnitude of the generated values. Useful for monitoring or
     * adaptive systems that need to know the current scaling.
     */
    inline double get_amplitude() { return m_amplitude; }

    /**
     * @brief Registers a callback for every generated value
     * @param callback Function to call when a new value is generated
     *
     * This method allows external components to monitor or react to
     * every value produced by the stochastic generator. The callback
     * receives a NodeContext containing the generated value and
     * distribution parameters.
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback for generated values
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback is triggered
     *
     * This method enables selective monitoring of the stochastic process,
     * where callbacks are only triggered when specific statistical conditions
     * are met. This is useful for detecting emergent patterns or specific
     * value ranges in the generated sequence.
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Removes a previously registered callback
     * @param callback The callback function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a callback previously added with on_tick(), stopping
     * it from receiving further notifications about generated values.
     */
    bool remove_hook(const NodeHook& callback) override;

    /**
     * @brief Removes a previously registered conditional callback
     * @param callback The condition function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a conditional callback previously added with on_tick_if(),
     * stopping it from receiving further notifications about generated values.
     */
    bool remove_conditional_hook(const NodeCondition& callback) override;

    /**
     * @brief Removes all registered callbacks
     *
     * Clears all standard and conditional callbacks, effectively
     * disconnecting all external components from this generator's
     * notification system. Useful when reconfiguring the processing
     * graph or shutting down components.
     */
    inline void remove_all_hooks() override
    {
        m_callbacks.clear();
        m_conditional_callbacks.clear();
    }

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
    inline void reset_processed_state() override
    {
        atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    }

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
     * @param value The current generated value
     * @return A unique pointer to a StochasticContext object
     *
     * This method creates a specialized context object containing
     * the current value and all distribution parameters, providing
     * callbacks with rich information about the generator's state.
     */
    std::unique_ptr<NodeContext> create_context(double value) override;

    /**
     * @brief Notifies all registered callbacks about a new value
     * @param value The newly generated value
     *
     * This method is called internally whenever a new value is generated,
     * creating the appropriate context and invoking all registered callbacks
     * that should receive notification about this value.
     */
    void notify_tick(double value) override;

private:
    /**
     * @brief Generates a raw value according to the current distribution
     * @return Raw stochastic value before range transformation
     *
     * This internal method applies the selected probability algorithm
     * to generate a value with the appropriate statistical properties.
     */
    double generate_distributed_sample();

    /**
     * @brief Transforms a raw value to fit within the specified range
     * @param sample Raw value from the distribution
     * @param start Lower bound of the target range
     * @param end Upper bound of the target range
     * @return Transformed value within the specified range
     *
     * Different distributions require different mathematical transformations
     * to properly map their output while preserving their statistical properties.
     */
    double transform_sample(double sample, double start, double end) const;

    /**
     * @brief Validates that the specified range is mathematically valid
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @throws std::invalid_argument if the range is invalid
     *
     * Ensures that the range parameters satisfy the mathematical constraints
     * of the selected distribution algorithm.
     */
    void validate_range(double start, double end) const;

    /**
     * @brief Mersenne Twister entropy generator
     *
     * A high-quality pseudo-random number algorithm that provides
     * excellent statistical properties for computational applications.
     */
    std::mt19937 m_random_engine;

    /**
     * @brief Current probability distribution algorithm
     */
    Utils::distribution m_type;

    /**
     * @brief Lower bound of the current output range
     */
    double m_current_start;

    /**
     * @brief Upper bound of the current output range
     */
    double m_current_end;

    /**
     * @brief Scaling factor for the output values
     */
    double m_amplitude;

    /**
     * @brief Variance parameter for normal distribution
     *
     * Controls the statistical spread in normal distribution.
     * Higher values increase entropy and distribution width.
     */
    double m_normal_spread;

    /**
     * @brief Collection of standard callback functions
     *
     * Stores the registered callback functions that will be notified
     * whenever the generator produces a new value. These callbacks
     * enable external components to monitor and react to the generator's
     * activity without interrupting the generation process.
     */
    std::vector<NodeHook> m_callbacks;

    /**
     * @brief Collection of conditional callback functions with their predicates
     *
     * Stores pairs of callback functions and their associated condition predicates.
     * These callbacks are only invoked when their condition evaluates to true
     * for a generated value, enabling selective monitoring of specific
     * statistical conditions or value patterns.
     */
    std::vector<std::pair<NodeHook, NodeCondition>> m_conditional_callbacks;

    /**
     * @brief The most recent sample value generated by this oscillator
     *
     * This value is updated each time process_sample() is called and can be
     * accessed via get_last_output() without triggering additional processing.
     * It's useful for monitoring the oscillator's state and for implementing
     * feedback loops.
     */
    double m_last_output;
};
}
