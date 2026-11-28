#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

// =============================================================================
// Physical falloff
// =============================================================================

/**
 * @brief Inverse square of a distance, held constant inside @p minimum.
 * @param distance From the source.
 * @param minimum  Distance below which the value stops rising. Above zero.
 */
[[nodiscard]] inline float inverse_square_falloff(float distance, float minimum = 0.01F) noexcept
{
    return 1.0F / std::max(distance * distance, minimum * minimum);
}

/**
 * @brief Window that takes a falloff smoothly to zero at @p range: `saturate(1 - (d/r)^4)^2`.
 * @return Zero when @p range is not above zero.
 */
[[nodiscard]] inline float range_window(float distance, float range) noexcept
{
    if (!(range > 0.0F)) {
        return 0.0F;
    }
    const float ratio = distance / range;
    const float squared = ratio * ratio;
    const float window = std::clamp(1.0F - squared * squared, 0.0F, 1.0F);
    return window * window;
}

/**
 * @brief Window about an axis: one inside the inner cone, zero outside the outer.
 * @param cos_angle Cosine of the angle from the axis.
 * @param cos_inner Cosine of the inner angle.
 * @param cos_outer Cosine of the outer angle.
 */
[[nodiscard]] inline float angular_window(float cos_angle, float cos_inner, float cos_outer) noexcept
{
    const float t = std::clamp((cos_angle - cos_outer) / std::max(cos_inner - cos_outer, 1.0e-4F), 0.0F, 1.0F);
    return t * t;
}

/**
 * @brief Reciprocal of `terms.x + terms.y * d + terms.z * d^2`.
 * @return One when the sum is not above zero.
 */
[[nodiscard]] inline float polynomial_falloff(float distance, const glm::vec3& terms) noexcept
{
    const float divisor = terms.x + distance * (terms.y + distance * terms.z);
    return divisor > 1.0e-8F ? 1.0F / divisor : 1.0F;
}

// =============================================================================
// Weight curves over a normalised distance, clamped to [0, 1]
// =============================================================================

/** @brief `1 - t`. */
[[nodiscard]] inline float falloff_linear(float t) noexcept
{
    return 1.0F - std::clamp(t, 0.0F, 1.0F);
}

/** @brief Half a cosine period, from one to zero. */
[[nodiscard]] inline float falloff_cosine(float t) noexcept
{
    return 0.5F * (1.0F + std::cos(std::clamp(t, 0.0F, 1.0F) * std::numbers::pi_v<float>));
}

/** @brief `exp(-4 t)`. */
[[nodiscard]] inline float falloff_exponential(float t) noexcept
{
    return std::exp(-4.0F * std::clamp(t, 0.0F, 1.0F));
}

/** @brief `1 / (1 + 16 t^2)`. */
[[nodiscard]] inline float falloff_inverse_square(float t) noexcept
{
    const float c = std::clamp(t, 0.0F, 1.0F);
    return 1.0F / (1.0F + 16.0F * c * c);
}

} // namespace MayaFlux::Kinesis
