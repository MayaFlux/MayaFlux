#pragma once

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

// =============================================================================
// General finite differences
// =============================================================================

// scalar_t<T> now comes from the PCH type system (MayaFlux::scalar_t),
// resolving to T for plain arithmetic types and to glm_component_type<T>
// for GlmType T. Used unqualified below via MayaFlux namespace lookup.

/**
 * @brief N-th order backward finite difference of a HistoryBuffer, scaled by dt^N
 * @tparam N Difference order. 1 velocity, 2 acceleration, 3 jerk, 4 jounce, 5 crackle, 6 pop.
 * @tparam T Sample type. Must support operator+, operator-, and
 *           multiplication/division by scalar_t<T>.
 * @param history Buffer with capacity >= N + 1, [0] newest through [N] oldest used
 * @param dt Elapsed time between consecutive samples, assumed uniform across
 *           all N intervals
 * @return sum_{k=0}^{N} (-1)^k * C(N,k) * history[k], divided by dt^N
 *
 * Named wrappers below (velocity, acceleration, jerk, jounce, crackle, pop)
 * are instantiations of this at fixed N. A caller needing N = 7 or higher
 * calls backward_difference<7>(history, dt) directly.
 *
 * dt == 0 is not guarded: the correct response depends on the caller's
 * domain, so this stays a pure computation.
 *
 * Uniform dt across all N intervals is an assumption, not a check.
 * HistoryBuffer carries no per-sample timestamps.
 *
 * HistoryBuffer::operator[] indexes modulo capacity, not modulo the
 * number of samples actually pushed. Querying history[k] before k
 * samples have been pushed reads the zero initial condition.
 */
template <size_t N, typename T>
[[nodiscard]] inline T backward_difference(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    static_assert(N >= 1, "backward_difference<N> requires N >= 1; N = 0 is the sample itself");

    using S = scalar_t<T>;

    T acc = history[0];
    double binomial = 1.0;

    for (size_t k = 1; k <= N; ++k) {
        binomial = binomial * static_cast<double>(N - k + 1) / static_cast<double>(k);
        const double sign = (k % 2 == 0) ? 1.0 : -1.0;
        acc = acc + static_cast<S>(sign * binomial) * history[k];
    }

    double denom = 1.0;
    for (size_t i = 0; i < N; ++i)
        denom *= dt;

    return acc / static_cast<S>(denom);
}

/**
 * @brief N-th order forward finite difference, expressed on a HistoryBuffer
 * @tparam N Difference order
 * @tparam T Sample type
 * @param history Buffer with capacity >= N + 1
 * @param dt Elapsed time between consecutive samples
 * @return Forward-difference formula evaluated with the newest sample
 *         treated as the base point and older samples as the forward taps
 *
 * Mathematically the forward and backward difference formulas are the
 * same stencil read in opposite temporal direction. Since a HistoryBuffer
 * only ever exposes past samples relative to [0], this computes the
 * forward-difference coefficients but applied to the same available data
 * as backward_difference. The two differ only for even N in sign
 * convention on alternating terms; provided for callers whose downstream
 * math was derived against forward-difference tables.
 */
template <size_t N, typename T>
[[nodiscard]] inline T forward_difference(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    static_assert(N >= 1, "forward_difference<N> requires N >= 1");

    using S = scalar_t<T>;

    T acc = T {};
    double binomial = 1.0;

    for (size_t k = 0; k <= N; ++k) {
        if (k > 0)
            binomial = binomial * static_cast<double>(N - k + 1) / static_cast<double>(k);
        const double sign = ((N - k) % 2 == 0) ? 1.0 : -1.0;
        acc = acc + static_cast<S>(sign * binomial) * history[k];
    }

    double denom = 1.0;
    for (size_t i = 0; i < N; ++i)
        denom *= dt;

    return acc / static_cast<S>(denom);
}

/**
 * @brief Central difference approximation of the first derivative
 * @tparam T Sample type
 * @param history Buffer with capacity >= 3
 * @param dt Elapsed time between consecutive samples
 * @return (history[0] - history[2]) / (2 * dt)
 *
 * Second-order accurate versus the first-order accurate backward
 * difference used by velocity(). Costs one extra sample of lag since
 * the estimate is centered on history[1], not history[0].
 */
