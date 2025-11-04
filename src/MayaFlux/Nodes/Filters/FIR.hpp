#pragma once

#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

/**
 * @class FIR
 * @brief Finite Impulse Response filter implementation
 *
 * The FIR filter implements a non-recursive digital filter where the output
 * depends only on the current and past input values, with no feedback path.
 * This creates a filter with guaranteed stability and linear phase response
 * when coefficients are symmetric.
 *
 * FIR filters are characterized by the difference equation:
 * y[n] = b₀x[n] + b₁x[n-1] + ... + bₘx[n-m]
 *
 * Key properties of FIR filters:
 * - Always stable (no feedback means no potential for instability)
 * - Can have perfectly linear phase (when coefficients are symmetric)
 * - Typically require more coefficients than IIR for similar magnitude response
 * - No feedback means no resonance or self-oscillation
 *
 * Common applications include:
 * - Low/high/band-pass filtering with predictable phase response
 * - Time-domain transformations and delay-based effects
 * - Convolution-based processing (impulse responses, simulations)
 * - Windowed-sinc filters for precise frequency response
 * - Hilbert transformers and other specialized signal processors
 * - Data smoothing and noise reduction in sensor data
 * - Feature extraction in pattern recognition
 */
class MAYAFLUX_API FIR : public Filter {
public:
    /**
     * @brief Creates an FIR filter with specified order
     * @param input Source node providing input samples
     * @param zindex_shifts String in format "N_0" specifying filter order
     *
     * Creates an FIR filter with the specified input node and order.
     * For FIR filters, the second number in zindex_shifts should be 0
     * since there is no feedback path (e.g., "64_0" for a 64-tap FIR).
     * All coefficients are initialized to zero.
     */
    FIR(std::shared_ptr<Node> input, const std::string& zindex_shifts);

    /**
     * @brief Creates an FIR filter with specified coefficients
     * @param input Source node providing input samples
     * @param coeffs Vector of filter coefficients
     *
     * Creates an FIR filter with the specified input node and coefficients.
     * The coefficients directly define the filter's impulse response and
     * frequency response characteristics.
     */
    FIR(std::shared_ptr<Node> input, const std::vector<double> coeffs);

    /**
     * @brief Processes a single sample through the FIR filter
     * @param input The input sample
     * @return The filtered output sample
     *
     * Implements the FIR filtering algorithm, computing the weighted sum
     * of the current input and previous inputs according to the filter
     * coefficients. This is the core processing method called for each sample.
     */
    double process_sample(double input = 0.) override;

    void save_state() override;
    void restore_state() override;
};

}
