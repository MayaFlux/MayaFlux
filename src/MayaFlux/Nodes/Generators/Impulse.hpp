#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class Impulse
 * @brief Impulse generator node
 *
 * The Impulse class generates a single spike followed by zeros, which is a fundamental
 * signal used in digital signal processing. It produces a value of 1.0 (or specified amplitude)
 * at specified intervals, and 0.0 elsewhere.
 *
 * Key features:
 * - Configurable frequency, amplitude, and DC offset
 * - Support for frequency modulation (changing impulse rate)
 * - Support for amplitude modulation (changing impulse height)
 * - Automatic registration with the node graph manager (optional)
 * - Precise timing control for impulse generation
 *
 * Impulse generators are used extensively in audio and signal processing for:
 * - Triggering events at specific intervals
 * - Testing system responses (impulse response)
 * - Creating click trains and metronome-like signals
 * - Serving as the basis for more complex event-based generators
 * - Synchronization signals for other generators
 *
 * The implementation uses a phase accumulation approach similar to other oscillators
 * but only outputs a non-zero value at the beginning of each cycle.
 */
class Impulse : public Generator, public std::enable_shared_from_this<Impulse> {
public:
    /**
     * @brief Basic constructor with fixed parameters
     * @param frequency Impulse repetition rate in Hz (default: 1Hz, one impulse per second)
     * @param amplitude Impulse amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: false)
     *
     * Creates an impulse generator with fixed frequency and amplitude.
     */
    Impulse(float frequency = 1, float amplitude = 1, float offset = 0, bool bAuto_register = false);

    /**
     * @brief Constructor with frequency modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param frequency Base frequency in Hz (default: 1Hz)
     * @param amplitude Impulse amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates an impulse generator with frequency modulation, where the actual frequency
     * is the base frequency plus the output of the modulator node.
     */
    Impulse(std::shared_ptr<Node> frequency_modulator, float frequency = 1, float amplitude = 1, float offset = 0, bool bAuto_register = false);

    /**
     * @brief Constructor with amplitude modulation
     * @param frequency Impulse repetition rate in Hz
     * @param amplitude_modulator Node that modulates the amplitude
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates an impulse generator with amplitude modulation, where the actual amplitude
     * is the base amplitude multiplied by the output of the modulator node.
     */
    Impulse(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude = 1, float offset = 0, bool bAuto_register = false);