template <typename T>
[[nodiscard]] inline T central_first_derivative(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    using S = scalar_t<T>;
    return (history[0] - history[2]) / static_cast<S>(2.0 * dt);
}

/**
 * @brief Central difference approximation of the second derivative
 * @tparam T Sample type
 * @param history Buffer with capacity >= 3
 * @param dt Elapsed time between consecutive samples
 * @return (history[0] - 2*history[1] + history[2]) / dt^2
 *
 * Identical stencil to acceleration() since the standard central second
 * difference and backward second difference coincide at this order.
 * Provided as a named alias so call sites documenting derivation against
 * central-difference tables do not need to reach for acceleration().
 */
template <typename T>
[[nodiscard]] inline T central_second_derivative(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<2>(history, dt);
}

// =============================================================================
// Named kinematic orders
// =============================================================================

/** @brief First difference: rate of change of position. Capacity >= 2. */
template <typename T>
[[nodiscard]] inline T velocity(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<1>(history, dt);
}

/** @brief Second difference: rate of change of velocity. Capacity >= 3. */
template <typename T>
[[nodiscard]] inline T acceleration(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<2>(history, dt);
}

/**
 * @brief Third difference: rate of change of acceleration. Capacity >= 4.
 *
 * The gesture-relevant reading is abruptness: a stroke that suddenly
 * changes how it is accelerating, distinct from acceleration itself
 * which only says the speed is changing.
 */
template <typename T>
[[nodiscard]] inline T jerk(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<3>(history, dt);
}

/** @brief Fourth difference: rate of change of jerk. Capacity >= 5. */
template <typename T>
[[nodiscard]] inline T jounce(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<4>(history, dt);
}

/** @brief Fifth difference: rate of change of jounce. Capacity >= 6. */
template <typename T>
[[nodiscard]] inline T crackle(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<5>(history, dt);
}

/** @brief Sixth difference: rate of change of crackle. Capacity >= 7. */
template <typename T>
[[nodiscard]] inline T pop(const Memory::HistoryBuffer<T>& history, double dt) noexcept
{
    return backward_difference<6>(history, dt);
}

// =============================================================================
// Smoothed differences
// =============================================================================

/**
 * @brief Velocity averaged over a short window rather than a single interval
 * @tparam T Sample type
 * @param history Buffer with capacity >= window + 1
 * @param dt Elapsed time between consecutive samples
 * @param window Number of intervals to average over, minimum 1
 * @return Mean of the per-interval first differences across the window
 *
 * A single backward_difference<1> reading is sensitive to jitter on any
 * one sample pair. Averaging several consecutive one-step differences
 * trades responsiveness for stability, useful when the source stream
 * (tablet, camera-derived tracking) carries sensor noise that would
 * otherwise alias into a spurious high-frequency acceleration or jerk
 * reading downstream.
 */
template <typename T>
[[nodiscard]] inline T smoothed_velocity(const Memory::HistoryBuffer<T>& history, double dt, size_t window) noexcept
{
    using S = scalar_t<T>;
    window = window < 1 ? 1 : window;
    const auto s_dt = static_cast<S>(dt);
    T acc = (history[0] - history[1]) / s_dt;
    for (size_t i = 1; i < window; ++i)
        acc = acc + (history[i] - history[i + 1]) / s_dt;
    return acc / static_cast<S>(window);
}

/**
 * @brief Simple moving average over the newest @p window samples
 * @tparam T Sample type
 * @param history Buffer with capacity >= window
 * @param window Number of samples to average, minimum 1
 * @return Mean of history[0..window-1]
 *
 * Not a derivative. Included alongside the differential family because
 * a common pattern is smoothing the raw signal before differentiating it
 * rather than smoothing the derivative after the fact; the two produce
 * different noise characteristics and callers should be able to reach
 * for either without leaving this file.
 */
