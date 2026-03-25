#pragma once

#include <ranges>

#include "Tendency.hpp"

namespace MayaFlux::Kinesis {

// =========================================================================
// Constant
// =========================================================================

/**
 * @brief Tendency returning a fixed value regardless of input
 * @tparam D Domain type
 * @tparam R Range type
 * @param value Constant output value
 * @return Tendency that always returns value
 */
template <typename D, typename R>
Tendency<D, R> constant(R value)
{
    return { .fn = [value](const D&) -> R { return value; } };
}

// =========================================================================
// Distance
// =========================================================================

/**
 * @enum DistanceMetric
 * @brief Distance computation strategy for spatial queries
 */
enum class DistanceMetric : uint8_t {
    EUCLIDEAN,
    EUCLIDEAN_SQUARED,
    MANHATTAN,
    CHEBYSHEV
};

/**
 * @brief Normalized distance from an anchor point using the specified metric
 * @param anchor Reference point
 * @param radius Normalizing distance (output is 1.0 at this distance)
 * @param metric Distance computation strategy
 * @return SpatialField: glm::vec3 -> float
 *
 * Returns raw normalized distance. Compose with transfer_curve, smoothstep,
 * sigmoid, or any ScalarField via chain() to shape the result.
 */
inline SpatialField distance(
    const glm::vec3& anchor,
    float radius,
    DistanceMetric metric = DistanceMetric::EUCLIDEAN)
{
    switch (metric) {
    case DistanceMetric::EUCLIDEAN:
        return { .fn = [anchor, radius](const glm::vec3& p) -> float {
            return glm::length(p - anchor) / radius;
        } };
    case DistanceMetric::EUCLIDEAN_SQUARED: {
        float r_sq = radius * radius;
        return { .fn = [anchor, r_sq](const glm::vec3& p) -> float {
            glm::vec3 d = p - anchor;
            return glm::dot(d, d) / r_sq;
        } };
    }
    case DistanceMetric::MANHATTAN:
        return { .fn = [anchor, radius](const glm::vec3& p) -> float {
            glm::vec3 d = glm::abs(p - anchor);
            return (d.x + d.y + d.z) / radius;
        } };
    case DistanceMetric::CHEBYSHEV:
        return { .fn = [anchor, radius](const glm::vec3& p) -> float {
            glm::vec3 d = glm::abs(p - anchor);
            return std::max({ d.x, d.y, d.z }) / radius;
        } };
    }
    return { .fn = [anchor, radius](const glm::vec3& p) -> float {
        return glm::length(p - anchor) / radius;
    } };
}

// =========================================================================
// Scalar transfer functions
// =========================================================================

/**
 * @brief Transfer curve from polynomial coefficients (highest power first)
 * @param coefficients Coefficient vector, e.g. {1, 0, -1} evaluates as x^2 - 1
 * @return ScalarField: float -> float
 *
 * Distinct from the Polynomial node which is a stateful, buffer-aware
 * processor in the signal graph. This is a pure stateless evaluation.
 */
inline ScalarField transfer_curve(const std::vector<float>& coefficients)
{
    return { .fn = [coefficients](const float& x) -> float {
        float result = 0.0F;
        float power = 1.0F;

        for (float coefficient : std::views::reverse(coefficients)) {
            result += coefficient * power;
            power *= x;
        }
        return result;
    } };
}

/**
 * @brief Smoothstep: cubic Hermite interpolation between two edges
 * @param edge0 Lower edge (output 0.0 at and below)
 * @param edge1 Upper edge (output 1.0 at and above)
 * @return ScalarField: float -> float
 */
inline ScalarField smoothstep(float edge0, float edge1)
{
    return { .fn = [edge0, edge1](const float& x) -> float {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    } };
}

/**
 * @brief Sigmoid: 1 / (1 + e^(-k * (x - midpoint)))
 * @param k Steepness
 * @param midpoint Centre of the transition
 * @return ScalarField: float -> float
 */
inline ScalarField sigmoid(float k, float midpoint = 0.0F)
{
    return { .fn = [k, midpoint](const float& x) -> float {
        return 1.0F / (1.0F + std::exp(-k * (x - midpoint)));
    } };
}

} // namespace MayaFlux::Kinesis
