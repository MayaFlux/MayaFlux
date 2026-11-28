#pragma once

#include "SolidAngle.hpp"

/**
 * Linearly transformed cosines (Heitz, Dupuy, Hill and Neubelt): a cosine lobe
 * bent by a matrix to follow the specular lobe, so a polygon source integrates
 * in closed form.
 */
namespace MayaFlux::Kinesis {

inline constexpr uint32_t k_lobe_table_size = 32;

/**
 * @brief The inverse matrix an entry of fit_transformed_cosines() packs.
 *
 * It takes directions from the tangent frame, whose x lies toward the view and z
 * is the normal, to the frame of the plain cosine.
 */
[[nodiscard]] inline glm::mat3 lobe_matrix(const glm::vec4& entry) noexcept
{
    return { glm::vec3(entry.x, 0.0F, entry.y), glm::vec3(0.0F, 1.0F, 0.0F), glm::vec3(entry.z, 0.0F, entry.w) };
}

/** @brief Density per solid angle of the transformed lobe in a unit tangent frame direction. */
[[nodiscard]] inline float transformed_cosine_density(const glm::mat3& inverse, const glm::vec3& direction) noexcept
{
    const glm::vec3 original = inverse * direction;
    const float length_sq = glm::dot(original, original);
    if (!(original.z > 0.0F) || !(length_sq > 0.0F)) {
        return 0.0F;
    }
    return std::numbers::inv_pi_v<float> * original.z * std::abs(glm::determinant(inverse))
        / (length_sq * length_sq);
}

/**
 * @brief Share of the transformed lobe a polygon source covers.
 *
 * Multiply by the source's radiance and the specular albedo.
 *
 * @param inverse From lobe_matrix().
 * @param normal  Unit surface normal.
 * @param view    Unit direction toward the viewer.
 * @param corners Source corners relative to the point, at most k_polygon_corner_limit.
 */
[[nodiscard]] inline float transformed_coverage(
    const glm::mat3& inverse, const glm::vec3& normal, const glm::vec3& view,
    std::span<const glm::vec3> corners) noexcept
{
    glm::vec3 along = view - normal * glm::dot(view, normal);
    const float length = glm::length(along);
    if (length > 1.0e-6F) {
        along /= length;
    } else {
        const glm::vec3 other = std::abs(normal.x) < 0.9F ? glm::vec3(1.0F, 0.0F, 0.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
        along = glm::normalize(glm::cross(normal, other));
    }
    const glm::mat3 frame = glm::transpose(glm::mat3(along, glm::cross(normal, along), normal));
    const glm::mat3 bend = inverse * frame;

    std::array<glm::vec3, k_polygon_corner_limit> bent {};
    const size_t count = std::min(corners.size(), bent.size());
    for (size_t i = 0; i < count; ++i) {
        bent[i] = bend * corners[i];
    }
    return polygon_irradiance(std::span<const glm::vec3>(bent.data(), count), glm::vec3(0.0F, 0.0F, 1.0F))
        * std::numbers::inv_pi_v<float>;
}

/**
 * @brief Fit the lobe to GGX over view angle and roughness.
 *
 * Columns run over `sqrt(1 - cos_view)` and rows over roughness, both from 0 to
 * 1 inclusive. The work is large: call it once.
 *
 * @param size    Entries along each side, at least two.
 * @param samples Directions along each side of the error estimate.
 * @return Rows of entries for lobe_matrix(), empty for a size below two.
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec4> fit_transformed_cosines(
    uint32_t size = k_lobe_table_size, uint32_t samples = 16);

} // namespace MayaFlux::Kinesis
