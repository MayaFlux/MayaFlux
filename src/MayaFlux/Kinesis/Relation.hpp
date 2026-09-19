#pragma once

#include "MayaFlux/Kinesis/Stochastic/Estimate.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// Scope
//
// Everything in FeatureExtent.hpp, PathShape.hpp, and TemporalMeasures.hpp
// operates on one stream. This file is the missing sibling: how two or
// more streams move with respect to each other, independent of what
// either stream measures or where it came from. A tablet's pressure
// against its own tilt, one dancer's tracked hand against another's,
// a text's sentence length against its punctuation density: the same
// functions apply to all three, because none of them look at anything
// but the numbers.
//
// Chronological order, dt-as-uniform-seconds-per-sample, and the
// non-allocating single-or-double-pass shape all follow the convention
// established in TemporalMeasures.hpp; see that file's header comment
// for the reasoning, not repeated here.
//
// Discrete::cross_correlate (Discrete/Convolution.hpp) is the offline
// sibling: FFT-backed, computes every lag, allocates a full output
// vector, the right tool for characterizing a whole recording once.
// relation_at_lag below is its bounded, non-allocating, single-lag
// counterpart, the right shape for a scalar computed on every channel
// update, in the same relationship estimate_period in
// TemporalMeasures.hpp has to Discrete::auto_correlate.
// =============================================================================

/**
 * @brief Pearson correlation between two equal-length windows
 * @param a First span
 * @param b Second span, same length as @p a
 * @return Correlation in -1..1, or zero if either span has near-zero
 *         variance or the lengths differ
 *
 * The base relation measure: do these two streams move together, apart,
 * or independently, over this window, with no notion of one leading the
 * other. Scale-free, so a pressure axis in 0..1 and a tilt axis in
 * degrees compare meaningfully without the caller normalizing first.
 */
[[nodiscard]] inline double correlation(
    std::span<const double> a, std::span<const double> b) noexcept
{
    if (a.size() != b.size() || a.size() < 2)
        return 0.0;

    const size_t n = a.size();
    double mean_a = 0.0;
    double mean_b = 0.0;
    for (size_t i = 0; i < n; ++i) {
        mean_a += a[i];
        mean_b += b[i];
    }
    mean_a /= static_cast<double>(n);
    mean_b /= static_cast<double>(n);

    double cov = 0.0;
    double var_a = 0.0;
    double var_b = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double da = a[i] - mean_a;
        const double db = b[i] - mean_b;
        cov += da * db;
        var_a += da * da;
        var_b += db * db;
    }

    const double denom = std::sqrt(var_a * var_b);
    if (denom < 1e-12)
        return 0.0;

    return std::clamp(cov / denom, -1.0, 1.0);
}

/**
 * @struct LagRelation
 * @brief A candidate offset between two streams and how strongly the
 *        windows support it.
 *
 * Mirrors PeriodEstimate in TemporalMeasures.hpp for the same reason: a
 * bare best lag, with no strength attached, silently reports structure
 * in noise on every call, including calls where the two streams are
 * genuinely unrelated. lag and strength together separate "these lead
 * one another by this much" from "no relationship was found".
 */
struct LagRelation {
    long lag { 0 }; ///< Samples b is offset from a; positive means b lags a.
    double strength { 0.0 }; ///< Correlation at that lag, in -1..1. Near zero means no relation.
};

/**
 * @brief Correlation between two spans at a single fixed lag
 * @param a First span
 * @param b Second span
 * @param lag Samples to offset @p b relative to @p a; positive tests
 *        whether b's later samples resemble a's earlier ones
 * @return Correlation over the overlapping region at that lag, or zero
 *         if the overlap is too short to be meaningful
 *
 * The bounded, single-lag counterpart to Discrete::cross_correlate; see
 * this file's header comment for the relationship. Exists as its own
 * function, distinct from relation_lag below, because a caller who
 * already knows the lag they care about (a fixed device latency, a
 * known frame offset between two sensors) should not pay for a search
 * over a range to confirm what they already know.
 */
[[nodiscard]] inline double relation_at_lag(
    std::span<const double> a, std::span<const double> b, long lag) noexcept
{
    const auto na = static_cast<long>(a.size());
    const auto nb = static_cast<long>(b.size());

    const long start_a = std::max(0L, -lag);
    const long start_b = std::max(0L, lag);
    const long overlap = std::min(na - start_a, nb - start_b);

    if (overlap < 2)
        return 0.0;

    return correlation(
        a.subspan(static_cast<size_t>(start_a), static_cast<size_t>(overlap)),
        b.subspan(static_cast<size_t>(start_b), static_cast<size_t>(overlap)));
}

