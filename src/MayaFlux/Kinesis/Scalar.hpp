#pragma once

namespace MayaFlux::Kinesis {

// =============================================================================
// Range mapping
// =============================================================================

/**
 * @brief Map @p x from [in_lo, in_hi] to [out_lo, out_hi], unclamped.
 *
 * Values outside [in_lo, in_hi] extrapolate linearly. Use map_clamped()
 * when the output must stay within [out_lo, out_hi].
 *
 * Degenerate input range (in_lo == in_hi) returns out_lo.
 */
template <typename T>
[[nodiscard]] constexpr T map(T x, T in_lo, T in_hi, T out_lo, T out_hi) noexcept
{
    const T in_range = in_hi - in_lo;
    if (in_range == T { 0 })
        return out_lo;
    return out_lo + (x - in_lo) / in_range * (out_hi - out_lo);
}

/**
 * @brief Map @p x from [in_lo, in_hi] to [out_lo, out_hi], clamped to
 *        [out_lo, out_hi].
 */
template <typename T>
[[nodiscard]] constexpr T map_clamped(T x, T in_lo, T in_hi, T out_lo, T out_hi) noexcept
{
    const T v = map(x, in_lo, in_hi, out_lo, out_hi);
    return std::clamp(v, std::min(out_lo, out_hi), std::max(out_lo, out_hi));
}

/**
 * @brief Normalize @p x in [lo, hi] to [0, 1], unclamped.
 *
 * Distinct from Kinesis::Discrete::normalize, which operates in-place on
 * a span and computes its own min/max. This is the scalar equivalent.
 *
 * Degenerate range (lo == hi) returns 0.
 */
template <typename T>
[[nodiscard]] constexpr T normalize(T x, T lo, T hi) noexcept
{
    const T range = hi - lo;
    if (range == T { 0 })
        return T { 0 };
    return (x - lo) / range;
}

/**
 * @brief Denormalize @p t from [0, 1] to [lo, hi].
 *
 * Equivalent to glm::mix(lo, hi, t) but named for legibility at call sites
 * where the intent is range reconstruction rather than interpolation.
 */
template <typename T>
[[nodiscard]] constexpr T denormalize(T t, T lo, T hi) noexcept
{
    return lo + t * (hi - lo);
}

// =============================================================================
// Smoothing
// =============================================================================

/**
 * @brief GLSL smoothstep: Hermite interpolation in [lo, hi].
 *
 * Returns 0 when x <= lo, 1 when x >= hi, and a smooth cubic curve
 * in between. Equivalent to GLSL smoothstep(lo, hi, x).
 *
 * GLM provides this as glm::smoothstep but requires glm/gtx/compatibility.hpp
 * and a vector type. This overload accepts any scalar T.
 */
template <typename T>
[[nodiscard]] constexpr T smoothstep(T lo, T hi, T x) noexcept
{
    const T t = std::clamp((x - lo) / (hi - lo), T { 0 }, T { 1 });
    return t * t * (T { 3 } - T { 2 } * t);
}

/**
 * @brief Ken Perlin's quintic smootherstep: C2-continuous in [lo, hi].
 *
 * Derivative is zero at both endpoints (unlike smoothstep which has zero
 * first derivative only). Use when second-derivative continuity matters:
 * camera motion, envelope transitions, SDF blending weights.
 */
template <typename T>
[[nodiscard]] constexpr T smootherstep(T lo, T hi, T x) noexcept
{
    const T t = std::clamp((x - lo) / (hi - lo), T { 0 }, T { 1 });
    return t * t * t * (t * (t * T { 6 } - T { 15 }) + T { 10 });
}

/**
 * @brief Exponential smoothing: move @p current toward @p target at rate
 *        @p smoothing over time step @p dt.
 *
 * @p smoothing is a half-life in seconds: the distance to target halves
 * every @p smoothing seconds. At smoothing = 0.1 the value tracks quickly;
 * at smoothing = 2.0 it lags heavily.
 *
 * Framerate-independent. Equivalent to the "lerp every frame" pattern
 * done correctly:
 *   current = lerp(current, target, 1 - exp(-dt / smoothing))
 *
 * Degenerate case (smoothing <= 0) snaps to target immediately.
 */
template <typename T>
[[nodiscard]] inline T damp(T current, T target, T smoothing, T dt) noexcept
{
    if (smoothing <= T { 0 })
        return target;
    return current + (target - current) * (T { 1 } - std::exp(-dt / smoothing));
}

// =============================================================================
// Wrapping and folding
// =============================================================================

/**
 * @brief Wrap @p x into [lo, hi) with modulo semantics.
 *
 * Unlike std::fmod, handles negative values and arbitrary lo correctly.
 * Equivalent to the GLSL mod() applied after shifting by lo.
 *
 * Example: wrap(-0.1F, 0.F, 1.F) -> 0.9F
 */
template <typename T>
[[nodiscard]] inline T wrap(T x, T lo, T hi) noexcept
{
    const T range = hi - lo;
    if (range <= T { 0 })
        return lo;
    return x - range * std::floor((x - lo) / range);
}

/**
 * @brief Ping-pong (triangle wave): fold @p x back and forth in [lo, hi].
 *
 * At x = lo the value is lo. It increases to hi then folds back toward lo.
 * Period is 2 * (hi - lo). Use for oscillating parameters, bounce animations,
 * or any signal that should reverse at the boundary instead of wrapping.
 *
 * Example: ping_pong(1.3F, 0.F, 1.F) -> 0.7F
 */
template <typename T>
[[nodiscard]] inline T ping_pong(T x, T lo, T hi) noexcept
{
    const T range = hi - lo;
    if (range <= T { 0 })
        return lo;
    const T shifted = x - lo;
    const T period = T { 2 } * range;
    const T t = shifted - period * std::floor(shifted / period);
    return lo + (t < range ? t : period - t);
}

// =============================================================================
// Snapping
// =============================================================================

/**
 * @brief Round @p x to the nearest multiple of @p step.
 *
 * std::round(x / step) * step but named. step must be > 0.
 *
 * Example: snap(0.73F, 0.25F) -> 0.75F
 */
template <typename T>
[[nodiscard]] inline T snap(T x, T step) noexcept
{
    if (step <= T { 0 })
        return x;
    return std::round(x / step) * step;
}

/**
 * @brief Snap @p x to the nearest multiple of @p step within [lo, hi].
 */
template <typename T>
[[nodiscard]] inline T snap(T x, T lo, T hi, T step) noexcept
{
    return std::clamp(snap(x, step), lo, hi);
}

// =============================================================================
// Easing
// =============================================================================

/**
 * @brief Cubic ease-in: slow start, fast end.
 * @param t Normalized time in [0, 1].
 */
template <typename T>
[[nodiscard]] constexpr T ease_in(T t) noexcept
{
    return t * t * t;
}

/**
 * @brief Cubic ease-out: fast start, slow end.
 * @param t Normalized time in [0, 1].
 */
template <typename T>
[[nodiscard]] constexpr T ease_out(T t) noexcept
{
    const T u = T { 1 } - t;
    return T { 1 } - u * u * u;
}

/**
 * @brief Cubic ease-in-out: slow start, fast middle, slow end.
 * @param t Normalized time in [0, 1].
 */
template <typename T>
[[nodiscard]] constexpr T ease_in_out(T t) noexcept
{
    return t < T { 0.5 }
        ? T { 4 } * t * t * t
        : T { 1 } - T { 4 } * (T { 1 } - t) * (T { 1 } - t) * (T { 1 } - t) * T { 0.5 };
}

// =============================================================================
// Deadzone and sign
// =============================================================================

/**
 * @brief Zero values within [-threshold, threshold] and rescale the remainder
 *        to fill [0, 1] (or [-1, 1] for negative inputs).
 *
 * Standard controller/HID deadzone. Input and output are in [-1, 1].
 * threshold must be in [0, 1).
 *
 * Example: deadzone(0.1F, 0.2F) -> 0.0F  (inside dead zone)
 *          deadzone(0.6F, 0.2F) -> 0.5F  (rescaled from [0.2, 1.0] to [0, 1])
 */
template <typename T>
[[nodiscard]] constexpr T deadzone(T x, T threshold) noexcept
{
    if (std::abs(x) <= threshold)
        return T { 0 };
    const T sign = x > T { 0 } ? T { 1 } : T { -1 };
    return sign * (std::abs(x) - threshold) / (T { 1 } - threshold);
}

/**
 * @brief Sign of @p x, returning -1 or +1, never 0.
 *
 * std::copysign(1, x) but readable. Useful when zero must be treated as
 * positive (e.g. initial state of a direction accumulator).
 */
template <typename T>
[[nodiscard]] constexpr T sign_nonzero(T x) noexcept
{
    return x < T { 0 } ? T { -1 } : T { 1 };
}

} // namespace MayaFlux::Kinesis