template <typename T>
[[nodiscard]] inline T moving_average(const Memory::HistoryBuffer<T>& history, size_t window) noexcept
{
    using S = scalar_t<T>;
    window = window < 1 ? 1 : window;
    T acc = history[0];
    for (size_t i = 1; i < window; ++i)
        acc = acc + history[i];
    return acc / static_cast<S>(window);
}

// =============================================================================
// Magnitude and direction (scalar T)
// =============================================================================

/**
 * @brief Absolute value of a scalar first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return |velocity|, direction discarded
 */
[[nodiscard]] inline double speed(const Memory::HistoryBuffer<double>& history, double dt) noexcept
{
    return std::abs(velocity(history, dt));
}

/**
 * @brief Sign of a scalar first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return -1.0, 0.0, or 1.0
 */
[[nodiscard]] inline double direction_sign(const Memory::HistoryBuffer<double>& history, double dt) noexcept
{
    const double v = velocity(history, dt);
    return (v > 0.0) ? 1.0 : ((v < 0.0) ? -1.0 : 0.0);
}

// =============================================================================
// Magnitude and direction (glm::vec2 / glm::vec3)
// =============================================================================

/**
 * @brief Magnitude of a vec2 first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return Euclidean speed
 */
[[nodiscard]] inline float speed(const Memory::HistoryBuffer<glm::vec2>& history, double dt) noexcept
{
    return glm::length(velocity(history, dt));
}

/**
 * @brief Magnitude of a vec3 first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return Euclidean speed
 */
[[nodiscard]] inline float speed(const Memory::HistoryBuffer<glm::vec3>& history, double dt) noexcept
{
    return glm::length(velocity(history, dt));
}

/**
 * @brief Unit direction of a vec2 first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return Normalized velocity, or the zero vector when speed is
 *         below 1e-6 to avoid dividing by zero on a stationary point
 */
[[nodiscard]] inline glm::vec2 heading_vector(const Memory::HistoryBuffer<glm::vec2>& history, double dt) noexcept
{
    const glm::vec2 v = velocity(history, dt);
    const float len = glm::length(v);
    return (len > 1e-6F) ? (v / len) : glm::vec2(0.0F);
}

/**
 * @brief Unit direction of a vec3 first difference
 * @param history Buffer with capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return Normalized velocity, or the zero vector when speed is
 *         below 1e-6 to avoid dividing by zero on a stationary point
 */
[[nodiscard]] inline glm::vec3 heading_vector(const Memory::HistoryBuffer<glm::vec3>& history, double dt) noexcept
{
    const glm::vec3 v = velocity(history, dt);
    const float len = glm::length(v);
    return (len > 1e-6F) ? (v / len) : glm::vec3(0.0F);
}

// =============================================================================
// Angular quantities (glm::vec2)
// =============================================================================

/**
 * @brief Heading angle of a 2D vector
 * @param v Any vector, typically a velocity
 * @return atan2(v.y, v.x) in radians, range (-pi, pi]
 */
[[nodiscard]] inline float heading(const glm::vec2& v) noexcept
{
    return std::atan2(v.y, v.x);
}

/**
 * @brief Shortest signed angular difference between two headings
 * @param to Target angle in radians
 * @param from Source angle in radians
 * @return Signed difference in (-pi, pi], wrapped correctly across the seam
 *
 * Plain subtraction (to - from) is wrong whenever the pair straddles the
 * -pi/pi boundary, e.g. from = 3.0, to = -3.0 is a small turn, not a turn
 * of nearly 2*pi. This wraps into the shortest equivalent angle first.
 */
[[nodiscard]] inline float angular_delta(float to, float from) noexcept
{
    float delta = to - from;
    const float two_pi = 2.0F * std::numbers::pi_v<float>;
    delta = std::fmod(delta + std::numbers::pi_v<float>, two_pi);
    if (delta < 0.0F)
        delta += two_pi;
    return delta - std::numbers::pi_v<float>;
}

