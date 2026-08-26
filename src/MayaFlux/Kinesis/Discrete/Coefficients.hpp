#pragma once

/**
 * @file Coefficients.hpp
 * @brief Digital filter coefficient design for MayaFlux::Kinesis
 *
 * Pure numerical functions producing difference-equation coefficient vectors.
 * No MayaFlux type dependencies. Domain-agnostic: the resulting coefficients
 * describe a recurrence over any sampled sequence, not specifically audio.
 *
 * All biquad designs follow the RBJ Audio EQ Cookbook bilinear transform and
 * are normalised so that a[0] == 1.0 on return. Coefficients are written into
 * caller-supplied vectors, which are resized to the required order.
 *
 * Convention matches the direct-form recurrence:
 *   y[n] = sum(b[i] * x[n-i]) - sum(a[i] * y[n-i]) for i >= 1
 *
 * SIMD notes:
 *   Design functions are scalar and called at configuration time, not per
 *   sample. cascade() is a small convolution and is not hot-path.
 *   max_pole_magnitude() carries an Eigen dependency confined to the .cpp.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Biquad design
// ============================================================================

/**
 * @brief Second-order lowpass coefficients
 * @param frequency   Cutoff in Hz, clamped to (0, sample_rate/2)
 * @param q           Quality factor; 0.7071 is maximally flat
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_lowpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order highpass coefficients
 * @param frequency   Cutoff in Hz, clamped to (0, sample_rate/2)
 * @param q           Quality factor; 0.7071 is maximally flat
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_highpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order bandpass coefficients, constant 0 dB peak gain
 * @param frequency   Centre frequency in Hz, clamped to (0, sample_rate/2)
 * @param q           Quality factor; higher is narrower
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_bandpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order notch coefficients
 * @param frequency   Rejection frequency in Hz, clamped to (0, sample_rate/2)
 * @param q           Quality factor; higher is narrower
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_notch(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order allpass coefficients
 *
 * Unity magnitude at all frequencies with frequency-dependent phase shift.
 * Useful for phase manipulation and dispersion without spectral change.
 *
 * @param frequency   Centre of phase transition in Hz
 * @param q           Sharpness of the phase transition
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_allpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order peaking coefficients
 * @param frequency   Centre frequency in Hz
 * @param q           Bandwidth control; higher is narrower
 * @param gain_db     Boost (positive) or cut (negative) at centre, in dB
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_peaking(double frequency, double q, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order low shelf coefficients
 * @param frequency   Shelf midpoint in Hz
 * @param slope       Shelf slope; 1.0 is the steepest without overshoot
 * @param gain_db     Shelf level relative to unity, in dB
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_low_shelf(double frequency, double slope, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b);

/**
 * @brief Second-order high shelf coefficients
 * @param frequency   Shelf midpoint in Hz
 * @param slope       Shelf slope; 1.0 is the steepest without overshoot
 * @param gain_db     Shelf level relative to unity, in dB
 * @param sample_rate Sampling rate in Hz
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 3
 */
MAYAFLUX_API void biquad_high_shelf(double frequency, double slope, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b);

// ============================================================================
// Composition
// ============================================================================

/**
 * @brief Polynomial product of two coefficient vectors
 *
 * Cascading two sections is multiplication of their transfer functions, which
 * is convolution of the numerators and of the denominators separately. Apply
 * once to the a pair and once to the b pair to collapse two sections into a
 * single higher-order recurrence.
 *
 * @param lhs First coefficient vector
 * @param rhs Second coefficient vector
 * @return Vector of length lhs.size() + rhs.size() - 1
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> cascade(
    std::span<const double> lhs, std::span<const double> rhs);

// ============================================================================
// Stability
// ============================================================================

/**
 * @brief Largest pole magnitude of a denominator polynomial
 *
 * Roots of a[0] + a[1]z^-1 + ... are found via the companion matrix
 * eigenvalues. A return value below 1.0 indicates a stable recurrence;
 * at or above 1.0 the recurrence diverges.
 *
 * Orders 1 and 2 are closed-form. Higher orders carry an Eigen dependency.
 *
 * @param a Denominator coefficients, a[0] must be non-zero
 * @return Largest |root|, or 0.0 for a constant polynomial
 */
[[nodiscard]] MAYAFLUX_API double max_pole_magnitude(std::span<const double> a);

} // namespace MayaFlux::Kinesis::Discrete
