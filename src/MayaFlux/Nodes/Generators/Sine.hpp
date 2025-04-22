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
 * - Automatic registration with the node graph manager (optional)
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
class Sine : public Generator, public std::enable_shared_from_this<Sine> {
public:
    /**
     * @brief Basic constructor with fixed parameters
     * @param frequency Oscillation frequency in Hz (default: 440Hz, A4 note)
     * @param amplitude Output amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: false)
     *
     * Creates a sine oscillator with fixed frequency and amplitude.
     */
    Sine(float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = false);

    /**
     * @brief Constructor with frequency modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param frequency Base frequency in Hz (default: 440Hz)
     * @param amplitude Output amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates a sine oscillator with frequency modulation, where the actual frequency
     * is the base frequency plus the output of the modulator node.
     */
    Sine(std::shared_ptr<Node> frequency_modulator, float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    /**
     * @brief Constructor with amplitude modulation
     * @param frequency Oscillation frequency in Hz
     * @param amplitude_modulator Node that modulates the amplitude
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates a sine oscillator with amplitude modulation, where the actual amplitude
     * is the base amplitude multiplied by the output of the modulator node.
     */
    Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    /**
     * @brief Constructor with both frequency and amplitude modulation
     * @param frequency_modulator Node that modulates the frequency
     * @param amplitude_modulator Node that modulates the amplitude
     * @param frequency Base frequency in Hz (default: 440Hz)
     * @param amplitude Base amplitude (default: 1.0)
     * @param offset DC offset added to the output (default: 0.0)
     * @param bAuto_register Whether to automatically register with the node graph manager (default: true)
     *
     * Creates a sine oscillator with both frequency and amplitude modulation,
     * enabling complex synthesis techniques like FM and AM simultaneously.
     */
    Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
        float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    /**
     * @brief Common setup code for all constructors
     * @param bAuto_register Whether to automatically register with the node graph manager
     *
     * Initializes the oscillator's internal state and registers it with
     * the node graph manager if requested.
     */
    void Setup(bool bAuto_register);

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
    virtual std::vector<double> processFull(unsigned int num_samples) override;

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
     * @brief Registers this oscillator with the default node graph manager
     *
     * This method allows delayed registration with the node graph manager
     * when the oscillator wasn't registered at construction time.
     */
    void register_to_defult();

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
};
}