/**
 * @brief Angular velocity from consecutive headings in a HistoryBuffer
 * @param headings Buffer of heading angles in radians, capacity >= 2
 * @param dt Elapsed time between samples 0 and 1
 * @return Signed rate of heading change in radians per second, wrapped
 *         correctly across the -pi/pi seam
 *
 * Takes a HistoryBuffer<float> of already-computed headings rather than
 * positions directly, since heading is itself derived (see heading()
 * above) and keeping this function ignorant of that derivation keeps it
 * reusable for any angle-producing source, not only motion.
 */
[[nodiscard]] inline float angular_velocity(const Memory::HistoryBuffer<float>& headings, double dt) noexcept
{
    return angular_delta(headings[0], headings[1]) / static_cast<float>(dt);
}

/**
 * @brief 2D scalar cross product, sign of turn direction
 * @param a First vector
 * @param b Second vector
 * @return a.x*b.y - a.y*b.x. Positive is CCW from a to b, negative CW.
 */
[[nodiscard]] inline float cross_2d(const glm::vec2& a, const glm::vec2& b) noexcept
{
    return a.x * b.y - a.y * b.x;
}

/**
 * @brief Signed curvature from velocity and acceleration
 * @param vel Velocity vector
 * @param accel Acceleration vector
 * @return (vx*ay - vy*ax) / |v|^3, or 0 when speed is below 1e-6
 *
 * dtheta/ds rather than dtheta/dt: speed-independent, so a sharp corner
 * drawn slowly and the same corner drawn fast report the same curvature,
 * unlike angular_velocity which conflates turn sharpness with pace.
 * Positive is a leftward (CCW) bend, negative rightward.
 */
[[nodiscard]] inline float curvature(const glm::vec2& vel, const glm::vec2& accel) noexcept
{
    const float speed_sq = glm::dot(vel, vel);
    const float spd = std::sqrt(speed_sq);
    if (spd < 1e-6F)
        return 0.0F;
    return cross_2d(vel, accel) / (speed_sq * spd);
}

/**
 * @brief Signed curvature computed directly from a position HistoryBuffer
 * @param history Buffer of positions with capacity >= 3
 * @param dt Elapsed time between consecutive samples
 * @return curvature(velocity(history, dt), acceleration(history, dt))
 *
 * Convenience wrapper chaining the two differences a caller would
 * otherwise compute separately before calling curvature() above.
 */
[[nodiscard]] inline float curvature_from_history(const Memory::HistoryBuffer<glm::vec2>& history, double dt) noexcept
{
    return curvature(velocity(history, dt), acceleration(history, dt));
}

// =============================================================================
// Windowed path-shape measures (glm::vec2)
// =============================================================================

/**
 * @brief Path length accumulated across the newest @p window positions
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples spanning window - 1 segments, minimum 2
 * @return Sum of consecutive segment lengths
 */
[[nodiscard]] inline float path_length(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    window = window < 2 ? 2 : window;
    float len = 0.0F;
    for (size_t i = 0; i + 1 < window; ++i)
        len += glm::length(history[i] - history[i + 1]);
    return len;
}

/**
 * @brief Net displacement across the newest @p window positions
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples, minimum 2
 * @return Straight-line distance from history[window-1] to history[0]
 */
[[nodiscard]] inline float net_displacement(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    window = window < 2 ? 2 : window;
    return glm::length(history[0] - history[window - 1]);
}

/**
 * @brief Straightness of a path over a window, 1.0 is a straight line
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples, minimum 2
 * @return net_displacement / path_length, or 0 when path_length is
 *         below 1e-6 to avoid dividing by a stationary point
 *
 * A gesture that doubles back on itself drives this toward 0 even
 * though individual segment speeds may be high; a gesture that moves
 * directly toward one point holds this near 1.0 regardless of speed.
 */
[[nodiscard]] inline float straightness(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    const float len = path_length(history, window);
    if (len < 1e-6F)
        return 0.0F;
    return net_displacement(history, window) / len;
}

/**
 * @brief Total absolute turning accumulated across a window
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples, minimum 3
 * @return Sum of |angular_delta| between consecutive segment headings
 *
 * A jitteriness measure distinct from curvature at a point: a path that
 * wiggles back and forth accumulates large total turning even if its net
 * curvature at any single sample is small, since positive and negative
 * bends do not cancel here the way they would in a single derivative.
 */
