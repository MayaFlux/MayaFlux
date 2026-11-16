#pragma once

/**
 * @file RootFinding.hpp
 * @brief Roots of scalar functions held inside a bracket.
 */

namespace MayaFlux::Kinesis {

/**
 * @enum RootStatus
 * @brief How a root search ended.
 */
enum class RootStatus : uint8_t {
    CONVERGED,
    MAX_ITERATIONS,
    NO_BRACKET,
    NOT_FINITE
};

/** @brief Value and slope of a function at one point. */
using RootSample = std::pair<double, double>;

/** @brief The best point found and how the search ended. */
using RootResult = std::pair<double, RootStatus>;

/**
 * @struct RootOptions
 * @brief Stopping rule of a root search.
 *
 * The search stops when a step, or the bracket, is no wider than
 * tolerance times the larger of the iterate and floor.
 */
struct RootOptions {
    double tolerance { 1.0e-12 };
    double floor { 1.0e-14 };
    uint32_t max_iterations { 64 };
    bool check_bracket { true };
};

/**
 * @brief Root of an increasing function by Newton steps kept inside a bracket.
 * @param function Callable returning a RootSample, value then slope, for a point
 * @param lower Lower end of the bracket
 * @param upper Upper end of the bracket
 * @param guess Starting point, clamped into the bracket
 * @param options Stopping rule
 * @return The root and the status. On MAX_ITERATIONS the root is the last
 *         iterate, and on NO_BRACKET or NOT_FINITE it is the guess.
 *
 * Needs value(lower) <= 0 <= value(upper). A Newton step that leaves the
 * bracket, or comes from a slope that is not positive and finite, is replaced
 * by bisection, so the bracket always narrows and the search ends within
 * max_iterations. Near a root it converges quadratically, and a guess from the
 * previous call, when the function moves slowly between calls, usually
 * finishes in two or three evaluations.
 *
 * With options.check_bracket set, the ends are evaluated first and a missing
 * sign change returns NO_BRACKET. Turn it off when the bracket is known by
 * construction. A decreasing function is handled by negating it.
 */
template <typename Function>
    requires std::invocable<Function&, double>
    && std::convertible_to<std::invoke_result_t<Function&, double>, RootSample>
[[nodiscard]] RootResult bracketed_newton(
    Function function, double lower, double upper, double guess, const RootOptions& options = {})
{
    if (!(lower <= upper) || !std::isfinite(lower) || !std::isfinite(upper)) {
        return { guess, RootStatus::NO_BRACKET };
    }

    double lo = lower;
    double hi = upper;

    if (options.check_bracket) {
        const double at_lower = static_cast<RootSample>(function(lo)).first;
        const double at_upper = static_cast<RootSample>(function(hi)).first;
        if (!std::isfinite(at_lower) || !std::isfinite(at_upper)) {
            return { guess, RootStatus::NOT_FINITE };
        }
        if (at_lower > 0.0 || at_upper < 0.0) {
            return { guess, RootStatus::NO_BRACKET };
        }
        if (at_lower == 0.0 || at_upper == 0.0) {
            return { at_lower == 0.0 ? lo : hi, RootStatus::CONVERGED };
        }
    }

    double x = std::clamp(guess, lo, hi);
    for (uint32_t iteration = 0; iteration < options.max_iterations; ++iteration) {
        const auto [value, slope] = static_cast<RootSample>(function(x));

        if (!std::isfinite(value)) {
            return { x, RootStatus::NOT_FINITE };
        }
        if (value == 0.0) {
            return { x, RootStatus::CONVERGED };
        }

        (value < 0.0 ? lo : hi) = x;

        const bool usable = slope > 0.0 && std::isfinite(slope);
        const double step = usable ? value / slope : 0.0;
        if (usable && std::abs(step) <= options.tolerance * std::max(std::abs(x), options.floor)) {
            return { x - step, RootStatus::CONVERGED };
        }

        double next = usable ? x - step : lo;
        if (!usable || next <= lo || next >= hi) {
            next = 0.5 * (lo + hi);
        }

        const double reach = options.tolerance * std::max(std::abs(next), options.floor);
        if (std::abs(next - x) <= reach || hi - lo <= reach) {
            return { next, RootStatus::CONVERGED };
        }
        x = next;
    }

    return { x, RootStatus::MAX_ITERATIONS };
}

} // namespace MayaFlux::Kinesis
