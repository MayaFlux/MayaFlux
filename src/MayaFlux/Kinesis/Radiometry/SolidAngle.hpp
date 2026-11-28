#pragma once

#include <glm/glm.hpp>

/**
 * Irradiance from uniformly radiating shapes, and the point of a shape nearest
 * a direction. Positions are relative to the receiving point. Irradiances are
 * per unit radiance.
 */
namespace MayaFlux::Kinesis {

/** @brief Most corners polygon_irradiance() reads. */
inline constexpr size_t k_polygon_corner_limit = 24;

// =============================================================================
// Solid angle and irradiance
// =============================================================================

/**
 * @brief Solid angle a sphere subtends; the full sphere from inside it.
 * @param distance From the point to the centre.
 * @param radius   Of the sphere.
 */
[[nodiscard]] inline float sphere_solid_angle(float distance, float radius) noexcept
{
    const float s = (radius * radius) / std::max(distance * distance, 1.0e-12F);
    if (s >= 1.0F) {
        return 4.0F * std::numbers::pi_v<float>;
    }
    return 2.0F * std::numbers::pi_v<float> * s / (1.0F + std::sqrt(1.0F - s));
}

/**
 * @brief Irradiance from a sphere, including the part cut off by the horizon.
 * @param cos_theta Cosine between the surface normal and the direction to the centre.
 * @param sin_sq    Squared radius over squared distance to the centre.
 * @return Pi for a point on or inside the sphere.
 */
[[nodiscard]] inline float sphere_irradiance(float cos_theta, float sin_sq) noexcept
{
    constexpr float pi = std::numbers::pi_v<float>;
    if (!(sin_sq > 0.0F)) {
        return 0.0F;
    }
    if (sin_sq >= 1.0F) {
        return pi;
    }
    const float c = std::clamp(cos_theta, -1.0F, 1.0F);
    if (c * c > sin_sq) {
        return pi * sin_sq * std::max(c, 0.0F);
    }
    const float sin_theta = std::sqrt(std::max(1.0F - c * c, 1.0e-12F));
    const float x = std::sqrt(1.0F / sin_sq - 1.0F);
    const float y = std::clamp(-x * c / sin_theta, -1.0F, 1.0F);
    const float across = sin_theta * std::sqrt(std::max(1.0F - y * y, 0.0F));
    return std::max((c * std::acos(y) - x * across) * sin_sq + std::atan(across / x), 0.0F);
}

/**
 * @brief Irradiance from a planar convex polygon, by Lambert's edge sum.
 *
 * The part behind the horizon of @p normal is clipped away. Winding does not matter.
 *
 * @param corners Up to k_polygon_corner_limit corners in order; further ones are ignored.
 * @param normal  Unit surface normal.
 */
[[nodiscard]] inline float polygon_irradiance(std::span<const glm::vec3> corners, const glm::vec3& normal) noexcept
{
    const size_t count = std::min(corners.size(), k_polygon_corner_limit);
    if (count < 3) {
        return 0.0F;
    }

    std::array<glm::vec3, k_polygon_corner_limit + 1> kept {};
    size_t held = 0;
    for (size_t i = 0; i < count; ++i) {
        const glm::vec3& a = corners[i];
        const glm::vec3& b = corners[(i + 1) % count];
        const float above_a = glm::dot(a, normal);
        const float above_b = glm::dot(b, normal);
        if (above_a > 0.0F && held < kept.size()) {
            kept[held++] = a;
        }
        if ((above_a > 0.0F) != (above_b > 0.0F) && held < kept.size()) {
            kept[held++] = a + (b - a) * (above_a / (above_a - above_b));
        }
    }
    if (held < 3) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < held; ++i) {
        const glm::vec3& a = kept[i];
        const glm::vec3& b = kept[(i + 1) % held];
        const glm::vec3 axis = glm::cross(a, b);
        const float length = glm::length(axis);
        if (!(length > 1.0e-12F)) {
            continue;
        }
        sum += std::atan2(length, glm::dot(a, b)) * glm::dot(axis / length, normal);
    }
    return 0.5F * std::abs(sum);
}

// =============================================================================
// Outlines
// =============================================================================

/**
 * @brief Corners of a rectangle.
 * @param right Unit direction of its width.
 * @param up    Unit direction of its height.
 */
[[nodiscard]] inline std::array<glm::vec3, 4> rectangle_corners(
    const glm::vec3& center, const glm::vec3& right, const glm::vec3& up, const glm::vec2& size) noexcept
{
    const glm::vec3 w = right * (0.5F * size.x);
    const glm::vec3 h = up * (0.5F * size.y);
    return { center - w - h, center + w - h, center + w + h, center - w + h };
}

