#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Filters {

/**
 * @brief Parses a string representation of filter order into input/output shift configuration
 * @param str String in format "N_M" where N is input order and M is output order
 * @return Pair of integers representing input and output shift values
 *
 * This utility function converts a simple string representation of filter order
 * into the corresponding shift configuration for the filter's internal buffers.
 */
std::pair<int, int> shift_parser(const std::string& str);

/**
 * @class Filter
 * @brief Base class for computational signal transformers implementing difference equations
 *
 * The Filter class provides a comprehensive framework for implementing digital
 * transformations based on difference equations. It transcends traditional audio
 * filtering concepts, offering a flexible foundation for:
 *
 * - Complex resonant systems for generative synthesis
 * - Computational physical modeling components
 * - Recursive signal transformation algorithms
 * - Data-driven transformation design from frequency responses
 * - Dynamic coefficient modulation for emergent behaviors
 *
 * At its core, the Filter implements the general difference equation:
 * y[n] = (b₀x[n] + b₁x[n-1] + ... + bₘx[n-m]) - (a₁y[n-1] + ... + aₙy[n-n])
 *
 * Where:
 * - x[n] are input values
 * - y[n] are output values
 * - b coefficients apply to input values (feedforward)
 * - a coefficients apply to previous output values (feedback)
 *
 * This mathematical structure can represent virtually any linear time-invariant system,
 * making it a powerful tool for signal transformation across domains. The same
 * equations can process audio, control data, or even be applied to visual or
 * physical simulation parameters.
 */
class Filter : public Node {
protected:
    /**
     * @brief Input node providing samples to filter
     *
     * The filter processes samples from this node, allowing filters
     * to be chained together or connected to generators.
     */
    std::shared_ptr<Node> inputNode;

    /**
     * @brief Configuration for input and output buffer sizes
     *
     * Defines how many past input and output samples are stored,
     * based on the filter's order.
     */
    std::pair<int, int> shift_config;

    /**
     * @brief Buffer storing previous input samples
     *
     * Maintains a history of input samples needed for the filter's
     * feedforward path (b coefficients).
     */
    std::vector<double> input_history;

    /**
     * @brief Buffer storing previous output samples
     *
     * Maintains a history of output samples needed for the filter's
     * feedback path (a coefficients).
     */
    std::vector<double> output_history;

    /**
     * @brief Feedback (denominator) coefficients
     *
     * The 'a' coefficients in the difference equation, applied to
     * previous output samples. a[0] is typically normalized to 1.0.
     */
    std::vector<double> coef_a;

    /**
     * @brief Feedforward (numerator) coefficients
     *
     * The 'b' coefficients in the difference equation, applied to
     * current and previous input samples.
     */
    std::vector<double> coef_b;

    /**
     * @brief Overall gain factor applied to the filter output
     *
     * Provides a simple way to adjust the filter's output level
     * without changing its frequency response characteristics.
     */
    double gain = 1.0;

    /**
     * @brief Flag to bypass filter processing
     *
     * When enabled, the filter passes input directly to output
     * without applying any filtering.
     */
    bool bypass_enabled = false;

public:
    /**
     * @brief Constructor using string-based filter configuration
     * @param input Source node providing input samples
     * @param zindex_shifts String in format "N_M" specifying filter order
     *
     * Creates a filter with the specified input node and order configuration.
     * The zindex_shifts parameter provides a simple way to define filter order
     * using a string like "2_2" for a biquad filter.
     */
    Filter(std::shared_ptr<Node> input, const std::string& zindex_shifts);

    /**
     * @brief Constructor using explicit coefficient vectors
     * @param input Source node providing input samples
     * @param a_coef Feedback (denominator) coefficients
     * @param b_coef Feedforward (numerator) coefficients
     *
     * Creates a filter with the specified input node and coefficient vectors.
     * This allows direct specification of filter coefficients for precise
     * control over filter behavior.
     */
    Filter(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef);

    /**
     * @brief Virtual destructor
     */
    virtual ~Filter() = default;

    /**
     * @brief Gets the current processing latency of the filter
     * @return Latency in samples
     *
     * The latency is determined by the maximum of the input and output
     * buffer sizes, representing how many samples of delay the filter introduces.
     */
    inline int get_current_latency() const
    {
        return std::max(shift_config.first, shift_config.second);
    }

    /**
     * @brief Gets the current shift configuration
     * @return Pair of integers representing input and output shift values
     *
     * Returns the current configuration for input and output buffer sizes.
     */
    inline std::pair<int, int> get_current_shift() const
    {
        return shift_config;
    }