[[nodiscard]] inline float total_turning(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    window = window < 3 ? 3 : window;
    float total = 0.0F;
    float prev_heading = heading(history[0] - history[1]);
    for (size_t i = 1; i + 1 < window; ++i) {
        const float h = heading(history[i] - history[i + 1]);
        total += std::abs(angular_delta(h, prev_heading));
        prev_heading = h;
    }
    return total;
}

/**
 * @brief Mean position across a window
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples, minimum 1
 * @return Centroid of history[0..window-1]
 */
[[nodiscard]] inline glm::vec2 centroid(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    return moving_average(history, window);
}

/**
 * @brief Bounding radius of a window around its centroid
 * @param history Buffer of positions, capacity >= window
 * @param window Number of samples, minimum 1
 * @return Maximum distance from centroid to any sample in the window
 *
 * A cheap containment/extent measure: how large a circle a gesture
 * currently occupies, independent of how much path length it traced
 * to get there. A tight scribble and a single large slow circle can
 * have similar path_length but very different spread.
 */
[[nodiscard]] inline float spread_radius(const Memory::HistoryBuffer<glm::vec2>& history, size_t window) noexcept
{
    const glm::vec2 c = centroid(history, window);
    float max_dist = 0.0F;
    for (size_t i = 0; i < window; ++i)
        max_dist = std::max(max_dist, glm::length(history[i] - c));
    return max_dist;
}

// =============================================================================
// Convenience overloads: raw sample spans, HistoryBuffer built inline
//
// Every function above takes a HistoryBuffer<T>& because that is the
// correct long-lived object for a caller pushing one sample per frame.
// These overloads exist for the other common shape: a caller already
// holding a handful of samples in a plain span (a small stack array
// pulled off a queue, a window sliced from a larger recording, values
// read straight out of TabletContext::frame) who does not want to stand
// up a HistoryBuffer just to call one of the functions above once.
//
// Ordering matches HistoryBuffer convention: samples[0] is newest,
// samples[k] is k steps back. A HistoryBuffer is constructed with
// capacity equal to samples.size() and pushed in reverse so that
// history[0] ends up holding samples[0], matching what the caller
// would get from pushing samples live in newest-last arrival order.
// =============================================================================

/**
 * @brief Build a HistoryBuffer<T> from a newest-first span
 * @tparam T Sample type
 * @param samples Span with samples[0] newest, samples[N-1] oldest
 * @return HistoryBuffer<T> of capacity samples.size() with matching contents
 *
 * Exposed directly since several call sites below only need this step
 * once before calling a HistoryBuffer<T>& overload repeatedly, e.g.
 * comparing several difference orders against the same window without
 * rebuilding it per call.
 */
template <typename T>
[[nodiscard]] inline Memory::HistoryBuffer<T> to_history(std::span<const T> samples) noexcept
{
    Memory::HistoryBuffer<T> history(samples.size());
    for (size_t i = samples.size(); i-- > 0;)
        history.push(samples[i]);
    return history;
}

/** @brief Convenience overload of backward_difference<N> over a raw span. */
template <size_t N, typename T>
[[nodiscard]] inline T backward_difference(std::span<const T> samples, double dt) noexcept
{
    return backward_difference<N>(to_history(samples), dt);
}

/** @brief Convenience overload of forward_difference<N> over a raw span. */
template <size_t N, typename T>
[[nodiscard]] inline T forward_difference(std::span<const T> samples, double dt) noexcept
{
    return forward_difference<N>(to_history(samples), dt);
}

/** @brief Convenience overload of central_first_derivative over a raw span. */
template <typename T>
[[nodiscard]] inline T central_first_derivative(std::span<const T> samples, double dt) noexcept
{
    return central_first_derivative(to_history(samples), dt);
}

/** @brief Convenience overload of central_second_derivative over a raw span. */
template <typename T>
[[nodiscard]] inline T central_second_derivative(std::span<const T> samples, double dt) noexcept
{
    return central_second_derivative(to_history(samples), dt);
}

