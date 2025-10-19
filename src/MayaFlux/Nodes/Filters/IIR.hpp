#pragma once

#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

/**
 * @class IIR
 * @brief Infinite Impulse Response filter implementation
 *
 * The IIR filter implements a recursive digital filter where the output
 * depends on both current/past input values and past output values through
 * a feedback path. This creates a filter that can achieve complex frequency
 * responses with fewer coefficients than FIR filters.
 *
 * IIR filters are characterized by the difference equation:
 * y[n] = (b₀x[n] + b₁x[n-1] + ... + bₘx[n-m]) - (a₁y[n-1] + ... + aₙy[n-n])
 *
 * Key properties of IIR filters:
 * - Can become unstable if coefficients are improperly designed
 * - Non-linear phase response (phase distortion)
 * - Require fewer coefficients than FIR for similar magnitude response
 * - Feedback path enables resonance and self-reinforcing behaviors
 * - Can model analog system responses (Butterworth, Chebyshev, etc.)
 *
 * Common applications include:
 * - Efficient low/high/band-pass filtering
 * - Resonant systems for dynamic modeling
 * - Spectral shaping and frequency-domain transformations
 * - Physical modeling components (resonators, feedback systems)
 * - Adaptive filters and dynamic response systems
 * - Control systems and feedback loops
 * - Predictive modeling in time-series data
 * - Simulation of natural and mechanical systems
 */
class MAYAFLUX_API IIR : public Filter {
public:
    /**
     * @brief Creates an IIR filter with specified order
     * @param input Source node providing input samples
     * @param zindex_shifts String in format "N_M" specifying filter order
     *
     * Creates an IIR filter with the specified input node and order configuration.
     * The zindex_shifts parameter defines both feedforward and feedback paths
     * (e.g., "2_2" for a biquad filter). All coefficients are initialized to zero
     * except a[0] which is set to 1.0.
     */
    IIR(std::shared_ptr<Node> input, const std::string& zindex_shifts);

    /**
     * @brief Creates an IIR filter with specified coefficients
     * @param input Source node providing input samples
     * @param a_coef Feedback (denominator) coefficients
     * @param b_coef Feedforward (numerator) coefficients
     *
     * Creates an IIR filter with the specified input node and coefficient vectors.
     * The a_coef vector contains feedback coefficients (denominator of transfer function),
     * while b_coef contains feedforward coefficients (numerator of transfer function).
     *
     * Note: a[0] is typically normalized to 1.0, and the remaining a coefficients
     * are negated compared to the standard transfer function representation.
     */
    IIR(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef);

    /**
     * @brief Processes a single sample through the IIR filter
     * @param input The input sample
     * @return The filtered output sample
     *
     * Implements the IIR filtering algorithm, computing the weighted sum
     * of the current input, previous inputs, and previous outputs according
     * to the filter coefficients. This is the core processing method called
     * for each sample.
     */
    double process_sample(double input) override;
};

}
