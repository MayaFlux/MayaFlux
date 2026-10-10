#pragma once

/**
 * @file Kernels.hpp
 * @brief Pure functions of a newest-first window and an array of numbers.
 *
 * Every kernel has the shape `double(window, coefs)` and reads the window as
 * a newest-first span: window[0] is the current value, window[k] is k steps
 * back. In a recursive Polynomial, window[0] is the new input and window[k]
 * is the output k steps ago. The array is whatever the kernel documents it
 * to be, and nothing about it is a mode: the numbers in it are the behaviour.
 *
 * The functions convert directly to Polynomial::CoefBufferFunction, so
 * `Polynomial(Kinesis::Discrete::weighted_sum, mode, size, weights)` is a
 * complete node. Read-only kernels take a const array; phasor_bank writes
 * its phases back into the array and takes a mutable one.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief Horner evaluation, coefficients highest power first.
 */
[[nodiscard]] MAYAFLUX_API double horner(std::span<const double> coefficients, double x) noexcept;

/**
 * @brief Sum of window[k] * coefs[k] over the shorter of the two.
 *
 * The linear case: FIR taps in a feedforward window, feedback weights in a
 * recursive one, or the output of any generator in Coefficients.
 */
[[nodiscard]] MAYAFLUX_API double weighted_sum(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Sum of gain * window[lag] over (lag, gain) pairs.
 *
 * coefs holds pairs: coefs[2j] is a lag in samples, coefs[2j + 1] its gain.
 * Fractional lags interpolate linearly between neighbours. A pair whose lag
 * lies outside the window contributes nothing, so a short window simply
 * reaches fewer taps.
 */
[[nodiscard]] MAYAFLUX_API double tapped_sum(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Transfer table indexed by the current value.
 *
 * coefs are samples of a curve over [-1, 1], read with linear interpolation
 * and clamped at the ends. The input is window[0]. A table of one entry is a
 * constant; an empty table gives zero.
 */
[[nodiscard]] MAYAFLUX_API double table_lookup(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief table_lookup indexed by window[1] instead of window[0].
 *
 * In a recursive Polynomial window[1] is the previous output, so the table is
 * a map the output iterates: y[n] = table(y[n - 1]).
 */
[[nodiscard]] MAYAFLUX_API double table_lookup_previous(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Sum of coefs[k] * T_k(window[0]) over Chebyshev polynomials.
 *
 * Because T_k(cos t) = cos(kt), a sine of amplitude one passed through this
 * has harmonic k at weight coefs[k]. The input is clamped to [-1, 1] and the
 * sum is evaluated by Clenshaw's recurrence.
 */
[[nodiscard]] MAYAFLUX_API double chebyshev_series(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Polynomial of the previous value, coefs highest power first.
 *
 * In a recursive Polynomial this iterates y[n] = P(y[n - 1]): the logistic
 * and cubic maps are one array each.
 */
[[nodiscard]] MAYAFLUX_API double iterated_polynomial(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Radians per sample for a frequency, for building phasor_bank state.
 */
[[nodiscard]] inline double radians_per_sample(double frequency, double sample_rate) noexcept
{
    return 2.0 * std::numbers::pi * frequency / sample_rate;
}

/**
 * @brief Builds the array phasor_bank reads and writes.
 * @param increments Radians per sample of each partial; sets the count
 * @param scales Amplitude of each partial; missing entries are 1
 * @param am_increments Radians per sample of each partial's slow amplitude
 *        swell; missing entries are 0, which holds the swell at one half
 * @param phases Starting phase of each partial; missing entries are 0
 * @param am_phases Starting phase of each swell; missing entries are 0
 *
 * Layout: [N, increments, scales, am_increments, phases, am_phases], each
 * column N long. Whether partials start together or apart is only what
 * the phase columns hold.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> phasor_bank_state(
    std::span<const double> increments,
    std::span<const double> scales = {},
    std::span<const double> am_increments = {},
    std::span<const double> phases = {},
    std::span<const double> am_phases = {});

/**
 * @brief Sum of N sinusoidal partials, each with its own slow amplitude swell.
 *
 * coefs is the array phasor_bank_state builds. Every call advances each
 * partial's phase and swell phase by its increment and returns
 * sum(scale * sin(phase) * (0.5 + 0.5 * sin(swell_phase))). The window is
 * not read. An array too short for its stated count gives zero. Increments
 * must stay below one revolution per sample.
 */
[[nodiscard]] MAYAFLUX_API double phasor_bank(
    std::span<const double> window, std::span<double> coefs) noexcept;

} // namespace MayaFlux::Kinesis::Discrete