    /**
     * @brief Updates the filter's shift configuration
     * @param zindex_shifts String in format "N_M" specifying new filter order
     *
     * Reconfigures the filter with a new order specification, resizing
     * internal buffers as needed.
     */
    inline void set_shift(std::string& zindex_shifts)
    {
        shift_config = shift_parser(zindex_shifts);
        initialize_shift_buffers();
    }

    /**
     * @brief Updates filter coefficients
     * @param new_coefs New coefficient values
     * @param type Which set of coefficients to update (input, output, or both)
     *
     * Provides a flexible way to update filter coefficients, allowing
     * dynamic modification of filter characteristics during processing.
     */
    void set_coefs(const std::vector<double>& new_coefs, Utils::coefficients type = Utils::coefficients::ALL);

    /**
     * @brief Updates coefficients from another node's output
     * @param length Number of coefficients to update
     * @param source Node providing coefficient values
     * @param type Which set of coefficients to update (input, output, or both)
     *
     * Enables cross-domain interaction by deriving transformation coefficients from
     * another node's output. This creates dynamic relationships between different
     * parts of the computational graph, allowing one signal path to influence
     * the behavior of another - perfect for generative systems where parameters
     * evolve based on the system's own output.
     */
    void update_coefs_from_node(int lenght, std::shared_ptr<Node> source, Utils::coefficients type = Utils::coefficients::ALL);

    /**
     * @brief Updates coefficients from the filter's own input
     * @param length Number of coefficients to update
     * @param type Which set of coefficients to update (input, output, or both)
     *
     * Creates a self-modifying transformation where the input signal itself
     * influences the transformation characteristics. This enables complex
     * emergent behaviors and feedback systems where the signal's own properties
     * determine how it will be processed, leading to evolving, non-linear responses.
     */
    void update_coef_from_input(int lenght, Utils::coefficients type = Utils::coefficients::ALL);

    /**
     * @brief Modifies a specific coefficient
     * @param index Index of the coefficient to modify
     * @param value New value for the coefficient
     * @param type Which set of coefficients to update (input, output, or both)
     *
     * Allows precise control over individual coefficients, useful for
     * fine-tuning filter behavior or implementing parameter automation.
     */
    void add_coef(int index, double value, Utils::coefficients type = Utils::coefficients::ALL);

    /**
     * @brief Resets the filter's internal state
     *
     * Clears the input and output history buffers, effectively
     * resetting the filter to its initial state. This is useful
     * when switching between audio segments to prevent artifacts.
     */
    virtual void reset();

    /**
     * @brief Sets the filter's output gain
     * @param new_gain New gain value
     *
     * Adjusts the overall output level of the filter without
     * changing its frequency response characteristics.
     */
    inline void set_gain(double new_gain) { gain = new_gain; }

    /**
     * @brief Gets the current gain value
     * @return Current gain value
     */
    inline double get_gain() const { return gain; }

    /**
     * @brief Enables or disables filter bypass
     * @param enable True to enable bypass, false to disable
     *
     * When bypass is enabled, the filter passes input directly to output
     * without applying any filtering, useful for A/B testing or
     * temporarily disabling filters.
     */
    inline void set_bypass(bool enable) { bypass_enabled = enable; }

    /**
     * @brief Checks if bypass is currently enabled
     * @return True if bypass is enabled, false otherwise
     */
    inline bool is_bypass_enabled() const { return bypass_enabled; }

    /**
     * @brief Gets the filter's order
     * @return Maximum order of the filter
     *
     * The filter order is determined by the maximum of the input and output
     * coefficient counts minus one, representing the highest power of z⁻¹
     * in the filter's transfer function.
     */
    inline int get_order() const { return std::max(coef_a.size() - 1, coef_b.size() - 1); }

    /**
     * @brief Gets the input history buffer
     * @return Constant reference to the input history vector
     *
     * Provides access to the filter's internal input history buffer,
     * useful for analysis and visualization.
     */
    inline const std::vector<double>& get_input_history() const { return input_history; }

    /**
     * @brief Gets the output history buffer
     * @return Constant reference to the output history vector
     *
     * Provides access to the filter's internal output history buffer,
     * useful for analysis and visualization.
     */
    inline const std::vector<double>& get_output_history() const { return output_history; }

    /**
     * @brief Normalizes filter coefficients
     * @param type Which set of coefficients to normalize (input, output, or both)
     *
     * Scales coefficients to ensure a[0] = 1.0 and/or maintain consistent
     * gain at DC or Nyquist, depending on the filter type.
     */
    void normalize_coefficients(Utils::coefficients type = Utils::coefficients::ALL);

