#pragma once

#include "Fresnel.hpp"
#include "Microfacet.hpp"

#include "MayaFlux/Kinesis/Stochastic/Directions.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct SurfaceResponse
 * @brief What a surface point does with arriving radiance, in the metallic and roughness model.
 */
struct SurfaceResponse {
    glm::vec3 base_color { 0.8F }; ///< Linear. Diffuse albedo, or reflectance of a metal.
    float roughness { 0.5F }; ///< Perceptual roughness of the specular lobe.
    float metallic { 0.0F };
    float diffuse_roughness { 0.0F }; ///< Roughness of the diffuse lobe; zero is Lambertian.
    float dielectric_f0 { 0.04F }; ///< Normal incidence reflectance of the non metal part.
};

// =============================================================================
// Diffuse: energy preserving Oren-Nayar (Portsmouth, Kutz and Hill)
// =============================================================================

namespace detail {

    inline constexpr float k_fon_a = 0.5F - 2.0F / (3.0F * std::numbers::pi_v<float>);
    inline constexpr float k_fon_average = 2.0F / 3.0F - 28.0F / (15.0F * std::numbers::pi_v<float>);

    /** @brief Fitted directional albedo of the single scattering lobe at unit albedo. */
    [[nodiscard]] inline float fon_albedo(float cos_theta, float roughness) noexcept
    {
        const float m = 1.0F - cos_theta;
        const float g_over_pi = m * (0.0571085289F + m * (0.491881867F + m * (-0.332181442F + m * 0.0714429953F)));
        return (1.0F + roughness * g_over_pi) / (1.0F + k_fon_a * roughness);
    }

} // namespace detail

/**
 * @brief Value of the diffuse lobe, without the cosine of the arriving direction.
 * @param albedo    Single scattering albedo.
 * @param roughness In [0, 1].
 * @param normal    Unit surface normal.
 * @param view      Unit direction toward the viewer.
 * @param source    Unit direction toward where the radiance arrives from.
 */
[[nodiscard]] inline glm::vec3 oren_nayar_energy_preserving(
    const glm::vec3& albedo, float roughness,
    const glm::vec3& normal, const glm::vec3& view, const glm::vec3& source) noexcept
{
    const float cos_source = glm::dot(normal, source);
    const float cos_view = glm::dot(normal, view);
    if (!(cos_source > 0.0F) || !(cos_view > 0.0F)) {
        return glm::vec3(0.0F);
    }

    const float s = glm::dot(source, view) - cos_source * cos_view;
    const float s_over_t = s > 0.0F ? s / std::max({ cos_source, cos_view, 1.0e-6F }) : s;

    const float a = 1.0F / (1.0F + detail::k_fon_a * roughness);
    const glm::vec3 single = albedo * std::numbers::inv_pi_v<float> * a * (1.0F + roughness * s_over_t);

    const float average = a * (1.0F + detail::k_fon_average * roughness);
    const glm::vec3 multi_albedo = albedo * albedo * average / (glm::vec3(1.0F) - albedo * (1.0F - average));
    constexpr float floor = 1.0e-7F;
    const glm::vec3 multiple = multi_albedo * std::numbers::inv_pi_v<float>
        * std::max(floor, 1.0F - detail::fon_albedo(cos_view, roughness))
        * std::max(floor, 1.0F - detail::fon_albedo(cos_source, roughness))
        / std::max(floor, 1.0F - average);

    return single + multiple;
}

// =============================================================================
// Combined response
// =============================================================================

/** @brief Reflectance at normal incidence of both parts of the surface together. */
[[nodiscard]] inline glm::vec3 surface_f0(const SurfaceResponse& surface) noexcept
{
    return glm::mix(glm::vec3(surface.dielectric_f0), surface.base_color, surface.metallic);
}

/** @brief The diffuse lobe of surface_reflectance(). */
[[nodiscard]] inline glm::vec3 surface_diffuse(
    const SurfaceResponse& surface,
    const glm::vec3& normal, const glm::vec3& view, const glm::vec3& source) noexcept
{
    const float cos_view = glm::dot(normal, view);
    const float cos_source = glm::dot(normal, source);
    if (!(cos_view > 0.0F) || !(cos_source > 0.0F)) {
        return glm::vec3(0.0F);
    }

    const glm::vec3 boundary(surface.dielectric_f0);
    const glm::vec3 entering = (glm::vec3(1.0F) - schlick_fresnel(boundary, cos_view))
        * (glm::vec3(1.0F) - schlick_fresnel(boundary, cos_source))
        / (glm::vec3(1.0F) - schlick_average(boundary));
    return entering * (1.0F - surface.metallic)
        * oren_nayar_energy_preserving(surface.base_color, surface.diffuse_roughness, normal, view, source);
}

