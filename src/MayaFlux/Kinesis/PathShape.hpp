#pragma once

#include "MayaFlux/Kinesis/Differential.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// Ordering convention
//
// Every function in this file takes a path in CHRONOLOGICAL order:
// path[0] is the earliest sample, path[n-1] the latest. This is the
// opposite of the newest-first convention HistoryBuffer and the span
// overloads in Differential.hpp use.
//
// The difference is not cosmetic. Reversing a path negates its signed
// turning and its signed area, which are precisely the quantities this
// file exists to compute, so a silently reversed path does not produce
// a slightly wrong answer, it produces the mirror answer with full
// confidence. Rather than provide overloads that would make the
// ordering ambiguous at the call site, the bridge is explicit:
// chronological_window<T>() (Differential.hpp, beside to_history)
// reverses a HistoryBuffer into the order these functions expect.
// =============================================================================

/**
 * @brief Resample a path to a fixed point count at uniform arc length
 * @param path Positions in chronological order, at least two
 * @param count Output point count, minimum 2
 * @return @p count points evenly spaced along the path by distance
 *
 * Removes speed from the path entirely. Two strokes tracing the same
 * shape, one hurried through its middle and one dwelling there, produce
 * different sample distributions in time and identical ones after this,
 * which is the precondition for comparing them point against point.
 *
 * A path whose total length is below the epsilon guard is degenerate,
 * every sample having landed in effectively one place, and is returned
 * as @p count copies of its first point rather than dividing by zero.
 *
 * MotionCurves::reparameterize_by_arc_length does the same thing over
 * an Eigen::MatrixXd with columns as points. This exists so a caller
 * holding glm::vec2 samples does not transpose into a matrix and back
 * for what is a linear walk.
 */
[[nodiscard]] inline std::vector<glm::vec2> resample_uniform(
    std::span<const glm::vec2> path, size_t count)
{
    count = count < 2 ? 2 : count;
    if (path.size() < 2)
        return std::vector<glm::vec2>(count, path.empty() ? glm::vec2 { 0.0F } : path[0]);

    std::vector<float> arc;
    arc.reserve(path.size());
    arc.push_back(0.0F);
    float total = 0.0F;
    for (size_t i = 1; i < path.size(); ++i) {
        total += glm::length(path[i] - path[i - 1]);
        arc.push_back(total);
    }

    if (total < 1e-6F)
        return std::vector<glm::vec2>(count, path[0]);

    std::vector<glm::vec2> out;
    out.reserve(count);
    const float step = total / static_cast<float>(count - 1);

    size_t upper = 1;
    for (size_t i = 0; i < count; ++i) {
        const float target = static_cast<float>(i) * step;
        while (upper < arc.size() - 1 && arc[upper] < target)
            ++upper;
        const size_t lower = upper - 1;
        const float segment = arc[upper] - arc[lower];
        const float t = (segment > 1e-9F) ? ((target - arc[lower]) / segment) : 0.0F;
        out.push_back(glm::mix(path[lower], path[upper], t));
    }
    return out;
}

/**
 * @brief Turning accumulated along a path, with sign retained
 * @param path Positions in chronological order, at least three
 * @return Sum of signed angular deltas between consecutive segment
 *         headings, positive for net counterclockwise
 *
 * The signed counterpart to total_turning in Differential.hpp, which
 * accumulates absolute values. That one measures how much a path bent
 * in total and is blind to direction; this one measures where it ended
 * up in heading and lets opposing bends cancel.
 *
 * The pair separates cases neither resolves alone. A closed circle and
 * a figure-eight of the same arc length accumulate similar absolute
 * turning and very different signed turning, near one full revolution
 * against near zero. A clockwise and a counterclockwise circle are
 * identical under absolute turning and opposite under this.
 */
[[nodiscard]] inline float signed_turning(std::span<const glm::vec2> path) noexcept
{
    if (path.size() < 3)
        return 0.0F;

    float total = 0.0F;
    float prev = heading(path[1] - path[0]);
    for (size_t i = 2; i < path.size(); ++i) {
        const float h = heading(path[i] - path[i - 1]);
        total += angular_delta(h, prev);
        prev = h;
    }
    return total;
}

/**
 * @brief Net revolutions a path turned through
 * @param path Positions in chronological order
 * @return signed_turning divided by two pi
 *
 * Reads directly as a count: near 1 for one counterclockwise loop, near
 * -1 for one clockwise loop, near 0 for a path that returned to its
 * starting heading without net rotation.
 */
[[nodiscard]] inline float winding_number(std::span<const glm::vec2> path) noexcept
{
    return signed_turning(path) / (2.0F * std::numbers::pi_v<float>);
}

