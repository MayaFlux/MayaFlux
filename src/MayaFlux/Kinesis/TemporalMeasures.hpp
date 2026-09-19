#pragma once

#include "Stochastic/Estimate.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// Ordering and sampling convention
//
// Every function here takes a CHRONOLOGICAL span: values[0] earliest,
// values[n-1] latest. This matches PathShape.hpp and is the opposite of
// the newest-first convention in Differential.hpp. Nothing computed
// directly in this file is order-sensitive (Estimate::trend_slope,
// which this file calls rather than reimplements, is itself order-
// sensitive in the same way), so the convention is carried for
// consistency with PathShape.hpp rather than because every function
// below individually requires it.
//
// dt is seconds per sample and is assumed uniform across the window.
// Channel updates driven by device arrival are not exactly uniform, so
// every result scaled by dt carries that approximation. Over a window
// short enough for the measure to mean anything the jitter is small
// against the quantity being measured, and carrying per-sample
// timestamps through every signature to remove an error smaller than
// the measurement is not a trade worth making.
//
// Mean, variance, and trend slope over a plain span already exist as
// Stochastic::Estimate::variance and Estimate::trend_slope and are used
// here rather than reimplemented; Estimate::trend_slope reports per
// sample index, and this file's own functions report per second, so a
// caller mixing the two must apply dt to Estimate's result rather than
// assume the units already match. Discrete::mean and Discrete::variance
// in Discrete/Analysis.hpp are a third sibling, parallelized and
// windowed into a vector over a whole buffer; the right tool for
// offline analysis of a long recording, not for a scalar computed once
// per channel update, which is what everything below is shaped for.
// =============================================================================

/**
 * @brief Mean absolute difference between consecutive samples, per second
 * @param values Chronological span, at least two entries
 * @param dt Seconds per sample
 * @return Average absolute step size divided by dt
 *
 * Agitation independent of direction and of where the value sits. A
 * quantity oscillating rapidly within a narrow band and one holding
 * still have similar means and similar ranges and very different
 * roughness, and a quantity sweeping smoothly across its whole range
 * has a large variance and a small roughness.
 *
 * Distinct from two existing measures that sound related. Not
 * Discrete::mad, which is median absolute deviation about a centre and
 * measures spread, not step size. Not Estimate::trend_explained_ratio,
 * which asks what fraction of a window's variance its own linear trend
 * accounts for and answers signal against noise around that trend;
 * roughness answers nothing about a trend and reports the same value
 * whether the steps are around a fixed point or along a steady climb.
 * A window can score high on both: a value climbing steadily in small
 * jittery increments has both a high trend-explained ratio and, if the
 * increments are large relative to the climb's own rate, high
 * roughness.
 */
[[nodiscard]] inline double roughness(std::span<const double> values, double dt) noexcept
{
    const size_t n = values.size();
    if (n < 2 || dt <= 0.0)
        return 0.0;

    double acc = 0.0;
    for (size_t i = 1; i < n; ++i)
        acc += std::abs(values[i] - values[i - 1]);

    return (acc / static_cast<double>(n - 1)) / dt;
}

/**
 * @brief Difference between the largest and smallest value in a window
 * @param values Chronological span, non-empty
 * @return Range, or zero for an empty span
 *
 * How much ground the value covered, ignoring how many times it covered
 * it. Pairs with roughness: a large excursion with low roughness is one
 * slow sweep, a small excursion with high roughness is a tremor, and
 * both large is a sustained thrashing.
 */
[[nodiscard]] inline double excursion(std::span<const double> values) noexcept
{
    if (values.empty())
        return 0.0;
    const auto [lo, hi] = std::ranges::minmax_element(values);
    return *hi - *lo;
}

/**
 * @brief Seconds within a window spent at or above a threshold
 * @param values Chronological span
 * @param threshold Level to test against
 * @param dt Seconds per sample
 * @return Count of qualifying samples multiplied by dt
 *
 * Occupancy in time rather than in space. The scalar counterpart to
 * SymbolicTrajectory::dwell_count, which answers the same question for
 * a point moving through a lattice.
 */
[[nodiscard]] inline double time_above(
    std::span<const double> values, double threshold, double dt) noexcept
{
    size_t count = 0;
    for (const double v : values) {
        if (v >= threshold)
            ++count;
    }
    return static_cast<double>(count) * dt;
}