/** @brief A sixteen sided polygon with the area of a disk. */
[[nodiscard]] inline std::array<glm::vec3, 16> disk_corners(
    const glm::vec3& center, const glm::vec3& right, const glm::vec3& up, float radius) noexcept
{
    constexpr float step = std::numbers::pi_v<float> / 8.0F;
    const float reach = radius * std::sqrt(step / std::sin(step));
    std::array<glm::vec3, 16> corners {};
    for (size_t i = 0; i < corners.size(); ++i) {
        const float angle = step * static_cast<float>(i);
        corners[i] = center + (right * std::cos(angle) + up * std::sin(angle)) * reach;
    }
    return corners;
}

/**
 * @brief A tube as the rectangle of its length and diameter turned toward the point.
 * @param axis Unit direction of the tube. End caps are not counted.
 */
[[nodiscard]] inline std::array<glm::vec3, 4> tube_corners(
    const glm::vec3& center, const glm::vec3& axis, float radius, float length) noexcept
{
    glm::vec3 side = glm::cross(axis, center);
    const float size = glm::length(side);
    side = size > 1.0e-8F ? side / size : glm::vec3(0.0F);
    const glm::vec3 half = axis * (0.5F * length);
    const glm::vec3 across = side * radius;
    return { center - half - across, center + half - across, center + half + across, center - half + across };
}

// =============================================================================
// Nearest point to a direction
// =============================================================================

/**
 * @brief Point of a sphere nearest the ray from the origin along @p ray.
 * @param ray Unit direction.
 */
[[nodiscard]] inline glm::vec3 sphere_toward(const glm::vec3& center, float radius, const glm::vec3& ray) noexcept
{
    const glm::vec3 miss = ray * glm::dot(center, ray) - center;
    const float distance = glm::length(miss);
    return distance > radius ? center + miss * (radius / distance) : center + miss;
}

/** @brief Point of a segment nearest the ray from the origin along unit @p ray. */
[[nodiscard]] inline glm::vec3 segment_toward(const glm::vec3& from, const glm::vec3& to, const glm::vec3& ray) noexcept
{
    const glm::vec3 along = to - from;
    const float ray_along = glm::dot(ray, along);
    const float divisor = glm::dot(along, along) - ray_along * ray_along;
    const float t = divisor > 1.0e-12F
        ? std::clamp((glm::dot(ray, from) * ray_along - glm::dot(from, along)) / divisor, 0.0F, 1.0F)
        : 0.5F;
    return from + along * t;
}

/** @brief Where the ray along @p ray meets the plane through @p center, or @p center when it never does. */
[[nodiscard]] inline glm::vec3 plane_toward(
    const glm::vec3& center, const glm::vec3& plane_normal, const glm::vec3& ray) noexcept
{
    const float facing = glm::dot(ray, plane_normal);
    if (std::abs(facing) < 1.0e-6F) {
        return center;
    }
    const float t = glm::dot(center, plane_normal) / facing;
    return t > 0.0F ? ray * t : center;
}

/** @brief Point of a rectangle nearest where the ray meets its plane. */
[[nodiscard]] inline glm::vec3 rectangle_toward(
    const glm::vec3& center, const glm::vec3& right, const glm::vec3& up, const glm::vec2& size,
    const glm::vec3& ray) noexcept
{
    const glm::vec3 offset = plane_toward(center, glm::cross(right, up), ray) - center;
    return center
        + right * std::clamp(glm::dot(offset, right), -0.5F * size.x, 0.5F * size.x)
        + up * std::clamp(glm::dot(offset, up), -0.5F * size.y, 0.5F * size.y);
}

/** @brief Point of a disk nearest where the ray meets its plane. */
[[nodiscard]] inline glm::vec3 disk_toward(
    const glm::vec3& center, const glm::vec3& plane_normal, float radius, const glm::vec3& ray) noexcept
{
    const glm::vec3 offset = plane_toward(center, plane_normal, ray) - center;
    const float distance = glm::length(offset);
    return distance > radius ? center + offset * (radius / distance) : center + offset;
}

/**
 * @brief Width of a lobe widened to cover a source of a size (Karis).
 * @param alpha    Width of the surface's lobe.
 * @param radius   Of the source.
 * @param distance To it.
 */
[[nodiscard]] inline float widened_alpha(float alpha, float radius, float distance) noexcept
{
    return std::clamp(alpha + radius / (2.0F * std::max(distance, 1.0e-4F)), alpha, 1.0F);
}

} // namespace MayaFlux::Kinesis
