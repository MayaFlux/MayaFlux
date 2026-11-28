#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/** @brief Scale linear colour by a number of stops. */
[[nodiscard]] inline glm::vec3 scale_by_stops(const glm::vec3& color, float stops) noexcept
{
    return color * std::exp2(stops);
}

/**
 * @brief Reinhard's curve with a white point, per channel.
 * @param color Linear, non negative.
 * @param white Value that maps to one. Above zero.
 */
[[nodiscard]] inline glm::vec3 reinhard_extended(const glm::vec3& color, float white = 4.0F) noexcept
{
    return color * (glm::vec3(1.0F) + color / (white * white)) / (glm::vec3(1.0F) + color);
}

/**
 * @brief Khronos PBR Neutral: base colours below the shoulder keep their value.
 * @param color Linear Rec. 709, non negative.
 * @return Linear Rec. 709 in [0, 1].
 */
[[nodiscard]] inline glm::vec3 pbr_neutral(glm::vec3 color) noexcept
{
    constexpr float start = 0.8F - 0.04F;
    constexpr float desaturation = 0.15F;

    const float low = std::min({ color.x, color.y, color.z });
    color -= low < 0.08F ? low - 6.25F * low * low : 0.04F;

    const float peak = std::max({ color.x, color.y, color.z });
    if (peak < start) {
        return color;
    }

    constexpr float d = 1.0F - start;
    const float new_peak = 1.0F - d * d / (peak + d - start);
    color *= new_peak / peak;

    const float g = 1.0F - 1.0F / (desaturation * (peak - new_peak) + 1.0F);
    return glm::mix(color, glm::vec3(new_peak), g);
}

} // namespace MayaFlux::Kinesis