    /**
     * @brief Constructor with both frequency and amplitude modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param amplitude_modulator Node that modulates the amplitude
     * @param frequency Base frequency in Hz (default: 1Hz)
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates an impulse generator with both frequency and amplitude modulation,
     * enabling complex timing and amplitude control.
     */
    Impulse(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
        float frequency = 1, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    /**
     * @brief Virtual destructor
     */
    virtual ~Impulse() = default;

    /**
     * @brief Processes a single input sample and generates an impulse sample
     * @param input Input sample (used for modulation when modulators are connected)
     * @return Generated impulse sample (1.0 at the start of each cycle, 0.0 elsewhere)
     *
     * This method advances the generator's phase and computes the next
     * sample of the impulse train, applying any modulation from connected nodes.
     */
    virtual double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to generate
     * @return Vector of generated impulse samples
     *
     * This method is more efficient than calling process_sample() repeatedly
     * when generating multiple samples at once.
     */
    virtual std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Prints a visual representation of the impulse train
     *
     * Outputs a text-based graph of the impulse pattern over time,
     * useful for debugging and visualization.
     */
    inline virtual void printGraph() override { }

    /**
     * @brief Prints the current parameters of the impulse generator
     *
     * Outputs the current frequency, amplitude, offset, and modulation
     * settings of the generator.
     */
    inline virtual void printCurrent() override { }

    /**
     * @brief Sets the generator's frequency
     * @param frequency New frequency in Hz
     *
     * Updates the generator's frequency, which controls how often impulses occur.
     */
    void set_frequency(float frequency);

    /**
     * @brief Gets the current base frequency
     * @return Current frequency in Hz
     */
    inline float get_frequency() const { return m_frequency; }

    /**
     * @brief Gets the current base amplitude
     * @return Current amplitude
     */
    inline float get_amplitude() const { return m_amplitude; }

    /**
     * @brief Sets the generator's amplitude
     * @param amplitude New amplitude
     */
    inline void set_amplitude(double amplitude) override
    {
        m_amplitude = amplitude;
    }

    /**
     * @brief Sets all basic parameters at once
     * @param frequency New frequency in Hz
     * @param amplitude New amplitude
     * @param offset New DC offset
     *
     * This is more efficient than setting parameters individually
     * when multiple parameters need to be changed.
     */
    inline void set_params(float frequency, float amplitude, float offset)
    {
        m_amplitude = amplitude;
        m_offset = offset;
        set_frequency(frequency);
    }

    /**
     * @brief Sets a node to modulate the generator's frequency
     * @param modulator Node that will modulate the frequency
     *
     * The modulator's output is added to the base frequency,
     * enabling dynamic control of impulse timing.
     */
    void set_frequency_modulator(std::shared_ptr<Node> modulator);

    /**
     * @brief Sets a node to modulate the generator's amplitude
     * @param modulator Node that will modulate the amplitude
     *
     * The modulator's output is multiplied with the base amplitude,
     * enabling dynamic control of impulse height.
     */
    void set_amplitude_modulator(std::shared_ptr<Node> modulator);

    /**
     * @brief Removes all modulation connections
     *
     * After calling this method, the generator will use only its
     * base frequency and amplitude without any modulation.
     */
    void clear_modulators();

    /**
     * @brief Resets the generator's phase and parameters
     * @param frequency New frequency in Hz (default: 1Hz)
     * @param amplitude New amplitude (default: 1.0)
     * @param offset New DC offset (default: 0)
     *
     * This method resets the generator's internal state and parameters,
     * effectively restarting it from the beginning of its cycle.
     */
    void reset(float frequency = 1, float amplitude = 1.0f, float offset = 0);

    /**
     * @brief Registers a callback for every generated sample
     * @param callback Function to call when a new sample is generated
     *
     * This method allows external components to monitor or react to
     * every sample produced by the impulse generator. The callback
     * receives a GeneratorContext containing the generated value and
     * generator parameters like frequency, amplitude, and phase.
     */
    void on_tick(NodeHook callback) override;

    /**
     * @brief Registers a conditional callback for generated samples
     * @param callback Function to call when condition is met
     * @param condition Predicate that determines when callback is triggered
     *
     * This method enables selective monitoring of the generator,
     * where callbacks are only triggered when specific conditions
     * are met. This is useful for detecting impulses or other specific
     * points in the generator cycle.
     */
    void on_tick_if(NodeHook callback, NodeCondition condition) override;

    /**
     * @brief Registers a callback for every impulse
     * @param callback Function to call when an impulse occurs
     *
     * This method allows external components to monitor or react to
     * every impulse produced by the generator. The callback
     * receives a GeneratorContext containing the generated value and
     * generator parameters like frequency, amplitude, and phase.
     */
    void on_impulse(NodeHook callback);

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
     * @brief Marks the node as registered or unregistered for processing
     * @param is_registered True to mark as registered, false to mark as unregistered
     */
    inline void mark_registered_for_processing(bool is_registered) override { m_is_registered = is_registered; }

    /**
     * @brief Checks if the node is currently registered for processing
     * @return True if the node is registered, false otherwise
     */
    inline bool is_registered_for_processing() const override { return m_is_registered; }

    /**
     * @brief Marks the node as processed or unprocessed in the current cycle
     * @param is_processed True to mark as processed, false to mark as unprocessed
     */
    inline void mark_processed(bool is_processed) override { m_is_processed = is_processed; }

    /**
     * @brief Resets the processed state of the node and any attached modulators
     */
    void reset_processed_state() override;

    /**
     * @brief Checks if the node has been processed in the current cycle
     * @return True if the node has been processed, false otherwise
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
     * @brief Retrieves the most recent output value produced by the generator
     * @return The last generated impulse sample
     */
    inline double get_last_output() override { return m_last_output; }

protected:
    /**
     * @brief Creates a context object for callbacks
     * @param value The current generated sample
     * @return A unique pointer to a GeneratorContext object
     *
     * This method creates a specialized context object containing
     * the current sample value and all generator parameters, providing
     * callbacks with rich information about the generator's state.
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
     * @brief Phase increment per sample
     *
     * This value determines how much the phase advances with each sample,
     * controlling the generator's frequency.
     */
    double m_phase_inc;

    /**
     * @brief Current phase of the generator
     *
     * The phase represents the current position in the impulse cycle,
     * ranging from 0 to 1.
     */
    double m_phase;

    /**
     * @brief Base amplitude of the generator
     */
    float m_amplitude;

    /**
     * @brief Base frequency of the generator in Hz
     */
    float m_frequency;

    /**
     * @brief DC offset added to the output
     */
    float m_offset;

    /**
     * @brief Node that modulates the frequency
     */
    std::shared_ptr<Node> m_frequency_modulator;

    /**
     * @brief Node that modulates the amplitude
     */
    std::shared_ptr<Node> m_amplitude_modulator;

    /**
     * @brief Updates the phase increment based on a new frequency
     * @param frequency New frequency in Hz
     *
     * This method calculates the phase increment needed to produce
     * impulses at the specified frequency at the current sample rate.
     */
    void update_phase_increment(float frequency);

    /**
     * @brief Collection of standard callback functions
     */
    std::vector<NodeHook> m_callbacks;

    /**
     * @brief Collection of impulse-specific callback functions
     */
    std::vector<NodeHook> m_impulse_callbacks;

    /**
     * @brief Collection of conditional callback functions with their predicates
     */
    std::vector<std::pair<NodeHook, NodeCondition>> m_conditional_callbacks;

    /**
     * @brief Flag indicating whether this generator is registered with the node graph manager
     */
    bool m_is_registered;

    /**
     * @brief Flag indicating whether this generator has been processed in the current cycle
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

    /**
     * @brief The most recent sample value generated by this generator
     */
    double m_last_output;

    bool m_impulse_occurred;
};
}