/**
 * @brief Signed area enclosed by treating the path as a closed polygon
 * @param path Positions in chronological order, at least three
 * @return Shoelace area, positive for counterclockwise winding
 *
 * Implicitly closes the path from its last point back to its first.
 * Distinct from signed_turning as a direction measure: turning responds
 * to the sequence of headings and stays large for a tight scribble that
 * encloses nothing, while this responds to enclosure and stays near
 * zero for a path that doubles back over itself regardless of how much
 * it turned doing so.
 */
[[nodiscard]] inline float signed_area(std::span<const glm::vec2> path) noexcept
{
    if (path.size() < 3)
        return 0.0F;

    float acc = 0.0F;
    for (size_t i = 0; i < path.size(); ++i) {
        const glm::vec2& a = path[i];
        const glm::vec2& b = path[(i + 1) % path.size()];
        acc += cross_2d(a, b);
    }
    return 0.5F * acc;
}

/**
 * @struct TurningProfile
 * @brief A path's heading as a function of arc length, at a fixed
 *        sample count.
 *
 * The shape descriptor. Sampling heading at evenly spaced positions
 * along the path discards where the path was drawn and how fast, and
 * retains only how it bent, so two strokes of the same shape at
 * different sizes in different corners of the space produce the same
 * profile. Comparing shapes then reduces to comparing two equal-length
 * vectors of angles.
 *
 * Headings are unwrapped rather than confined to a principal range, so
 * a path that turns through more than one revolution reads as
 * continuing to climb rather than folding back on itself. A profile
 * built with rotation invariance has its first heading subtracted from
 * every sample, which makes a shape match regardless of the direction
 * it was started in; without it, orientation is part of the identity of
 * the shape, and the same arc drawn upward and downward are different
 * things. Which is correct is a decision about the meaning being
 * defined, not a property of the mathematics, so it is a parameter.
 *
 * initial_heading and length are retained rather than discarded so a
 * caller can reintroduce the orientation and scale the profile threw
 * away, either as separate axes of a feature vector or to recover the
 * approximate original.
 */
struct TurningProfile {
    std::vector<float> turning; ///< Unwrapped heading at each arc-length sample.
    float initial_heading { 0.0F }; ///< Heading of the first segment, before any invariance subtraction.
    float length { 0.0F }; ///< Total arc length of the source path.
};

/**
 * @brief Build a turning profile from a path
 * @param path Positions in chronological order, at least three
 * @param samples Profile length, minimum 3
 * @param rotation_invariant Subtract the initial heading from every
 *        sample, making the profile independent of which direction the
 *        path was started in
 * @return A profile of exactly @p samples entries
 *
 * Resamples to uniform arc length first, so the profile is indexed by
 * distance along the path rather than by time, and two recordings of
 * the same shape at different frame rates produce comparable profiles.
 */
[[nodiscard]] inline TurningProfile turning_profile(
    std::span<const glm::vec2> path, size_t samples, bool rotation_invariant = true)
{
    samples = samples < 3 ? 3 : samples;

    TurningProfile profile;
    profile.turning.assign(samples - 1, 0.0F);

    if (path.size() < 2)
        return profile;

    for (size_t i = 1; i < path.size(); ++i)
        profile.length += glm::length(path[i] - path[i - 1]);

    const std::vector<glm::vec2> even = resample_uniform(path, samples);

    float unwrapped = heading(even[1] - even[0]);
    profile.initial_heading = unwrapped;
    profile.turning[0] = unwrapped;

    float prev = unwrapped;
    for (size_t i = 2; i < even.size(); ++i) {
        const float h = heading(even[i] - even[i - 1]);
        unwrapped += angular_delta(h, prev);
        prev = h;
        profile.turning[i - 1] = unwrapped;
    }

    if (rotation_invariant) {
        for (auto& t : profile.turning)
            t -= profile.initial_heading;
    }

    return profile;
}

/**
 * @brief Root mean square difference between two turning profiles
 * @param a Left profile
 * @param b Right profile, same sample count as @p a
 * @return RMS angular difference in radians, or infinity if the sample
 *         counts differ
 *
 * In radians, so the result is directly interpretable: a value near
 * zero is the same shape, a value near pi is a shape bending the
 * opposite way at every point. Profiles of different lengths are not
 * comparable and return infinity rather than silently truncating,
 * since a partial comparison of two shapes is not a weaker answer but
 * a wrong one.
 */
[[nodiscard]] inline float profile_distance(
    const TurningProfile& a, const TurningProfile& b) noexcept
{
    if (a.turning.size() != b.turning.size() || a.turning.empty())
        return std::numeric_limits<float>::infinity();

    float acc = 0.0F;
    for (size_t i = 0; i < a.turning.size(); ++i) {
        const float d = a.turning[i] - b.turning[i];
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<float>(a.turning.size()));
}

