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

// ---------------------------------------------------------------------------
// Polynomial over lags, series and curves
// ---------------------------------------------------------------------------

/**
 * @brief Polynomial in several lags at once.
 *
 * coefs: [L, P, C...] with L lags and highest power P, then (P + 1) numbers
 * per lag, lag-major and power-ascending. The result is the sum over lags k
 * and powers j of C[k][j] * window[k]^j. Power zero is a constant, so a bias
 * is C[0][0]. In a recursive Polynomial this is any nonlinear difference
 * equation of the last L outputs: Henon is lags 0, 1, 2 with
 * C[0][0] = 1, C[1][2] = -a, C[2][1] = b. Lags beyond the window are
 * skipped; an array shorter than its header gives zero.
 */
[[nodiscard]] MAYAFLUX_API double polynomial_lags(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Fourier series of a phase.
 *
 * The phase in radians is window[0]. coefs: [a0, a1, b1, a2, b2, ...] and
 * the result is a0 + sum a_k cos(k t) + b_k sin(k t). A final a_k without its
 * b_k is read with b_k = 0. One sine and one cosine are evaluated, the
 * harmonics follow by rotation.
 */
[[nodiscard]] MAYAFLUX_API double fourier_series(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Piecewise linear curve through (x, y) points.
 *
 * coefs: pairs (x, y) with ascending x. The input is window[0], held at the
 * first and last y outside the span of x. Uneven spacing is fine; unsorted x
 * is not. An empty array gives zero.
 */
[[nodiscard]] MAYAFLUX_API double breakpoint_curve(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Table indexed by the recent shape of the window.
 *
 * coefs: [levels, count, lo, hi, table...]. The newest `count` values are
 * each quantised to one of `levels` cells over [lo, hi] (Lattice1D), combined
 * into an index with the newest value as the lowest digit, and the table of
 * levels^count entries is read there. Values outside [lo, hi] fall in the
 * edge cells and missing values count as zero. A table too small for the
 * index range, or hi not above lo, gives zero.
 */
[[nodiscard]] MAYAFLUX_API double pattern_lookup(
    std::span<const double> window, std::span<const double> coefs) noexcept;

// ---------------------------------------------------------------------------
// Windows as sets: order statistics and features
// ---------------------------------------------------------------------------

/**
 * @brief Maximum over the window of window[k] + coefs[k], the grayscale dilation.
 *
 * coefs are the offsets of the structuring element. Runs over the shorter of
 * the two; empty gives zero.
 */
[[nodiscard]] MAYAFLUX_API double dilate(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Minimum over the window of window[k] - coefs[k], the grayscale erosion.
 */
[[nodiscard]] MAYAFLUX_API double erode(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Quantile of the whole window, interpolating between ranks.
 *
 * The per-call counterpart of Discrete::percentile, which is the batch form
 * over every window of a buffer and takes percent.
 *
 * coefs[0] is the quantile in [0, 1], 0.5 when the array is empty: 0 is the
 * minimum, 1 the maximum. Uses a per-thread scratch vector, so it allocates
 * only the first time a window of a new size is seen.
 */
[[nodiscard]] MAYAFLUX_API double quantile(
    std::span<const double> window, std::span<const double> coefs);

/**
 * @brief How far the newest value sits from the rest of the window, in standard deviations.
 *
 * A novelty measure. Not Discrete::mean_zscore, which averages the z-scores of
 * a window about its own mean.
 *
 * window[0] against the mean and sample standard deviation of window[1..].
 * coefs[0], when present, is the smallest deviation used as a divisor
 * (default 1e-12), so a flat history does not divide by zero. Fewer than two
 * older values give zero.
 */
[[nodiscard]] MAYAFLUX_API double zscore(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Weighted energy of the window at chosen frequencies.
 *
 * coefs: pairs (frequency in cycles per sample, weight). Each frequency is
 * measured with the Goertzel recurrence over the whole window and gives the
 * squared amplitude of the sinusoid there (a sine of amplitude A on the
 * frequency gives A^2 when it fits the window in whole cycles); the result is
 * the weighted sum.
 */
[[nodiscard]] MAYAFLUX_API double goertzel(
    std::span<const double> window, std::span<const double> coefs) noexcept;

/**
 * @brief Weighted sum wrapped into [0, modulus).
 *
 * coefs: [modulus, weights...]. In a recursive Polynomial the weights act on
 * the last outputs, which is a lagged-Fibonacci style recurrence with the
 * structure the weights and modulus give it. A modulus of zero or less
 * leaves the sum unwrapped.
 */
[[nodiscard]] MAYAFLUX_API double modular_recurrence(
    std::span<const double> window, std::span<const double> coefs) noexcept;

// ---------------------------------------------------------------------------
// State-bearing kernels: the array holds the state
// ---------------------------------------------------------------------------

/**
 * @brief Builds the array kuramoto reads and writes.
 * @param omegas Natural increment of each oscillator in radians per sample;
 *        sets the count
 * @param coupling Strength of the pull toward the group
 * @param phases Starting phases; missing entries are 0
 *
 * Layout: [N, coupling, omegas, phases].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> kuramoto_state(
    std::span<const double> omegas, double coupling, std::span<const double> phases = {});

/**
 * @brief Kuramoto oscillators pulled toward their mean phase.
 *
 * Each call advances every phase by its own increment plus
 * coupling * r * sin(mean_phase - phase), where r e^(i mean_phase) is the
 * mean of the phases on the unit circle, then returns the mean of the sines.
 * With no coupling the group stays loose; past a critical coupling it
 * locks, and the output grows as the phases align. Costs a few
 * trigonometric calls per oscillator per call.
 */
[[nodiscard]] MAYAFLUX_API double kuramoto(
    std::span<const double> window, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array coupled_map_lattice reads and writes.
 * @param cells Starting cell values in [0, 1]; sets the count
 * @param r Logistic parameter, up to 4
 * @param epsilon Coupling to the two neighbours in [0, 1]
 *
 * Layout: [N, r, epsilon, cells, scratch], the scratch column holding the
 * mapped values between steps.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> coupled_map_lattice_state(
    std::span<const double> cells, double r, double epsilon);

/**
 * @brief Ring of logistic maps with nearest neighbour coupling.
 *
 * Each call maps every cell, f(x) = r x (1 - x), then mixes each cell with
 * its two neighbours (periodic): x' = (1 - eps) f(x) + eps / 2 (f(left) +
 * f(right)). Returns the mean cell minus one half. Uniform cells stay
 * uniform; different cells give the pattern formation the coupling allows.
 */
[[nodiscard]] MAYAFLUX_API double coupled_map_lattice(
    std::span<const double> window, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array lorenz_attractor reads and writes.
 *
 * Layout: [sigma, rho, beta, dt, x, y, z]. The defaults are the classic
 * parameters in model time units.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> lorenz_state(
    double dt = 0.005,
    double sigma = 10.0,
    double rho = 28.0,
    double beta = 8.0 / 3.0,
    double x = 1.0,
    double y = 1.0,
    double z = 1.0);

/**
 * @brief Lorenz system stepped once by fourth order Runge-Kutta, returning x.
 *
 * The state and parameters live in the array, so editing sigma, rho, beta or
 * dt between calls sweeps the system through its regimes. An array shorter
 * than seven gives zero.
 */
[[nodiscard]] MAYAFLUX_API double lorenz_attractor(
    std::span<const double> window, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array step_sequence reads and writes.
 * @param values The sequence, which sets the length
 * @param threshold Level the trigger must rise through
 *
 * Layout: [threshold, cursor, latched, N, values]. It starts latched, so an
 * input that begins high does not advance until it has fallen and risen.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> step_sequence_state(
    std::span<const double> values, double threshold = 0.5);

/**
 * @brief Table of values advanced by a trigger.
 *
 * Returns values[cursor], starting at the first value. Each time window[0]
 * rises through the threshold the cursor advances by one and wraps. A held
 * high input advances once.
 */
[[nodiscard]] MAYAFLUX_API double step_sequence(
    std::span<const double> window, std::span<double> coefs) noexcept;

} // namespace MayaFlux::Kinesis::Discrete