/**
 * @brief The specular lobe of surface_reflectance().
 * @param alpha           GGX width to evaluate, which a source with a size widens.
 * @param specular_albedo As surface_reflectance().
 */
[[nodiscard]] inline glm::vec3 surface_specular(
    const SurfaceResponse& surface,
    const glm::vec3& normal, const glm::vec3& view, const glm::vec3& source,
    float alpha, float specular_albedo = 1.0F) noexcept
{
    const float cos_view = glm::dot(normal, view);
    const float cos_source = glm::dot(normal, source);
    if (!(cos_view > 0.0F) || !(cos_source > 0.0F)) {
        return glm::vec3(0.0F);
    }

    const glm::vec3 half = glm::normalize(view + source);
    const glm::vec3 f0 = surface_f0(surface);
    return schlick_fresnel(f0, std::max(glm::dot(view, half), 0.0F))
        * ggx_density(glm::dot(normal, half), alpha)
        * ggx_visibility(cos_view, cos_source, alpha)
        * multiple_scattering_compensation(f0, specular_albedo);
}

/**
 * @brief Reflectance of a surface for one pair of directions, without the cosine.
 *
 * Multiply by `dot(normal, source)` and the arriving radiance to get what leaves
 * toward @p view.
 *
 * @param surface         The surface.
 * @param normal          Unit surface normal.
 * @param view            Unit direction toward the viewer.
 * @param source          Unit direction toward where the radiance arrives from.
 * @param specular_albedo Directional albedo of the specular lobe at unit Fresnel,
 *                        for multiple scattering compensation. One applies none.
 */
[[nodiscard]] inline glm::vec3 surface_reflectance(
    const SurfaceResponse& surface,
    const glm::vec3& normal, const glm::vec3& view, const glm::vec3& source,
    float specular_albedo = 1.0F) noexcept
{
    return surface_diffuse(surface, normal, view, source)
        + surface_specular(surface, normal, view, source, roughness_to_alpha(surface.roughness), specular_albedo);
}

// =============================================================================
// Sampling, in a frame whose +Z is the normal
// =============================================================================

/** @brief Share of samples surface_sample() draws from the specular lobe. */
[[nodiscard]] inline float specular_sample_share(const SurfaceResponse& surface) noexcept
{
    return std::clamp(0.5F + 0.5F * surface.metallic, 0.0F, 1.0F);
}

/**
 * @brief Density, per solid angle, of surface_sample() producing @p source.
 * @param view   Unit direction toward the viewer, +Z the normal.
 * @param source Unit direction in the same frame.
 */
[[nodiscard]] inline float surface_density(
    const SurfaceResponse& surface, const glm::vec3& view, const glm::vec3& source) noexcept
{
    if (!(view.z > 0.0F) || !(source.z > 0.0F)) {
        return 0.0F;
    }
    const glm::vec3 half = glm::normalize(view + source);
    const float share = specular_sample_share(surface);
    return share * ggx_reflected_density(view.z, half.z, roughness_to_alpha(surface.roughness))
        + (1.0F - share) * Stochastic::cosine_hemisphere_density(source.z);
}

/**
 * @brief Sample an arriving direction in proportion to roughly where the surface reflects.
 * @param view Unit direction toward the viewer, +Z the normal, z above zero.
 * @param u    Two values in [0, 1) for the direction.
 * @param lobe One value in [0, 1) choosing the lobe.
 * @return A unit direction. It is below the surface for some specular samples,
 *         where surface_reflectance() is zero.
 */
[[nodiscard]] inline glm::vec3 surface_sample(
    const SurfaceResponse& surface, const glm::vec3& view, const glm::vec2& u, float lobe) noexcept
{
    if (lobe < specular_sample_share(surface)) {
        const glm::vec3 half = ggx_visible_normal(view, roughness_to_alpha(surface.roughness), u);
        return glm::reflect(-view, half);
    }
    return Stochastic::square_to_cosine_hemisphere(u);
}

} // namespace MayaFlux::Kinesis
