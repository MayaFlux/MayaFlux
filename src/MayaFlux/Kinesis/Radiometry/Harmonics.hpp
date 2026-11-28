#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/** @brief Nine RGB coefficients of real spherical harmonics up to order two. */
using HarmonicCoefficients = std::array<glm::vec3, 9>;

/**
 * @brief The nine real basis functions in a direction, bands in order, m ascending.
 * @param d Unit direction.
 */
[[nodiscard]] inline std::array<float, 9> harmonic_basis(const glm::vec3& d) noexcept
{
    return {
        0.282095F,
        0.488603F * d.y,
        0.488603F * d.z,
        0.488603F * d.x,
        1.092548F * d.x * d.y,
        1.092548F * d.y * d.z,
        0.315392F * (3.0F * d.z * d.z - 1.0F),
        1.092548F * d.x * d.z,
        0.546274F * (d.x * d.x - d.y * d.y)
    };
}

/**
 * @brief Irradiance on a surface facing @p normal, from radiance coefficients
 *        (Ramamoorthi and Hanrahan).
 * @return Held at zero where the truncated series goes below it.
 */
[[nodiscard]] inline glm::vec3 harmonic_irradiance(
    const HarmonicCoefficients& radiance, const glm::vec3& normal) noexcept
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr std::array<float, 9> band {
        pi,
        2.0F * pi / 3.0F, 2.0F * pi / 3.0F, 2.0F * pi / 3.0F,
        pi / 4.0F, pi / 4.0F, pi / 4.0F, pi / 4.0F, pi / 4.0F
    };
    const auto basis = harmonic_basis(normal);
    glm::vec3 sum(0.0F);
    for (size_t i = 0; i < basis.size(); ++i) {
        sum += radiance[i] * (band[i] * basis[i]);
    }
    return glm::max(sum, glm::vec3(0.0F));
}

} // namespace MayaFlux::Kinesis