    /**
     * @brief Calculates the complex frequency response at a specific frequency
     * @param frequency Frequency in Hz to analyze
     * @param sample_rate Sample rate in Hz
     * @return Complex frequency response (magnitude and phase)
     *
     * Computes the transformation's complex response at the specified frequency.
     * This mathematical analysis can be used for visualization, further algorithmic
     * processing, or to inform cross-domain mappings between audio properties
     * and other computational parameters.
     */
    std::complex<double> get_frequency_response(double frequency, double sample_rate) const;

    /**
     * @brief Calculates the magnitude response at a specific frequency
     * @param frequency Frequency in Hz to analyze
     * @param sample_rate Sample rate in Hz
     * @return Magnitude response in linear scale
     *
     * Computes the filter's magnitude response at the specified frequency,
     * representing how much the filter amplifies or attenuates that frequency.
     */
    double get_magnitude_response(double frequency, double sample_rate) const;

    /**
     * @brief Calculates the phase response at a specific frequency
     * @param frequency Frequency in Hz to analyze
     * @param sample_rate Sample rate in Hz
     * @return Phase response in radians
     *
     * Computes the filter's phase response at the specified frequency,
     * representing the phase shift introduced by the filter at that frequency.
     */
    double get_phase_response(double frequency, double sample_rate) const;

    /**
     * @brief Calculates the phase response at a specific frequency
     * @param frequency Frequency in Hz to analyze
     * @param sample_rate Sample rate in Hz
     * @return Phase response in radians
     *
     * Computes the filter's phase response at the specified frequency,
     * representing the phase shift introduced by the filter at that frequency.
     */
    std::vector<double> processFull(unsigned int num_samples) override;

protected:
    /**
     * @brief Updates the feedback (denominator) coefficients
     * @param new_coefs New coefficient values
     *
     * Sets the 'a' coefficients in the difference equation, which
     * are applied to previous output samples. The method ensures
     * proper normalization and buffer sizing.
     */
    void setACoefficients(const std::vector<double>& new_coefs);

    /**
     * @brief Updates the feedforward (numerator) coefficients
     * @param new_coefs New coefficient values
     *
     * Sets the 'b' coefficients in the difference equation, which
     * are applied to current and previous input samples. The method
     * ensures proper buffer sizing.
     */
    void setBCoefficients(const std::vector<double>& new_coefs);

    /**
     * @brief Gets the feedback (denominator) coefficients
     * @return Constant reference to the 'a' coefficient vector
     *
     * Provides access to the filter's feedback coefficients for
     * analysis and visualization.
     */
    inline const std::vector<double>& getACoefficients() const { return coef_a; }

    /**
     * @brief Gets the feedforward (numerator) coefficients
     * @return Constant reference to the 'b' coefficient vector
     *
     * Provides access to the filter's feedforward coefficients for
     * analysis and visualization.
     */
    inline const std::vector<double>& getBCoefficients() const { return coef_b; }

    /**
     * @brief Modifies a specific coefficient in a coefficient buffer
     * @param index Index of the coefficient to modify
     * @param value New value for the coefficient
     * @param buffer Reference to the coefficient buffer to modify
     *
     * Internal implementation for adding or modifying a coefficient
     * in either the 'a' or 'b' coefficient vectors.
     */
    void add_coef_internal(u_int64_t index, double value, std::vector<double>& buffer);

    /**
     * @brief Initializes the input and output history buffers
     *
     * Resizes the history buffers based on the current shift configuration
     * and initializes them to zero. Called during construction and when
     * the filter configuration changes.
     */
    virtual void initialize_shift_buffers();

    /**
     * @brief Processes a single sample through the filter
     * @param input The input sample
     * @return The filtered output sample
     *
     * This is the core processing method that implements the difference
     * equation for a single sample. It must be implemented by derived
     * filter classes to define their specific filtering behavior.
     */
    virtual double process_sample(double input) override = 0;

    /**
     * @brief Updates the input history buffer with a new sample
     * @param current_sample The new input sample
     *
     * Shifts the input history buffer and adds the new sample at the
     * beginning. This maintains the history of input samples needed
     * for the filter's feedforward path.
     */
    virtual void update_inputs(double current_sample);

    /**
     * @brief Updates the output history buffer with a new sample
     * @param current_sample The new output sample
     *
     * Shifts the output history buffer and adds the new sample at the
     * beginning. This maintains the history of output samples needed
     * for the filter's feedback path.
     */
    virtual void update_outputs(double current_sample);
};
}
