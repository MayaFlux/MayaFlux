#pragma once

#include "Reflectance.hpp"
#include "SolidAngle.hpp"
#include "Visibility.hpp"

namespace MayaFlux::Kinesis {

/** @brief The shape a RadiantSource radiates from, which decides what its intensity measures. */
enum class RadiantShape : uint8_t {
    Point, ///< Radiant intensity.
    Spot, ///< Radiant intensity within a cone about axis.
    Directional, ///< Irradiance, arriving along axis from everywhere.
    Sphere, ///< Radiance, as for the shapes below. extent.x is the radius.
    Disk, ///< extent.x is the radius; radiates toward axis.
    Rectangle, ///< extent is width along right and height; radiates toward axis.
    Tube ///< extent.x is the radius and extent.y the length, along right.
};

/**
 * @struct RadiantSource
 * @brief One source of radiance, with the values the lighting shaders take.
 */
struct RadiantSource {
    RadiantShape shape { RadiantShape::Point };
    glm::vec3 position { 0.0F };
    glm::vec3 color { 1.0F };
    float intensity { 1.0F };
    float range { 1.0F }; ///< Distance at which a placed source reaches zero.
    glm::vec3 falloff { 0.0F, 0.0F, 1.0F }; ///< Divisor terms of Point and Spot.
    glm::vec3 axis { 0.0F, 0.0F, -1.0F }; ///< Unit.
    glm::vec3 right { 1.0F, 0.0F, 0.0F }; ///< Unit.
    glm::vec2 extent { 1.0F };
    float cos_inner { 1.0F };
    float cos_outer { -1.0F };
};

/**
 * @brief Irradiance one source delivers to a surface point, per channel.
 * @param normal Unit surface normal.
 */
[[nodiscard]] MAYAFLUX_API glm::vec3 source_irradiance(
    const RadiantSource& source, const glm::vec3& position, const glm::vec3& normal) noexcept;

/**
 * @brief Radiance one source adds toward the viewer.
 * @param normal Unit surface normal.
 * @param view   Unit direction toward the viewer.
 */
[[nodiscard]] MAYAFLUX_API glm::vec3 source_radiance(
    const RadiantSource& source, const SurfaceResponse& surface,
    const glm::vec3& position, const glm::vec3& normal, const glm::vec3& view) noexcept;

/**
 * @brief Irradiance of every source at every receiver, in parallel.
 *
 * What a bake of light onto vertices needs: it does not depend on a viewer.
 *
 * @param sources   Sources, summed.
 * @param positions Receiver positions.
 * @param normals   Unit receiver normals.
 * @param occluder  When given, a source reaches a receiver only if the line to
 *                  its position, or against its axis if Directional, is open.
 * @return One value per receiver, for as many as have a normal.
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec3> accumulate_irradiance(
    std::span<const RadiantSource> sources,
    std::span<const glm::vec3> positions, std::span<const glm::vec3> normals,
    const std::shared_ptr<const TriangleOccluder>& occluder = {});

/**
 * @brief Radiance of every source at every receiver toward one eye, in parallel.
 * @param eye      Where the receivers are seen from.
 * @param occluder As accumulate_irradiance().
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec3> accumulate_radiance(
    std::span<const RadiantSource> sources, const SurfaceResponse& surface,
    std::span<const glm::vec3> positions, std::span<const glm::vec3> normals, const glm::vec3& eye,
    const std::shared_ptr<const TriangleOccluder>& occluder = {});

} // namespace MayaFlux::Kinesis
