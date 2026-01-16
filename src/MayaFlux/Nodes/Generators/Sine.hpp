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
class MAYAFLUX_API Sine : public Generator {
public:
    /**
     * @brief Basic constructor with fixed parameters
     * @param frequency Oscillation frequency in Hz (default: 440Hz, A4 note)
     * @param amplitude Output amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     *
     * Creates a sine oscillator with fixed frequency and amplitude.
     */
    Sine(float frequency = 440, double amplitude = 1, float offset = 0);

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
    Sine(const std::shared_ptr<Node>& frequency_modulator, float frequency = 440, double amplitude = 1, float offset = 0);

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
    Sine(float frequency, const std::shared_ptr<Node>& amplitude_modulator, double amplitude = 1, float offset = 0);

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
    Sine(const std::shared_ptr<Node>& frequency_modulator, const std::shared_ptr<Node>& amplitude_modulator,
        float frequency = 440, double amplitude = 1, float offset = 0);

    /**
     * @brief Virtual destructor
     */
    ~Sine() override = default;

    /**
     * @brief Processes a single input sample and generates a sine wave sample
     * @param input Input sample (used for modulation when modulators are connected)
     * @return Generated sine wave sample
     *
     * This method advances the oscillator's phase and computes the next
     * sample of the sine wave, applying any modulation from connected nodes.
     */
    double process_sample(double input = 0.) override;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to generate
     * @return Vector of generated sine wave samples
     *
     * This method is more efficient than calling process_sample() repeatedly
     * when generating multiple samples at once.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Prints a visual representation of the sine wave
     *
     * Outputs a text-based graph of the sine wave's shape over time,
     * useful for debugging and visualization.
     */
    void printGraph() override;

    /**
     * @brief Prints the current parameters of the sine oscillator
     *
     * Outputs the current frequency, amplitude, offset, and modulation
     * settings of the oscillator.
     */
    void printCurrent() override;

    /**
     * @brief Sets the oscillator's frequency
     * @param frequency New frequency in Hz
     *
     * Updates the oscillator's frequency while maintaining phase continuity
     * to prevent clicks or pops in the audio output.
     */
    void set_frequency(float frequency) override;

    /**
     * @brief Gets the current base frequency
     * @return Current frequency in Hz
     */
    [[nodiscard]] inline float get_frequency() const { return m_frequency; }

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
     * @brief Sets a node to modulate the oscillator's frequency
     * @param modulator Node that will modulate the frequency
     *
     * The modulator's output is added to the base frequency,
     * enabling FM synthesis techniques.
     */
    void set_frequency_modulator(const std::shared_ptr<Node>& modulator);

    /**
     * @brief Sets a node to modulate the oscillator's amplitude
     * @param modulator Node that will modulate the amplitude
     *
     * The modulator's output is multiplied with the base amplitude,
     * enabling AM synthesis techniques.
     */
    void set_amplitude_modulator(const std::shared_ptr<Node>& modulator);

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
    void reset(float frequency = 440, double amplitude = 0.5, float offset = 0);

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

private:
    /**
     * @brief Phase increment per sample
     *
     * This value determines how much the phase advances with each sample,
     * controlling the oscillator's frequency.
     */
    double m_phase_inc {};

    /**
     * @brief DC offset added to the output
     */
    float m_offset {};

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
    void update_phase_increment(double frequency);

    double m_saved_phase {};
    float m_saved_frequency {};
    float m_saved_offset {};
    double m_saved_phase_inc {};
    double m_saved_last_output {};
};
}
