#pragma once

#include "Tendency.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief cartesian projection along a chosen axis
 * @param axis    Normal of the projection plane (normalised by caller)
 * @param origin  World-space origin of the UV tile
 * @param scale   UV scale: larger values tile more tightly
 * @return UVField: glm::vec3 -> glm::vec2
 *
 * Projects each position onto the plane perpendicular to @p axis and
 * returns the two remaining components as (u, v).  The active axes are
 * chosen to avoid the dominant axis so the result is never degenerate.
 *
 * Axis selection:
 *   |axis.y| <= |axis.x| and |axis.y| <= |axis.z|  ->  tangent = world Y
 *   otherwise                                        ->  tangent = world X
 * The bitangent is cross(axis, tangent), giving an orthonormal frame.
 */
inline UVField cartesian(
    const glm::vec3& axis = glm::vec3(0.0F, 0.0F, 1.0F),
    const glm::vec3& origin = glm::vec3(0.0F),
    float scale = 1.0F)
{
    const glm::vec3 n = glm::normalize(axis);

    const glm::vec3 tangent = (std::abs(n.y) <= std::abs(n.x) && std::abs(n.y) <= std::abs(n.z))
        ? glm::normalize(glm::cross(n, glm::vec3(0.0F, 1.0F, 0.0F)))
        : glm::normalize(glm::cross(n, glm::vec3(1.0F, 0.0F, 0.0F)));

    const glm::vec3 bitangent = glm::cross(n, tangent);

    return { .fn = [tangent, bitangent, origin, scale](const glm::vec3& p) -> glm::vec2 {
        const glm::vec3 d = p - origin;
        return glm::vec2(glm::dot(d, tangent), glm::dot(d, bitangent)) * scale;
    } };
}

/**
 * @brief Cylindrical projection around a world axis
 * @param axis   Cylinder axis direction (normalised by caller)
 * @param origin Point on the cylinder axis closest to world origin
 * @param radius Cylinder radius used to normalise the radial component
 * @param height Full height of one UV tile along the axis
 * @return UVField: glm::vec3 -> glm::vec2
 *
 * u = azimuth angle in [0, 1], wraps at the seam behind the axis.
 * v = axial distance / height, unbounded — tiles vertically.
 *
 * Vertices straddling the seam (angle wraps π to -π) will show a UV
 * discontinuity.  Orient geometry or rotate @p axis to push the seam
 * to a non-visible region.
 */
inline UVField cylindrical(
    const glm::vec3& axis = glm::vec3(0.0F, 1.0F, 0.0F),
    const glm::vec3& origin = glm::vec3(0.0F),
    float radius = 1.0F,
    float height = 1.0F)
{
    const glm::vec3 n = glm::normalize(axis);

    const glm::vec3 tangent = (std::abs(n.y) <= std::abs(n.x) && std::abs(n.y) <= std::abs(n.z))
        ? glm::normalize(glm::cross(n, glm::vec3(0.0F, 1.0F, 0.0F)))
        : glm::normalize(glm::cross(n, glm::vec3(1.0F, 0.0F, 0.0F)));

    const glm::vec3 bitangent = glm::cross(n, tangent);

    const float inv_radius = (radius > 1e-6F) ? (1.0F / radius) : 1.0F;
    const float inv_height = (height > 1e-6F) ? (1.0F / height) : 1.0F;

    return { .fn = [n, tangent, bitangent, origin, inv_radius, inv_height](const glm::vec3& p) -> glm::vec2 {
        const glm::vec3 d = p - origin;
        const float axial = glm::dot(d, n);
        const float radial_x = glm::dot(d, tangent) * inv_radius;
        const float radial_y = glm::dot(d, bitangent) * inv_radius;
        const float u = (glm::atan(radial_y, radial_x) / static_cast<float>(std::numbers::pi) + 1.0F) * 0.5F;
        const float v = axial * inv_height;
        return { u, v };
    } };
}

/**
 * @brief Spherical projection from a centre point
 * @param centre World-space centre of the sphere
 * @return UVField: glm::vec3 -> glm::vec2
 *
 * u = longitude in [0, 1], zero at +X, increasing counter-clockwise.
 * v = latitude  in [0, 1], zero at south pole, one at north pole.
 *
 * Degenerate at the poles (sin(phi) -> 0) and at @p centre.
 * Vertices straddling the longitude seam will show a UV discontinuity.
 */
inline UVField spherical(const glm::vec3& centre = glm::vec3(0.0F))
{
    return { .fn = [centre](const glm::vec3& p) -> glm::vec2 {
        const glm::vec3 d = p - centre;
        const float len = glm::length(d);
        if (len < 1e-6F)
            return glm::vec2(0.0F);

        const glm::vec3 n = d / len;
        const float u = (glm::atan(n.z, n.x) / static_cast<float>(std::numbers::pi) + 1.0F) * 0.5F;
        const float v = n.y * 0.5F + 0.5F;
        return { u, v };
    } };
}

/**
 * @brief axial_blend projection: blend of three axis-aligned planar projections
 * @param origin World-space tile origin
 * @param scale  UV scale applied uniformly to all three planes
 * @param blend  Sharpness of the blend between planes (higher = sharper)
 * @return UVField: glm::vec3 -> glm::vec2
 *
 * Projects onto XZ, XY, and YZ planes and blends by weights derived
 * from the absolute value of the offset position, raised to @p blend
 * and normalised.  Uses position as a proxy for surface normal, which
 * works well for geometry radiating from the origin.  Geometry that is
 * flat or axially aligned may show visible blending artefacts.
 *
 * The blended UV is a weighted average:
 *   XZ plane contributes (x, z) weighted by the Y component.
 *   XY plane contributes (x, y) weighted by the Z component.
 *   YZ plane contributes (y, z) weighted by the X component.
 */
inline UVField axial_blend(
    const glm::vec3& origin = glm::vec3(0.0F),
    float scale = 1.0F,
    float blend = 4.0F)
{
    return { .fn = [origin, scale, blend](const glm::vec3& p) -> glm::vec2 {
        const glm::vec3 d = (p - origin) * scale;

        glm::vec3 w = glm::pow(glm::abs(d), glm::vec3(blend));
        const float wsum = w.x + w.y + w.z;
        if (wsum < 1e-6F)
            return glm::vec2(0.0F);
        w /= wsum;

        const glm::vec2 uv_xz(d.x, d.z);
        const glm::vec2 uv_xy(d.x, d.y);
        const glm::vec2 uv_yz(d.y, d.z);

        return uv_xz * w.y + uv_xy * w.z + uv_yz * w.x;
    } };
}

} // namespace MayaFlux::Kinesis
