#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class Phasor
 * @brief Phase ramp generator node
 *
 * The Phasor class generates a linearly increasing ramp from 0 to 1 that wraps around,
 * creating a sawtooth-like waveform. This is a fundamental building block for many
 * synthesis techniques and can be used to derive other waveforms.
 *
 * Key features:
 * - Configurable frequency, amplitude, and DC offset
 * - Support for frequency modulation (changing ramp rate)
 * - Support for amplitude modulation (changing ramp height)
 * - Automatic registration with the node graph manager (optional)
 * - Precise phase control for synchronization with other generators
 *
 * Phasor generators are used extensively in audio and signal processing for:
 * - Creating time-based effects and modulations
 * - Serving as the basis for other waveforms (triangle, pulse, etc.)
 * - Synchronizing multiple oscillators
 * - Driving sample playback positions
 *
 * The implementation uses a simple phase accumulation approach that ensures
 * sample-accurate timing and smooth transitions.
 */
class MAYAFLUX_API Phasor : public Generator, public std::enable_shared_from_this<Phasor> {
public:
    /**
     * @brief Basic constructor with fixed parameters
     * @param frequency Phasor cycle rate in Hz (default: 1Hz, one cycle per second)
     * @param amplitude Phasor amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a phasor generator with fixed frequency and amplitude.
     */
    Phasor(float frequency = 1, double amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with frequency modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param frequency Base frequency in Hz (default: 1Hz)
     * @param amplitude Phasor amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a phasor generator with frequency modulation, where the actual frequency
     * is the base frequency plus the output of the modulator node.
     */
    Phasor(const std::shared_ptr<Node>& frequency_modulator, float frequency = 1, double amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with amplitude modulation
     * @param frequency Phasor cycle rate in Hz
     * @param amplitude_modulator Node that modulates the amplitude
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a phasor generator with amplitude modulation, where the actual amplitude
     * is the base amplitude multiplied by the output of the modulator node.
     */
    Phasor(float frequency, const std::shared_ptr<Node>& amplitude_modulator, double amplitude = 1, float offset = 0);

    /**
     * @brief Constructor with both frequency and amplitude modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param amplitude_modulator Node that modulates the amplitude
     * @param frequency Base frequency in Hz (default: 1Hz)
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a phasor generator with both frequency and amplitude modulation,
     * enabling complex timing and amplitude control.
     */
    Phasor(const std::shared_ptr<Node>& frequency_modulator, const std::shared_ptr<Node>& amplitude_modulator,
        float frequency = 1, double amplitude = 1, float offset = 0);

    /**
     * @brief Virtual destructor
     */
    ~Phasor() override = default;

    /**
     * @brief Processes a single input sample and generates a phasor sample
     * @param input Input sample (used for modulation when modulators are connected)
     * @return Generated phasor sample (ramp from 0 to amplitude)
     *
     * This method advances the generator's phase and computes the next
     * sample of the phasor ramp, applying any modulation from connected nodes.
     */
    double process_sample(double input = 0.) override;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to generate
     * @return Vector of generated phasor samples
     *
     * This method is more efficient than calling process_sample() repeatedly
     * when generating multiple samples at once.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Prints a visual representation of the phasor waveform
     *
     * Outputs a text-based graph of the phasor pattern over time,
     * useful for debugging and visualization.
     */
    inline void printGraph() override { }

    /**
     * @brief Prints the current parameters of the phasor generator
     *
     * Outputs the current frequency, amplitude, offset, and modulation
     * settings of the generator.
     */
    inline void printCurrent() override { }

    /**
     * @brief Sets the generator's frequency
     * @param frequency New frequency in Hz
     *
     * Updates the generator's frequency, which controls how fast the phase ramps.
     */
    void set_frequency(float frequency);

    /**
     * @brief Gets the current base frequency
     * @return Current frequency in Hz
     */
    inline float get_frequency() const { return m_frequency; }

    /**
     * @brief Sets all basic parameters at once
     * @param frequency New frequency in Hz
     * @param amplitude New amplitude
     * @param offset New DC offset
     *
     * This is more efficient than setting parameters individually
     * when multiple parameters need to be changed.
     */
    inline void set_params(float frequency, double amplitude, float offset)
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
     * enabling dynamic control of phasor rate.
     */
    void set_frequency_modulator(const std::shared_ptr<Node>& modulator);

    /**
     * @brief Sets a node to modulate the generator's amplitude
     * @param modulator Node that will modulate the amplitude
     *
     * The modulator's output is multiplied with the base amplitude,
     * enabling dynamic control of phasor height.
     */
    void set_amplitude_modulator(const std::shared_ptr<Node>& modulator);

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
     * @param phase Initial phase value (default: 0.0)
     *
     * This method resets the generator's internal state and parameters,
     * effectively restarting it from the specified phase.
     */
    void reset(float frequency = 1, float amplitude = 1.0, float offset = 0, double phase = 0.0);

    /**
     * @brief Sets the current phase of the phasor
     * @param phase New phase value (0.0 to 1.0)
     *
     * This allows direct control of the phasor's position in its cycle,
     * useful for synchronizing with other generators or external events.
     */
    inline void set_phase(double phase)
    {
        m_phase = phase;
        // Keep phase in [0, 1) range
        while (m_phase >= 1.0)
            m_phase -= 1.0;
        while (m_phase < 0.0)
            m_phase += 1.0;
    }

    /**
     * @brief Gets the current phase of the phasor
     * @return Current phase (0.0 to 1.0)
     */
    inline double get_phase() const { return m_phase; }

    /**
     * @brief Registers a callback for every phase wrap
     * @param callback Function to call when the phase wraps
     *
     * This method allows external components to monitor or react to
     * every time the phasor completes a cycle and wraps back to 0.
     * The callback receives a GeneratorContext containing the generated
     * value and generator parameters like frequency, amplitude, and phase.
     */
    void on_phase_wrap(const NodeHook& callback);

    /**
     * @brief Registers a callback for every time the phasor crosses a threshold
     * @param callback Function to call when the threshold is crossed
     * @param threshold Value at which to trigger the callback
     * @param rising Whether to trigger on rising (true) or falling (false) edges
     *
     * This method allows external components to monitor or react to
     * every time the phasor crosses a specific threshold value. The callback
     * receives a GeneratorContext containing the generated value and
     * generator parameters like frequency, amplitude, and phase.
     */
    void on_threshold(const NodeHook& callback, double threshold, bool rising = true);

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
        m_phase_wrap_callbacks.clear();
        m_threshold_callbacks.clear();
    }

    void save_state() override;
    void restore_state() override;

protected:
    /**
     * @brief Notifies all registered callbacks about a new sample
     * @param value The newly generated sample
     *
     * This method is called internally whenever a new sample is generated,
     * creating the appropriate context and invoking all registered callbacks
     * that should receive notification about this sample.
     */
    void notify_tick(double value) override;

    /**
     * @brief Removes a previously registered phase wrap callback
     * @param callback The callback function to remove
     * @return True if the callback was found and removed, false otherwise
     *
     * Unregisters a callback previously added with on_phase_wrap(),
     * stopping it from receiving further notifications about phase wraps.
     */
    bool remove_threshold_callback(const NodeHook& callback);

private:
    /**
     * @brief Phase increment per sample
     *
     * This value determines how much the phase advances with each sample,
     * controlling the generator's frequency.
     */
    double m_phase_inc {};

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
     * a complete cycle at the specified frequency at the current sample rate.
     */
    void update_phase_increment(double frequency);

    /**
     * @brief Collection of phase wrap-specific callback functions
     */
    std::vector<NodeHook> m_phase_wrap_callbacks;

    /**
     * @brief Collection of threshold-specific callback functions with their thresholds
     */
    std::vector<std::pair<NodeHook, double>> m_threshold_callbacks;

    /**
     * @brief Flag indicating whether the phase has wrapped in the current sample
     */
    bool m_phase_wrapped;

    /**
     * @brief Flag indicating whether the threshold has been crossed in the current sample
     */
    bool m_threshold_crossed;

    double m_saved_phase {};
    float m_saved_frequency {};
    float m_saved_offset {};
    double m_saved_phase_inc {};
    double m_saved_last_output {};

    bool m_state_saved {};
};
}
