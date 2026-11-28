#pragma once

#include "Harmonics.hpp"

namespace MayaFlux::Kinesis {

inline constexpr uint32_t k_surround_width = 128;
inline constexpr uint32_t k_surround_height = 64;
inline constexpr uint32_t k_surround_levels = 6;
inline constexpr uint32_t k_albedo_table_size = 64;

/**
 * @struct RadianceGrid
 * @brief A view of linear radiance on an equirectangular grid.
 *
 * +Y is up and the first row is the top. The column at the middle looks along +X
 * and columns advance toward +Z.
 */
struct RadianceGrid {
    std::span<const float> pixels; ///< Rows of interleaved channels.
    uint32_t width {};
    uint32_t height {};
    uint32_t channels { 3 }; ///< Three or more; the first three are read.
};

/**
 * @struct SurroundRadiance
 * @brief What surfaces need of their surroundings: irradiance, blurred
 *        reflections and the specular albedo table.
 */
struct SurroundRadiance {
    HarmonicCoefficients harmonics {};
    /** Levels from k_surround_width by k_surround_height, halving, of rising roughness. */
    std::array<std::vector<glm::vec3>, k_surround_levels> levels;
    /** k_albedo_table_size squared entries, as specular_albedo_table() gives. */
    std::vector<glm::vec2> albedo;
};

/** @brief The direction through a grid coordinate in [0, 1]. */
[[nodiscard]] inline glm::vec3 grid_direction(const glm::vec2& uv) noexcept
{
    const float phi = (uv.x - 0.5F) * 2.0F * std::numbers::pi_v<float>;
    const float theta = uv.y * std::numbers::pi_v<float>;
    const float ring = std::sin(theta);
    return { ring * std::cos(phi), std::cos(theta), ring * std::sin(phi) };
}

/** @brief The grid coordinate of a unit direction. */
[[nodiscard]] inline glm::vec2 grid_coordinate(const glm::vec3& d) noexcept
{
    return {
        std::atan2(d.z, d.x) * 0.5F * std::numbers::inv_pi_v<float> + 0.5F,
        std::acos(std::clamp(d.y, -1.0F, 1.0F)) * std::numbers::inv_pi_v<float>
    };
}

/** @brief Whether a grid has pixels for its size and at least three channels. */
[[nodiscard]] inline bool usable_grid(const RadianceGrid& grid) noexcept
{
    return grid.width > 0 && grid.height > 0 && grid.channels >= 3
        && grid.pixels.size() >= static_cast<size_t>(grid.width) * grid.height * grid.channels;
}

/** @brief Bilinear radiance of a usable grid in a direction. */
[[nodiscard]] MAYAFLUX_API glm::vec3 radiance_at(const RadianceGrid& grid, const glm::vec3& direction) noexcept;

/** @brief Spherical harmonic radiance coefficients of a grid. Zero for an unusable one. */
[[nodiscard]] MAYAFLUX_API HarmonicCoefficients project_harmonics(const RadianceGrid& grid);

/**
 * @brief The grid averaged over a GGX lobe about each direction (split sum).
 *
 * @param grid      Source radiance.
 * @param roughness Perceptual roughness; zero resamples the grid.
 * @param width     Output columns.
 * @param height    Output rows.
 * @param samples   Directions per output texel.
 * @return Output rows of RGB, empty for an unusable grid or size.
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec3> prefilter_radiance(
    const RadianceGrid& grid, float roughness, uint32_t width, uint32_t height, uint32_t samples = 256);

/**
 * @brief Directional albedo of the GGX specular lobe, as a scale and bias on
 *        normal incidence reflectance: `f0 * x + y`.
 *
 * Columns are the cosine of the view and rows the roughness, both at texel
 * centres in (0, 1). `x + y` is the albedo at unit reflectance.
 *
 * @param size    Entries along each side.
 * @param samples Directions per entry.
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec2> specular_albedo_table(uint32_t size, uint32_t samples = 256);

/** @brief Harmonics, levels and table of a grid. Empty levels for an unusable one. */
[[nodiscard]] MAYAFLUX_API SurroundRadiance prefilter_surround(const RadianceGrid& grid, uint32_t samples = 256);

} // namespace MayaFlux::Kinesis