/**
 * @brief Search a bounded lag range for the offset at which two streams
 *        correlate most strongly
 * @param a First span
 * @param b Second span
 * @param max_lag Furthest offset to test in either direction, in samples
 * @return The lag in [-max_lag, max_lag] with the highest absolute
 *         correlation, and that correlation as strength
 *
 * Answers which of two streams leads and by how much, without the
 * caller specifying the offset in advance. Searches both directions
 * since neither stream is privileged: a positive result lag means b
 * lags a, a negative one means a lags b.
 *
 * Signed strength is kept rather than always reporting the magnitude,
 * so a caller distinguishes streams moving together (positive) from
 * streams moving in exact opposition at the same offset (negative);
 * the best lag is chosen by absolute value since a strong inverse
 * relationship is as informative as a strong direct one.
 *
 * Bounded rather than full-length for the same reason estimate_period
 * in TemporalMeasures.hpp bounds its period search: the caller stating
 * the range of offsets that could plausibly matter keeps the search
 * from locking onto a coincidental alignment far outside any physically
 * meaningful lag between two related streams.
 */
[[nodiscard]] inline LagRelation relation_lag(
    std::span<const double> a, std::span<const double> b, long max_lag) noexcept
{
    LagRelation best;
    for (long lag = -max_lag; lag <= max_lag; ++lag) {
        const double r = relation_at_lag(a, b, lag);
        if (std::abs(r) > std::abs(best.strength)) {
            best.strength = r;
            best.lag = lag;
        }
    }
    return best;
}

/**
 * @brief Rate at which two streams approach or separate
 * @param a First span, chronological order
 * @param b Second span, chronological order, same length as @p a
 * @param dt Seconds per sample
 * @return Least-squares slope of |a - b| over the window, per second;
 *         negative means the streams are converging, positive diverging
 *
 * Distinct from correlation, which asks whether two streams move in the
 * same direction as each other, not whether they are getting nearer.
 * Two streams can correlate strongly while one holds steady and the
 * other drifts away from it, or diverge while both move in the same
 * direction at different rates. This is the direct measure of mutual
 * approach the dancers-and-a-third-point case needs: not whether the
 * third point's motion resembles either hand's, but whether it is
 * closing on the gap between them.
 *
 * Operates on scalar streams. A caller with vector positions supplies
 * the scalar separation (glm::length of the difference, or a projected
 * distance) rather than raw components, since "approaching" is a
 * statement about a single distance, not about three independent axes
 * each separately trending.
 */
[[nodiscard]] inline double approach_rate(
    std::span<const double> a, std::span<const double> b, double dt) noexcept
{
    const size_t n = a.size();
    if (n != b.size() || n < 2 || dt <= 0.0)
        return 0.0;

    std::vector<double> separation(n);
    for (size_t i = 0; i < n; ++i)
        separation[i] = std::abs(a[i] - b[i]);

    return Stochastic::Estimate::trend_slope(separation) / dt;
}

/**
 * @brief How tightly a set of streams move together
 * @param streams Each span the same length, at least two spans
 * @return Mean pairwise correlation across all distinct pairs, in
 *         -1..1, or zero if fewer than two streams are given
 *
 * Generalizes correlation from two streams to an arbitrary-size set,
 * with no assumption about which stream is which or how many there
 * are: two tracked hands, five gamepad axes, a whole ensemble's worth
 * of per-voice measurements. A high value means the set is moving as
 * one; a value near zero means the set's motion is not coordinated;
 * a strongly negative value means the set is systematically split into
 * streams moving in opposition to each other.
 *
 * Mean pairwise rather than a single eigenvalue-based coherence measure:
 * cheap, order-independent, and interpretable directly as a correlation
 * without a caller needing to reason about a covariance matrix's
 * spectrum. A caller wanting the finer-grained structure (which streams
 * are actually forming a bloc) computes correlation() over the specific
 * pairs of interest instead.
 */
[[nodiscard]] inline double coherence(
    std::span<const std::span<const double>> streams) noexcept
{
    const size_t n = streams.size();
    if (n < 2)
        return 0.0;

    double acc = 0.0;
    size_t pairs = 0;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            acc += correlation(streams[i], streams[j]);
            ++pairs;
        }
    }

    return (pairs > 0) ? (acc / static_cast<double>(pairs)) : 0.0;
}

} // namespace MayaFlux::Kinesis