/**
 * @brief Centre a path at the origin and scale it to unit spread
 * @param path Positions in chronological order
 * @return Path translated so its centroid is at the origin and scaled
 *         so its root mean square distance from the origin is one
 *
 * The alternative shape normalization to a turning profile. A profile
 * compares how paths bent; this compares where their points are, once
 * position and size are removed. Point comparison keeps information
 * about proportion that turning discards, and unlike turning it stays
 * well defined for paths with stationary stretches where no heading
 * exists. Rotation is not removed, so orientation remains part of the
 * identity of the shape under this normalization.
 */
[[nodiscard]] inline std::vector<glm::vec2> normalize_shape(
    std::span<const glm::vec2> path)
{
    std::vector<glm::vec2> out(path.begin(), path.end());
    if (out.empty())
        return out;

    glm::vec2 mean { 0.0F };
    for (const auto& p : out)
        mean += p;
    mean /= static_cast<float>(out.size());

    float acc = 0.0F;
    for (auto& p : out) {
        p -= mean;
        acc += glm::dot(p, p);
    }

    const float rms = std::sqrt(acc / static_cast<float>(out.size()));
    if (rms < 1e-6F)
        return out;

    for (auto& p : out)
        p /= rms;
    return out;
}

/**
 * @brief Root mean square point distance between two paths after
 *        resampling and normalization
 * @param a Left path in chronological order
 * @param b Right path in chronological order
 * @param samples Point count both paths are resampled to
 * @return RMS distance in normalized units
 *
 * Resamples both to the same arc-length spacing, removes position and
 * scale from each, and compares point against point. Sensitive to
 * orientation, unlike a rotation-invariant turning profile, and
 * sensitive to proportion, unlike turning in general.
 */
[[nodiscard]] inline float shape_distance(
    std::span<const glm::vec2> a, std::span<const glm::vec2> b, size_t samples = 32)
{
    const std::vector<glm::vec2> ra = normalize_shape(resample_uniform(a, samples));
    const std::vector<glm::vec2> rb = normalize_shape(resample_uniform(b, samples));
    if (ra.size() != rb.size() || ra.empty())
        return std::numeric_limits<float>::infinity();

    float acc = 0.0F;
    for (size_t i = 0; i < ra.size(); ++i) {
        const glm::vec2 d = ra[i] - rb[i];
        acc += glm::dot(d, d);
    }
    return std::sqrt(acc / static_cast<float>(ra.size()));
}

/**
 * @brief Dynamic time warping cost between two sequences
 * @tparam T Element type
 * @param a Left sequence
 * @param b Right sequence
 * @param distance Pointwise cost between one element of each
 * @param band Sakoe-Chiba radius: the furthest an alignment may stray
 *        from the diagonal, in elements. Zero means unconstrained
 * @return Accumulated cost of the cheapest monotone alignment, or
 *         infinity if either sequence is empty or the band admits no
 *         complete alignment
 *
 * Resampling by arc length removes speed variation from a path by
 * discarding time entirely, which is right when only shape matters and
 * wrong when the quantity being compared is not positional. This
 * instead keeps both sequences and finds the cheapest correspondence
 * between them, allowing one to stretch against the other, so two
 * recordings of the same thing performed at different tempos align
 * rather than disagree at every step.
 *
 * The band is the difference between a bounded cost and a quadratic
 * one, and it also encodes an assumption: a nonzero band asserts that
 * the two sequences are roughly in step and only locally out of it. A
 * band narrower than the genuine offset between two sequences reports
 * infinity rather than a poor alignment, which is the honest answer
 * given the constraint it was handed.
 */
template <typename T>
[[nodiscard]] inline float dtw_cost(
    std::span<const T> a,
    std::span<const T> b,
    const std::function<float(const T&, const T&)>& distance,
    size_t band = 0)
{
    if (a.empty() || b.empty())
        return std::numeric_limits<float>::infinity();

    const size_t n = a.size();
    const size_t m = b.size();
    constexpr float inf = std::numeric_limits<float>::infinity();

    std::vector<float> prev(m + 1, inf);
    std::vector<float> curr(m + 1, inf);
    prev[0] = 0.0F;

    for (size_t i = 1; i <= n; ++i) {
        std::ranges::fill(curr, inf);

        size_t lo = 1;
        size_t hi = m;
        if (band > 0) {
            const auto centre = static_cast<size_t>(
                (static_cast<double>(i) * static_cast<double>(m)) / static_cast<double>(n));
            lo = (centre > band) ? (centre - band) : 1;
            hi = std::min(m, centre + band);
            if (lo > hi)
                return inf;
        }

        for (size_t j = lo; j <= hi; ++j) {
            const float cost = distance(a[i - 1], b[j - 1]);
            const float best = std::min({ prev[j], curr[j - 1], prev[j - 1] });
            curr[j] = (best == inf) ? inf : (cost + best);
        }
        std::swap(prev, curr);
    }

    return prev[m];
}

} // namespace MayaFlux::Kinesis
