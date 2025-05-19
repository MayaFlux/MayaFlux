#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class Sine
 * @brief Sinusoidal oscillator generator node
 *
 * The Sine class generates a sinusoidal waveform, which is the fundamental
 * building block of audio synthesis. Despite its name, this class implements
 * a general sinusoidal oscillator that can be extended to produce various
 * waveforms beyond just the mathematical sine function.
 *
 * Key features:
 * - Configurable frequency, amplitude, and DC offset
 * - Support for frequency modulation (FM synthesis)
 * - Support for amplitude modulation (AM synthesis)
 * - Phase continuity to prevent clicks when changing parameters
 *
 * Sinusoidal oscillators are used extensively in audio synthesis for:
 * - Creating pure tones
 * - Serving as carriers or modulators in FM/AM synthesis
 * - Building more complex waveforms through additive synthesis
 * - LFOs (Low Frequency Oscillators) for parameter modulation
 *
 * The implementation uses a phase accumulation approach for sample-accurate
 * frequency control and efficient computation, avoiding repeated calls to
 * the computationally expensive sin() function.
 */
class Sine : public Generator {
public:
    /**
     * @brief Basic constructor with fixed parameters
     * @param frequency Oscillation frequency in Hz (default: 440Hz, A4 note)
     * @param amplitude Output amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a sine oscillator with fixed frequency and amplitude.
     */
    Sine(float frequency = 440, float amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with frequency modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param frequency Base frequency in Hz (default: 440Hz)
     * @param amplitude Output amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a sine oscillator with frequency modulation, where the actual frequency
     * is the base frequency plus the output of the modulator node.
     */
    Sine(std::shared_ptr<Node> frequency_modulator, float frequency = 440, float amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with amplitude modulation
     * @param frequency Oscillation frequency in Hz
     * @param amplitude_modulator Node that modulates the amplitude
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a sine oscillator with amplitude modulation, where the actual amplitude
     * is the base amplitude multiplied by the output of the modulator node.
     */
    Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with both frequency and amplitude modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param amplitude_modulator Node that modulates the amplitude
     * @param frequency Base frequency in Hz (default: 440Hz)
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a sine oscillator with both frequency and amplitude modulation,
     * enabling complex synthesis techniques like FM and AM simultaneously.
     */
    Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
        float frequency = 440, float amplitude = 1, float offset = 0);

    /**
     * @brief Virtual destructor
     */
    virtual ~Sine() = default;

    /**
     * @brief Processes a single input sample and generates a sine wave sample
     * @param input Input sample (used for modulation when modulators are connected)
     * @return Generated sine wave sample
     *
     * This method advances the oscillator's phase and computes the next
     * sample of the sine wave, applying any modulation from connected nodes.
     */
    virtual double process_sample(double input) override;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to generate
     * @return Vector of generated sine wave samples
     *
     * This method is more efficient than calling process_sample() repeatedly
     * when generating multiple samples at once.
     */
    virtual std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Prints a visual representation of the sine wave
     *
     * Outputs a text-based graph of the sine wave's shape over time,
     * useful for debugging and visualization.
     */
    virtual void printGraph() override;

    /**
     * @brief Prints the current parameters of the sine oscillator
     *
     * Outputs the current frequency, amplitude, offset, and modulation
     * settings of the oscillator.
     */
    virtual void printCurrent() override;

    /**
     * @brief Sets the oscillator's frequency
     * @param frequency New frequency in Hz
     *
     * Updates the oscillator's frequency while maintaining phase continuity
     * to prevent clicks or pops in the audio output.
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
     * @brief Sets the oscillator's amplitude
     * @param amplitude New amplitude
     */
    inline void set_amplitude(float amplitude)
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
     * @brief Sets a node to modulate the oscillator's frequency
     * @param modulator Node that will modulate the frequency
     *
     * The modulator's output is added to the base frequency,
     * enabling FM synthesis techniques.
     */
    void set_frequency_modulator(std::shared_ptr<Node> modulator);

    /**
     * @brief Sets a node to modulate the oscillator's amplitude
     * @param modulator Node that will modulate the amplitude
     *
     * The modulator's output is multiplied with the base amplitude,
     * enabling AM synthesis techniques.
     */
    void set_amplitude_modulator(std::shared_ptr<Node> modulator);

    /**
     * @brief Removes all modulation connections
     *
     * After calling this method, the oscillator will use only its
     * base frequency and amplitude without any modulation.
     */
    void clear_modulators();

    /**
     * @brief Resets the oscillator's phase and parameters
     * @param frequency New frequency in Hz (default: 440Hz)
     * @param amplitude New amplitude (default: 0.5)
     * @param offset New DC offset (default: 0)
     *
     * This method resets the oscillator's internal state and parameters,
     * effectively restarting it from the beginning of its cycle.
     */
    void reset(float frequency = 440, float amplitude = 0.5f, float offset = 0);

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
     * @brief Phase increment per sample
     *
     * This value determines how much the phase advances with each sample,
     * controlling the oscillator's frequency.
     */
    double m_phase_inc;

    /**
     * @brief Current phase of the oscillator
     *
     * The phase represents the current position in the sine wave cycle,
     * ranging from 0 to 2π.
     */
    double m_phase;

    /**
     * @brief Base amplitude of the oscillator
     */
    float m_amplitude;

    /**
     * @brief Base frequency of the oscillator in Hz
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
     * the specified frequency at the current sample rate.
     */
    void update_phase_increment(float frequency);

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
