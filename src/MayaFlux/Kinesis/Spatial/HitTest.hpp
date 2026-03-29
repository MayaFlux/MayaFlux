#pragma once

#include "SpatialIndex.hpp"

#include "MayaFlux/Kinesis/ViewTransform.hpp"

namespace MayaFlux::Kinesis {

// =========================================================================
// Ray
// =========================================================================

/**
 * @struct Ray
 * @brief Origin and normalized direction in world space.
 */
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

// =========================================================================
// Hit result
// =========================================================================

/**
 * @struct HitResult
 * @brief Entity id and perpendicular distance from the ray axis.
 */
struct HitResult {
    uint32_t id;
    float distance;
    float t;
};

// =========================================================================
// Ray construction
// =========================================================================

/**
 * @brief Construct a world-space ray from window pixel coordinates.
 * @param window_x  X in window space [0, width]
 * @param window_y  Y in window space [0, height] (top-left origin)
 * @param width     Window width in pixels.
 * @param height    Window height in pixels.
 * @param vt        Active ViewTransform (view + projection matrices).
 * @return Ray with origin at the near plane and normalized direction into the scene.
 *
 * Unprojects two points at z=0 (near) and z=1 (far) in NDC through the
 * inverse of projection * view. The resulting world-space positions define
 * the ray origin and direction.
 */
[[nodiscard]] inline Ray screen_to_ray(
    double window_x, double window_y,
    uint32_t width, uint32_t height,
    const ViewTransform& vt)
{
    float ndc_x = (static_cast<float>(window_x) / static_cast<float>(width)) * 2.0F - 1.0F;
    float ndc_y = 1.0F - (static_cast<float>(window_y) / static_cast<float>(height)) * 2.0F;

    glm::mat4 inv_vp = glm::inverse(vt.projection * vt.view);

    glm::vec4 near_h = inv_vp * glm::vec4(ndc_x, ndc_y, 0.0F, 1.0F);
    glm::vec4 far_h = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0F, 1.0F);

    glm::vec3 near_w = glm::vec3(near_h) / near_h.w;
    glm::vec3 far_w = glm::vec3(far_h) / far_h.w;

    return { .origin = near_w, .direction = glm::normalize(far_w - near_w) };
}

// =========================================================================
// Point-to-ray distance
// =========================================================================

/**
 * @brief Squared perpendicular distance from a point to an infinite ray.
 * @param ray  World-space ray.
 * @param point World-space position.
 * @param out_t If non-null, receives the projection parameter along the ray.
 *              Negative values indicate the point is behind the origin.
 * @return Squared perpendicular distance.
 */
[[nodiscard]] inline float point_ray_distance_sq(
    const Ray& ray,
    const glm::vec3& point,
    float* out_t = nullptr)
{
    glm::vec3 op = point - ray.origin;
    float t = glm::dot(op, ray.direction);
    glm::vec3 closest = ray.origin + ray.direction * t;
    glm::vec3 diff = point - closest;
    if (out_t) {
        *out_t = t;
    }
    return glm::dot(diff, diff);
}

// =========================================================================
// Hit testing
// =========================================================================

/**
 * @brief Find the closest entity to a ray within a tolerance radius.
 * @param index    Published SpatialIndex3D to query.
 * @param ray      World-space ray (from screen_to_ray or constructed manually).
 * @param tolerance Maximum perpendicular distance from the ray axis for a hit.
 * @return Closest entity within tolerance, or std::nullopt if nothing was hit.
 *
 * Iterates all entities in the published snapshot. Entities behind the ray
 * origin (t < 0) are rejected. Among forward candidates within tolerance,
 * returns the one with the smallest perpendicular distance. Ties broken by
 * smaller t (closer to camera).
 */
[[nodiscard]] inline std::optional<HitResult> ray_cast(
    const SpatialIndex3D& index,
    const Ray& ray,
    float tolerance)
{
    float tol_sq = tolerance * tolerance;
    std::optional<HitResult> best;

    for (const auto& [id, pos] : index.all()) {
        float t = 0.0F;
        float d_sq = point_ray_distance_sq(ray, pos, &t);

        if (t < 0.0F || d_sq > tol_sq) {
            continue;
        }

        float d = std::sqrt(d_sq);

        if (!best || d < best->distance || (d == best->distance && t < best->t)) {
            best = HitResult { .id = id, .distance = d, .t = t };
        }
    }

    return best;
}

/**
 * @brief Find all entities within tolerance of a ray, sorted by distance.
 * @param index     Published SpatialIndex3D to query.
 * @param ray       World-space ray.
 * @param tolerance Maximum perpendicular distance from the ray axis.
 * @return All hits sorted by ascending perpendicular distance. Empty if none.
 */
[[nodiscard]] inline std::vector<HitResult> ray_cast_all(
    const SpatialIndex3D& index,
    const Ray& ray,
    float tolerance)
{
    float tol_sq = tolerance * tolerance;
    std::vector<HitResult> results;

    for (const auto& [id, pos] : index.all()) {
        float t = 0.0F;
        float d_sq = point_ray_distance_sq(ray, pos, &t);

        if (t < 0.0F || d_sq > tol_sq) {
            continue;
        }

        results.push_back(HitResult {
            .id = id,
            .distance = std::sqrt(d_sq),
            .t = t });
    }

    std::ranges::sort(results, [](const HitResult& a, const HitResult& b) {
        return a.distance < b.distance;
    });

    return results;
}

} // namespace MayaFlux::Kinesis
