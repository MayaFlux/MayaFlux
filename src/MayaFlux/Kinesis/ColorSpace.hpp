#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @brief Convert hue, saturation and value to red, green and blue.
 *
 * All components are in [0, 1]. Hue wraps, so 1 is the same colour as 0;
 * saturation and value are clamped.
 *
 * @param hsv Hue in x, saturation in y, value in z.
 * @return    Red in x, green in y, blue in z.
 */
[[nodiscard]] inline glm::vec3 hsv_to_rgb(glm::vec3 hsv) noexcept
{
    const float hue = hsv.x - std::floor(hsv.x);
    const float saturation = std::clamp(hsv.y, 0.F, 1.F);
    const float value = std::clamp(hsv.z, 0.F, 1.F);
    const float sector = hue * 6.F;

    const auto channel = [&](float offset) {
        const float k = std::fmod(offset + sector, 6.F);
        return value - value * saturation * std::clamp(std::min(k, 4.F - k), 0.F, 1.F);
    };

    return { channel(5.F), channel(3.F), channel(1.F) };
}

/**
 * @brief Convert red, green and blue to hue, saturation and value.
 *
 * Inverse of hsv_to_rgb. Hue is 0 for greys, where it is undefined.
 *
 * @param rgb Red in x, green in y, blue in z, each in [0, 1].
 * @return    Hue in x, saturation in y, value in z, each in [0, 1].
 */
[[nodiscard]] inline glm::vec3 rgb_to_hsv(glm::vec3 rgb) noexcept
{
    const float cmax = std::max({ rgb.x, rgb.y, rgb.z });
    const float cmin = std::min({ rgb.x, rgb.y, rgb.z });
    const float delta = cmax - cmin;

    float hue = 0.F;
    if (delta > 0.F) {
        if (cmax == rgb.x) {
            hue = std::fmod((rgb.y - rgb.z) / delta, 6.F);
        } else if (cmax == rgb.y) {
            hue = (rgb.z - rgb.x) / delta + 2.F;
        } else {
            hue = (rgb.x - rgb.y) / delta + 4.F;
        }
        hue /= 6.F;
        if (hue < 0.F)
            hue += 1.F;
    }

    return { hue, cmax > 0.F ? delta / cmax : 0.F, cmax };
}

} // namespace MayaFlux::Kinesis
