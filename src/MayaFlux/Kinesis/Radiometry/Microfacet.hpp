#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @brief GGX width from a perceptual roughness: its square, held above 1e-3.
 */
[[nodiscard]] inline float roughness_to_alpha(float roughness) noexcept
{
    return std::max(roughness * roughness, 1.0e-3F);
}

/**
 * @brief GGX distribution of microfacet normals.
 * @param cos_half Cosine between the surface normal and the half vector.
 * @param alpha    Width, above zero.
 */
[[nodiscard]] inline float ggx_density(float cos_half, float alpha) noexcept
{
    if (!(cos_half > 0.0F)) {
        return 0.0F;
    }
    const float a2 = alpha * alpha;
    const float c2 = std::min(cos_half * cos_half, 1.0F);
    const float d = c2 * a2 + (1.0F - c2);
    return a2 / (std::numbers::pi_v<float> * d * d);
}

/**
 * @brief Smith masking of GGX seen from one direction.
 * @param cos_theta Cosine between the surface normal and the direction.
 */
[[nodiscard]] inline float ggx_masking(float cos_theta, float alpha) noexcept
{
    if (!(cos_theta > 0.0F)) {
        return 0.0F;
    }
    const float a2 = alpha * alpha;
    return 2.0F * cos_theta / (cos_theta + std::sqrt(a2 + (1.0F - a2) * cos_theta * cos_theta));
}

/**
 * @brief Height correlated Smith masking and shadowing divided by `4 cos_view cos_source`.
 */
[[nodiscard]] inline float ggx_visibility(float cos_view, float cos_source, float alpha) noexcept
{
    const float a2 = alpha * alpha;
    const float view = cos_source * std::sqrt(cos_view * cos_view * (1.0F - a2) + a2);
    const float source = cos_view * std::sqrt(cos_source * cos_source * (1.0F - a2) + a2);
    const float sum = view + source;
    return sum > 0.0F ? 0.5F / sum : 0.0F;
}

/**
 * @brief Factor on a single scattering lobe that returns the energy lost between microfacets.
 *
 * Turquin's compensation, `1 + f0 (1 / albedo - 1)`.
 *
 * @param f0     Reflectance at normal incidence.
 * @param albedo Directional albedo of the lobe at unit Fresnel, in (0, 1]. One leaves it unchanged.
 */
[[nodiscard]] inline glm::vec3 multiple_scattering_compensation(const glm::vec3& f0, float albedo) noexcept
{
    return glm::vec3(1.0F) + f0 * (1.0F / std::max(albedo, 1.0e-4F) - 1.0F);
}

/**
 * @brief Sample a microfacet normal from those visible to @p view (Dupuy and Benyoub).
 * @param view  Unit direction toward the viewer, +Z the surface normal, z above zero.
 * @param alpha Width.
 * @param u     Two values in [0, 1).
 * @return Unit microfacet normal in the same frame.
 */
[[nodiscard]] inline glm::vec3 ggx_visible_normal(const glm::vec3& view, float alpha, const glm::vec2& u) noexcept
{
    const glm::vec3 warped = glm::normalize(glm::vec3(view.x * alpha, view.y * alpha, view.z));

    const float phi = 2.0F * std::numbers::pi_v<float> * u.x;
    const float z = (1.0F - u.y) * (1.0F + warped.z) - warped.z;
    const float radius = std::sqrt(std::clamp(1.0F - z * z, 0.0F, 1.0F));
    const glm::vec3 half = glm::vec3(radius * std::cos(phi), radius * std::sin(phi), z) + warped;

    return glm::normalize(glm::vec3(half.x * alpha, half.y * alpha, std::max(half.z, 0.0F)));
}

/**
 * @brief Density, per solid angle, of the direction reflected about a ggx_visible_normal() sample.
 * @param cos_view Cosine between the surface normal and the view direction.
 * @param cos_half Cosine between the surface normal and the half vector.
 */
[[nodiscard]] inline float ggx_reflected_density(float cos_view, float cos_half, float alpha) noexcept
{
    if (!(cos_view > 0.0F)) {
        return 0.0F;
    }
    return ggx_masking(cos_view, alpha) * ggx_density(cos_half, alpha) / (4.0F * cos_view);
}

} // namespace MayaFlux::Kinesis