/** @brief Convenience overload of velocity over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T velocity(std::span<const T> samples, double dt) noexcept
{
    return velocity(to_history(samples), dt);
}

/** @brief Convenience overload of acceleration over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T acceleration(std::span<const T> samples, double dt) noexcept
{
    return acceleration(to_history(samples), dt);
}

/** @brief Convenience overload of jerk over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T jerk(std::span<const T> samples, double dt) noexcept
{
    return jerk(to_history(samples), dt);
}

/** @brief Convenience overload of jounce over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T jounce(std::span<const T> samples, double dt) noexcept
{
    return jounce(to_history(samples), dt);
}

/** @brief Convenience overload of crackle over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T crackle(std::span<const T> samples, double dt) noexcept
{
    return crackle(to_history(samples), dt);
}

/** @brief Convenience overload of pop over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T pop(std::span<const T> samples, double dt) noexcept
{
    return pop(to_history(samples), dt);
}

/** @brief Convenience overload of smoothed_velocity over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T smoothed_velocity(std::span<const T> samples, double dt, size_t window) noexcept
{
    return smoothed_velocity(to_history(samples), dt, window);
}

/** @brief Convenience overload of moving_average over a raw span, samples[0] newest. */
template <typename T>
[[nodiscard]] inline T moving_average(std::span<const T> samples, size_t window) noexcept
{
    return moving_average(to_history(samples), window);
}

/** @brief Convenience overload of speed(double) over a raw span, samples[0] newest. */
[[nodiscard]] inline double speed(std::span<const double> samples, double dt) noexcept
{
    return speed(to_history(samples), dt);
}

/** @brief Convenience overload of direction_sign over a raw span, samples[0] newest. */
[[nodiscard]] inline double direction_sign(std::span<const double> samples, double dt) noexcept
{
    return direction_sign(to_history(samples), dt);
}

/** @brief Convenience overload of speed(vec2) over a raw span, samples[0] newest. */
[[nodiscard]] inline float speed(std::span<const glm::vec2> samples, double dt) noexcept
{
    return speed(to_history(samples), dt);
}

/** @brief Convenience overload of speed(vec3) over a raw span, samples[0] newest. */
[[nodiscard]] inline float speed(std::span<const glm::vec3> samples, double dt) noexcept
{
    return speed(to_history(samples), dt);
}

/** @brief Convenience overload of heading_vector(vec2) over a raw span, samples[0] newest. */
[[nodiscard]] inline glm::vec2 heading_vector(std::span<const glm::vec2> samples, double dt) noexcept
{
    return heading_vector(to_history(samples), dt);
}

/** @brief Convenience overload of heading_vector(vec3) over a raw span, samples[0] newest. */
[[nodiscard]] inline glm::vec3 heading_vector(std::span<const glm::vec3> samples, double dt) noexcept
{
    return heading_vector(to_history(samples), dt);
}

/** @brief Convenience overload of angular_velocity over a raw span of headings, samples[0] newest. */
[[nodiscard]] inline float angular_velocity(std::span<const float> headings, double dt) noexcept
{
    return angular_velocity(to_history(headings), dt);
}

/** @brief Convenience overload of curvature_from_history over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float curvature_from_history(std::span<const glm::vec2> samples, double dt) noexcept
{
    return curvature_from_history(to_history(samples), dt);
}

/** @brief Convenience overload of path_length over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float path_length(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return path_length(to_history(samples), window);
}

/** @brief Convenience overload of net_displacement over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float net_displacement(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return net_displacement(to_history(samples), window);
}

/** @brief Convenience overload of straightness over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float straightness(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return straightness(to_history(samples), window);
}

/** @brief Convenience overload of total_turning over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float total_turning(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return total_turning(to_history(samples), window);
}

/** @brief Convenience overload of centroid over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline glm::vec2 centroid(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return centroid(to_history(samples), window);
}

/** @brief Convenience overload of spread_radius over a raw span of positions, samples[0] newest. */
[[nodiscard]] inline float spread_radius(std::span<const glm::vec2> samples, size_t window) noexcept
{
    return spread_radius(to_history(samples), window);
}

} // namespace MayaFlux::Kinesis
