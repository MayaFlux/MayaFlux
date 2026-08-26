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
// Windowed-sinc FIR design
// ============================================================================

/**
 * @brief Windowed-sinc lowpass FIR coefficients
 *
 * Truncates the ideal lowpass impulse response to @p taps and applies a Hann
 * taper. Transition width narrows as taps increases; stopband attenuation is
 * set by the taper, not the length.
 *
 * Returned in FIR tap order: index 0 multiplies the newest sample. Group
 * delay is (taps - 1) / 2 samples.
 *
 * @param frequency   Cutoff in Hz, clamped to (0, sample_rate/2)
 * @param sample_rate Sampling rate in Hz
 * @param taps        Kernel length; odd values give an integer group delay
 * @return Coefficient vector of length taps
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> sinc_lowpass(
    double frequency, double sample_rate, size_t taps);

/**
 * @brief Windowed-sinc highpass FIR coefficients
 *
 * Spectral inversion of the corresponding lowpass. Requires odd @p taps for
 * the inversion to be exact; even lengths are incremented internally.
 *
 * @param frequency   Cutoff in Hz, clamped to (0, sample_rate/2)
 * @param sample_rate Sampling rate in Hz
 * @param taps        Kernel length, forced odd
 * @return Coefficient vector of length taps or taps + 1
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> sinc_highpass(
    double frequency, double sample_rate, size_t taps);

/**
 * @brief Windowed-sinc bandpass FIR coefficients
 *
 * Difference of two lowpass kernels. Requires low_hz < high_hz; the arguments
 * are swapped internally if supplied in the other order.
 *
 * @param low_hz      Lower cutoff in Hz
 * @param high_hz     Upper cutoff in Hz
 * @param sample_rate Sampling rate in Hz
 * @param taps        Kernel length, forced odd
 * @return Coefficient vector of length taps or taps + 1
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> sinc_bandpass(
    double low_hz, double high_hz, double sample_rate, size_t taps);

// ============================================================================
// Least-squares polynomial kernels
// ============================================================================

/**
 * @brief Savitzky-Golay FIR coefficients for smoothing or differentiation
 *
 * Fits a polynomial of degree @p poly_order to a sliding window by least
 * squares and evaluates the requested derivative of that fit at the window
 * centre. With derivative 0 this smooths while preserving peak shape better
 * than a boxcar of the same length; with derivative 1 or 2 it estimates rate
 * of change with far less noise amplification than a raw finite difference,
 * since the differentiation acts on the fitted polynomial rather than on the
 * samples.
 *
 * This is the block and node form of the kernel that
 * Kinesis::Differential::backward_difference computes per sample on glm types.
 * The streaming form divides by an observed dt and handles irregular timing;
 * this form assumes uniform spacing and returns coefficients in sample units,
 * so a caller needing physical units scales by 1 / dt^derivative.
 *
 * Returned in FIR tap order: index 0 multiplies the newest sample. Group
 * delay is (window - 1) / 2 samples.
 *
 * @param window     Kernel length, forced odd, must exceed poly_order
 * @param poly_order Degree of the fitted polynomial
 * @param derivative Which derivative of the fit to evaluate; 0 smooths
 * @return Coefficient vector of length window, or empty if window <= poly_order
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> savitzky_golay(
    size_t window, size_t poly_order, size_t derivative = 0);

/**
 * @brief Lagrange fractional-delay FIR coefficients
 *
 * Interpolates between samples at a non-integer offset by fitting a polynomial
 * through @p order + 1 consecutive points. Order 1 is linear interpolation;
 * order 3 at a fractional position reproduces the Catmull-Rom weights held as
 * a matrix in Kinesis::BasisMatrices, generalised to any order.
 *
 * Accuracy is highest when @p delay falls near the centre of the tap span,
 * so callers wanting a delay of D samples with order N typically use an
 * integer delay line of D - N/2 followed by this kernel for the remainder.
 *
 * @param delay Delay in samples, need not be integral; clamped to [0, order]
 * @param order Polynomial order; produces order + 1 taps
 * @return Coefficient vector of length order + 1
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> lagrange_delay(
    double delay, size_t order);

// ============================================================================
// Direct pole placement
// ============================================================================

/**
 * @brief Two-pole resonator specified by z-plane pole position
 *
 * Places a conjugate pole pair at radius @p pole_radius and angle
 * @p pole_angle rather than deriving them from a cutoff and Q. Ring time
 * grows as the radius approaches 1.0; the resonant frequency is
 * pole_angle * sample_rate / (2 * pi).
 *
 * The numerator is a single scalar normalising peak magnitude to unity, so
 * b has length 1 and no zeros are placed. For a resonator with zeros at
 * DC and Nyquist, use biquad_bandpass instead.
 *
 * @param pole_radius Pole magnitude, clamped to [0, 0.9999]
 * @param pole_angle  Pole angle in radians, in [0, pi]
 * @param a           Denominator output, resized to 3, a[0] == 1.0
 * @param b           Numerator output, resized to 1
 */
MAYAFLUX_API void resonator(double pole_radius, double pole_angle,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief One-pole filter specified by pole position
 *
 * A positive @p pole gives a lowpass whose smoothing increases as the value
 * approaches 1.0; a negative pole gives a highpass. The numerator normalises
 * DC gain to unity for positive poles and Nyquist gain to unity for negative.
 *
 * @param pole Pole position on the real axis, clamped to (-0.9999, 0.9999)
 * @param a    Denominator output, resized to 2, a[0] == 1.0
 * @param b    Numerator output, resized to 1
 */
MAYAFLUX_API void one_pole(double pole,
    std::vector<double>& a, std::vector<double>& b);

/**
 * @brief DC blocking filter
 *
 * A zero at DC with a pole just inside it, removing constant offset while
 * leaving everything above the corner essentially untouched. The corner
 * frequency falls as @p pole approaches 1.0.
 *
 * @param pole Pole position, clamped to [0, 0.9999]; 0.995 is a typical value
 * @param a    Denominator output, resized to 2, a[0] == 1.0
 * @param b    Numerator output, resized to 2
 */
MAYAFLUX_API void dc_block(double pole,
    std::vector<double>& a, std::vector<double>& b);

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
