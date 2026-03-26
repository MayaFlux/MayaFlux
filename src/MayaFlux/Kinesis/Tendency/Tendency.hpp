#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @struct Tendency
 * @brief Typed, composable, stateless callable from domain D to range R
 * @tparam D Domain type (input)
 * @tparam R Range type (output)
 *
 * Pure mathematical substrate expressing directional pull: a function
 * that maps a point in the domain to a value in the range. No internal
 * state, no history, no Vulkan awareness, no analog-era vocabulary.
 * Parameters that evolve over time are Datum<T> instances captured
 * in the lambda at construction.
 *
 * Composition is handled by free functions in this namespace.
 */
template <typename D, typename R>
struct Tendency {
    std::function<R(const D&)> fn;

    /**
     * @brief Evaluate the tendency at a point in the domain
     * @param d Domain value
     * @return Range value
     */
    R operator()(const D& d) const { return fn(d); }

    /**
     * @brief Named evaluation (identical to operator())
     * @param d Domain value
     * @return Range value
     */
    [[nodiscard]] R evaluate(const D& d) const { return fn(d); }
};

// =========================================================================
// Type aliases
// =========================================================================

using ScalarField = Tendency<float, float>;
using SpatialField = Tendency<glm::vec3, float>;
using VectorField = Tendency<glm::vec3, glm::vec3>;
using TemporalField = Tendency<float, float>;
using UVField = Tendency<glm::vec3, glm::vec2>;

// =========================================================================
// Composition
// =========================================================================

/**
 * @brief Combine two tendencies with a binary operation on their outputs
 * @tparam D Shared domain type
 * @tparam R Shared range type
 * @tparam BinaryOp Callable: (R, R) -> R
 * @param a First tendency
 * @param b Second tendency
 * @param op Binary combiner
 * @return Combined tendency
 */
template <typename D, typename R, typename BinaryOp>
Tendency<D, R> combine(const Tendency<D, R>& a, const Tendency<D, R>& b, BinaryOp op)
{
    return { .fn = [a, b, op](const D& d) -> R {
        return op(a(d), b(d));
    } };
}

/**
 * @brief Sequential composition: evaluate first, feed result into second
 * @tparam A Input domain
 * @tparam B Intermediate type
 * @tparam C Output range
 * @param first A -> B tendency
 * @param second B -> C tendency
 * @return Composed A -> C tendency
 */
template <typename A, typename B, typename C>
Tendency<A, C> chain(const Tendency<A, B>& first, const Tendency<B, C>& second)
{
    return { .fn = [first, second](const A& a) -> C {
        return second(first(a));
    } };
}

/**
 * @brief Uniform scaling of a scalar-output tendency
 * @tparam D Domain type
 * @param t Source tendency
 * @param factor Scale multiplier
 * @return Scaled tendency
 */
template <typename D>
Tendency<D, float> scale(const Tendency<D, float>& t, float factor)
{
    return { .fn = [t, factor](const D& d) -> float {
        return t(d) * factor;
    } };
}

/**
 * @brief Uniform scaling of a vector field
 * @param t Source tendency
 * @param factor Scale multiplier
 * @return Scaled tendency
 */
inline VectorField scale(const VectorField& t, float factor)
{
    return { .fn = [t, factor](const glm::vec3& d) -> glm::vec3 {
        return t(d) * factor;
    } };
}

/**
 * @brief Clamp scalar output to [lo, hi]
 * @tparam D Domain type
 * @param t Source tendency
 * @param lo Lower bound
 * @param hi Upper bound
 * @return Clamped tendency
 */
template <typename D>
Tendency<D, float> clamp(const Tendency<D, float>& t, float lo, float hi)
{
    return { .fn = [t, lo, hi](const D& d) -> float {
        return std::clamp(t(d), lo, hi);
    } };
}

/**
 * @brief Zero output below threshold, pass through above
 * @tparam D Domain type
 * @param t Source tendency
 * @param thresh Threshold value
 * @return Thresholded tendency
 */
template <typename D>
Tendency<D, float> threshold(const Tendency<D, float>& t, float thresh)
{
    return { .fn = [t, thresh](const D& d) -> float {
        float v = t(d);
        return v < thresh ? 0.0F : v;
    } };
}

/**
 * @brief Negate a scalar-output tendency
 * @tparam D Domain type
 * @param t Source tendency
 * @return Negated tendency
 */
template <typename D>
Tendency<D, float> invert(const Tendency<D, float>& t)
{
    return { .fn = [t](const D& d) -> float {
        return -t(d);
    } };
}

/**
 * @brief Negate all components of a vector field
 * @param t Source tendency
 * @return Negated tendency
 */
inline VectorField invert(const VectorField& t)
{
    return { .fn = [t](const glm::vec3& d) -> glm::vec3 {
        return -t(d);
    } };
}

/**
 * @brief Interpolate between two tendencies controlled by a weight tendency
 * @tparam D Shared domain type
 * @param a First tendency (weight 0.0)
 * @param b Second tendency (weight 1.0)
 * @param weight D -> float tendency producing interpolation factor
 * @return Blended tendency
 */
template <typename D>
Tendency<D, float> lerp(const Tendency<D, float>& a, const Tendency<D, float>& b, const Tendency<D, float>& weight)
{
    return { .fn = [a, b, weight](const D& d) -> float {
        float w = weight(d);
        return a(d) * (1.0F - w) + b(d) * w;
    } };
}

/**
 * @brief Select between two tendencies based on a predicate tendency
 * @tparam D Shared domain type
 * @tparam R Shared range type
 * @param predicate D -> float tendency (positive selects then_t, non-positive selects else_t)
 * @param then_t Tendency selected when predicate > 0
 * @param else_t Tendency selected when predicate <= 0
 * @return Conditional tendency
 */
template <typename D, typename R>
Tendency<D, R> select(const Tendency<D, float>& predicate, const Tendency<D, R>& then_t, const Tendency<D, R>& else_t)
{
    return { .fn = [predicate, then_t, else_t](const D& d) -> R {
        return predicate(d) > 0.0F ? then_t(d) : else_t(d);
    } };
}

} // namespace MayaFlux::Kinesis