/**
 * @brief Threshold crossings per second within a window
 * @param values Chronological span, at least two entries
 * @param threshold Level whose crossings are counted
 * @param dt Seconds per sample
 * @return Crossings in either direction, divided by the window duration
 *
 * A rate in seconds against an arbitrary level, where
 * Discrete::zero_crossing_rate is normalized per sample and windowed
 * into a vector. Both count the same events; this is the shape a
 * per-update scalar measure needs.
 *
 * Counts crossings in both directions, so a value oscillating about the
 * threshold reports twice the rate of its underlying cycle. Halve the
 * result when the intended quantity is cycles rather than transitions.
 */
[[nodiscard]] inline double threshold_crossing_rate(
    std::span<const double> values, double threshold, double dt) noexcept
{
    const size_t n = values.size();
    if (n < 2 || dt <= 0.0)
        return 0.0;

    size_t crossings = 0;
    for (size_t i = 1; i < n; ++i) {
        if ((values[i] >= threshold) != (values[i - 1] >= threshold))
            ++crossings;
    }

    const double duration = static_cast<double>(n - 1) * dt;
    return static_cast<double>(crossings) / duration;
}

/**
 * @struct PeriodEstimate
 * @brief A candidate repetition period and how strongly the window
 *        actually supports it.
 *
 * Period alone is not a usable answer. Any window returns some lag at
 * which its correlation happens to be highest, including a window with
 * no repetition in it at all, so a bare period silently reports
 * structure in noise. Strength is the normalized correlation at that
 * lag and is what separates a quantity that genuinely repeats from one
 * that merely has a best lag.
 *
 * A meaning phrased as a tendency to do something every few seconds
 * needs both numbers: the period is the few seconds, and the strength
 * is the tendency.
 */
struct PeriodEstimate {
    double period { 0.0 }; ///< Seconds between repetitions, zero when no lag qualified.
    double strength { 0.0 }; ///< Normalized autocorrelation at that lag, in -1..1. Near zero means no repetition.
};

/**
 * @brief Estimate a repetition period by direct autocorrelation over a
 *        bounded lag range
 * @param values Chronological span
 * @param dt Seconds per sample
 * @param min_period Shortest period to consider, in seconds
 * @param max_period Longest period to consider, in seconds
 * @return The lag in the range with the highest normalized correlation,
 *         with that correlation as strength
 *
 * Direct rather than FFT-backed, and bounded rather than full-lag. The
 * caller stating the period range they care about is not a limitation
 * to work around: a meaning about turning every few seconds is not
 * interested in a correlation peak at forty milliseconds, and searching
 * for one invites the estimate to lock onto sensor noise or onto a
 * harmonic of the real period.
 *
 * Correlation is normalized by the window's own variance
 * (Estimate::variance), so strength is scale-free and comparable across
 * channels carrying different units. A window whose variance is
 * negligible has nothing to correlate and returns zero strength rather
 * than a ratio of two small numbers.
 *
 * Requires the window to be at least twice the longest period searched,
 * since a lag longer than half the window correlates too few pairs to
 * mean anything. Lags failing that are skipped rather than reported
 * with low confidence.
 *
 * Distinct from Discrete::auto_correlate, which is FFT-backed, computes
 * every lag up to the buffer length, and allocates a full output vector
 * each call; the right shape for characterizing a recording once, the
 * wrong shape for a bounded search run on every channel update.
 */
[[nodiscard]] inline PeriodEstimate estimate_period(
    std::span<const double> values,
    double dt,
    double min_period,
    double max_period) noexcept
{
    const size_t n = values.size();
    if (n < 4 || dt <= 0.0 || max_period <= min_period)
        return {};

    double mean = 0.0;
    for (const double v : values)
        mean += v;
    mean /= static_cast<double>(n);

    const double denom = Stochastic::Estimate::variance(values) * static_cast<double>(n - 1);
    if (denom < 1e-12)
        return {};

    const auto min_lag = std::max<size_t>(1, static_cast<size_t>(min_period / dt));
    const auto max_lag = std::min(n / 2, static_cast<size_t>(max_period / dt));
    if (min_lag > max_lag)
        return {};

    PeriodEstimate best;
    for (size_t lag = min_lag; lag <= max_lag; ++lag) {
        double acc = 0.0;
        for (size_t i = 0; i + lag < n; ++i)
            acc += (values[i] - mean) * (values[i + lag] - mean);

        const double correlation = acc / denom;
        if (correlation > best.strength) {
            best.strength = correlation;
            best.period = static_cast<double>(lag) * dt;
        }
    }

    return best;
}

} // namespace MayaFlux::Kinesis
