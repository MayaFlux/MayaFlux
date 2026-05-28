#pragma once

#include "MayaFlux/Kinesis/Spatial/HitTest.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// AABB2D
// =============================================================================

/**
 * @struct AABB2D
 * @brief Axis-aligned bounding rectangle in a 2D coordinate space.
 *
 * Used as a fast-reject hint in Portal::Forma::Element. All coordinates
 * are in NDC by convention when used in Forma: x and y in [-1, +1],
 * center origin, +Y up, matching ViewTransform.hpp and Windowing.hpp.
 *
 * Named constructors convert from other coordinate spaces into NDC.
 */
struct AABB2D {
    glm::vec2 min;
    glm::vec2 max;

    [[nodiscard]] bool contains(glm::vec2 p) const noexcept
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y;
    }

    [[nodiscard]] bool overlaps(const AABB2D& other) const noexcept
    {
        return min.x <= other.max.x && max.x >= other.min.x
            && min.y <= other.max.y && max.y >= other.min.y;
    }

    [[nodiscard]] float width() const noexcept { return max.x - min.x; }
    [[nodiscard]] float height() const noexcept { return max.y - min.y; }

    [[nodiscard]] glm::vec2 center() const noexcept { return (min + max) * 0.5F; }

    [[nodiscard]] AABB2D translated(glm::vec2 offset) const noexcept
    {
        return { .min = min + offset, .max = max + offset };
    }

    [[nodiscard]] AABB2D expanded(float margin) const noexcept
    {
        return { .min = min - glm::vec2(margin), .max = max + glm::vec2(margin) };
    }

    [[nodiscard]] static AABB2D from_ndc(glm::vec2 center, glm::vec2 half) noexcept
    {
        return { .min = center - half, .max = center + half };
    }

    /**
     * @brief Construct from pixel-space top-left rect, converted to NDC.
     * @param x     Left edge in pixels.
     * @param y     Top edge in pixels (top-left origin, +Y down).
     * @param w     Width in pixels.
     * @param h     Height in pixels.
     * @param win_w Window width in pixels.
     * @param win_h Window height in pixels.
     */
    [[nodiscard]] static AABB2D from_pixel(
        float x, float y, float w, float h,
        uint32_t win_w, uint32_t win_h) noexcept
    {
        auto nx = [&](float px) {
            return (px / static_cast<float>(win_w)) * 2.0F - 1.0F;
        };
        auto ny = [&](float py) {
            return 1.0F - (py / static_cast<float>(win_h)) * 2.0F;
        };
        return { .min = glm::vec2(nx(x), ny(y + h)), .max = glm::vec2(nx(x + w), ny(y)) };
    }

    /**
     * @brief Construct from a TextureBuffer NDC position and scale.
     * @param ndc_position Center as returned by TextureBuffer::get_position().
     * @param ndc_scale    Extent as returned by TextureBuffer::get_scale().
     */
    [[nodiscard]] static AABB2D from_buffer_transform(
        glm::vec2 ndc_position, glm::vec2 ndc_scale) noexcept
    {
        glm::vec2 half = ndc_scale * 0.5F;
        return { .min = ndc_position - half, .max = ndc_position + half };
    }
};

// =============================================================================
// BoundingSphere
// =============================================================================

/**
 * @struct BoundingSphere
 * @brief Spherical fast-reject hint in world space.
 *
 * Used as the cheap pre-filter for 3D spatial predicates, analogous to
 * AABB2D in the 2D path. A sphere is a better pre-filter than an axis-aligned
 * box in 3D: rotation-invariant, one dot product to test a point, one
 * discriminant to test a ray.
 */
struct BoundingSphere {
    glm::vec3 center;
    float radius;

    [[nodiscard]] bool contains(const glm::vec3& p) const noexcept
    {
        glm::vec3 d = p - center;
        return glm::dot(d, d) <= radius * radius;
    }

    /**
     * @brief Returns true if the ray passes through or originates inside the sphere.
     * @param ray World-space ray with normalized direction.
     */
    [[nodiscard]] bool overlaps_ray(const Ray& ray) const noexcept
    {
        glm::vec3 oc = ray.origin - center;
        float b = glm::dot(oc, ray.direction);
        float c = glm::dot(oc, oc) - radius * radius;
        return b * b - c >= 0.0F;
    }
};

// =============================================================================
// AABB3D
// =============================================================================

/**
 * @struct AABB3D
 * @brief Axis-aligned bounding box in 3D world space.
 *
 * Counterpart to AABB2D for three-dimensional geometry. Used by
 * Kinesis::aabb() to describe the extent of any PositionCarrying span,
 * and as a fast-reject pre-filter in 3D spatial predicates.
 *
 * No coordinate convention is enforced; the box is whatever the caller's
 * position data implies (world space, object space, NDC-extended, etc.).
 */
struct AABB3D {
    glm::vec3 min;
    glm::vec3 max;

    [[nodiscard]] bool contains(const glm::vec3& p) const noexcept
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    [[nodiscard]] bool overlaps(const AABB3D& other) const noexcept
    {
        return min.x <= other.max.x && max.x >= other.min.x
            && min.y <= other.max.y && max.y >= other.min.y
            && min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] glm::vec3 center() const noexcept { return (min + max) * 0.5F; }
    [[nodiscard]] glm::vec3 extent() const noexcept { return max - min; }
    [[nodiscard]] glm::vec3 half_extent() const noexcept { return (max - min) * 0.5F; }

    [[nodiscard]] AABB3D translated(const glm::vec3& offset) const noexcept
    {
        return { .min = min + offset, .max = max + offset };
    }

    [[nodiscard]] AABB3D expanded(float margin) const noexcept
    {
        return { .min = min - glm::vec3(margin), .max = max + glm::vec3(margin) };
    }

    /**
     * @brief Construct from a center point and half-extents.
     * @param c    Box centre.
     * @param half Half-size along each axis.
     */
    [[nodiscard]] static AABB3D from_center(const glm::vec3& c, const glm::vec3& half) noexcept
    {
        return { .min = c - half, .max = c + half };
    }

    /**
     * @brief Construct the tightest AABB enclosing a BoundingSphere.
     * @param s Source sphere.
     */
    [[nodiscard]] static AABB3D from_sphere(const BoundingSphere& s) noexcept
    {
        return { .min = s.center - glm::vec3(s.radius),
            .max = s.center + glm::vec3(s.radius) };
    }
};

// =============================================================================
// Containment callables
// All functions return std::function<bool(glm::vec2)> suitable for
// direct assignment to Portal::Forma::Element::contains.
// =============================================================================

/**
 * @brief Containment test for a circle.
 * @param center NDC center.
 * @param radius Radius in NDC units.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
circular_bounds(glm::vec2 center, float radius) noexcept
{
    float r2 = radius * radius;
    return [center, r2](glm::vec2 p) {
        glm::vec2 d = p - center;
        return d.x * d.x + d.y * d.y <= r2;
    };
}

/**
 * @brief Containment test for a convex or concave polygon.
 *
 * Uses the winding number algorithm, which handles both convex and
 * non-convex polygons correctly. Vertices are in NDC, ordered either
 * clockwise or counter-clockwise.
 *
 * @param vertices Polygon vertices. Copied into the closure.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
polygon_bounds(std::span<const glm::vec2> vertices)
{
    std::vector<glm::vec2> verts(vertices.begin(), vertices.end());
    return [verts = std::move(verts)](glm::vec2 p) {
        int winding = 0;
        size_t n = verts.size();
        for (size_t i = 0; i < n; ++i) {
            glm::vec2 a = verts[i];
            glm::vec2 b = verts[(i + 1) % n];
            if (a.y <= p.y) {
                if (b.y > p.y) {
                    float cross = (b.x - a.x) * (p.y - a.y)
                        - (b.y - a.y) * (p.x - a.x);
                    if (cross > 0.0F)
                        ++winding;
                }
            } else {
                if (b.y <= p.y) {
                    float cross = (b.x - a.x) * (p.y - a.y)
                        - (b.y - a.y) * (p.x - a.x);
                    if (cross < 0.0F)
                        --winding;
                }
            }
        }
        return winding != 0;
    };
}

/**
 * @brief Containment test for a polyline with a uniform half-thickness.
 *
 * Covers open and closed curves equally. A point is inside if its
 * perpendicular distance to any segment is within @p half_thickness.
 * Suitable for stroke-based interactive regions and cable hit testing.
 *
 * @param points        Ordered polyline vertices in NDC.
 * @param half_thickness Distance threshold in NDC units.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
stroke_bounds(std::span<const glm::vec2> points, float half_thickness)
{
    std::vector<glm::vec2> pts(points.begin(), points.end());
    float t2 = half_thickness * half_thickness;
    return [pts = std::move(pts), t2](glm::vec2 p) {
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            glm::vec2 a = pts[i];
            glm::vec2 b = pts[i + 1];
            glm::vec2 ab = b - a;
            float len2 = glm::dot(ab, ab);
            float d2 = 0.0F;
            if (len2 < 1e-12F) {
                glm::vec2 ap = p - a;
                d2 = glm::dot(ap, ap);
            } else {
                float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.0F, 1.0F);
                glm::vec2 proj = a + t * ab;
                glm::vec2 diff = p - proj;
                d2 = glm::dot(diff, diff);
            }
            if (d2 <= t2)
                return true;
        }
        return false;
    };
}

// =============================================================================
// 3D containment callables
// All functions return std::function<bool(glm::vec3)> suitable for
// direct use as spatial predicates in Nexus and NavigationState constraints.
// Pair with BoundingSphere for a cheap pre-filter where the predicate is
// expensive.
// =============================================================================

/**
 * @brief Containment test for a sphere.
 * @param center World-space center.
 * @param radius Radius in world units.
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
circular_bounds(glm::vec3 center, float radius) noexcept
{
    float r2 = radius * radius;
    return [center, r2](const glm::vec3& p) {
        glm::vec3 d = p - center;
        return glm::dot(d, d) <= r2;
    };
}

/**
 * @brief Containment test for a convex volume defined by inward-facing half-planes.
 *
 * A point is inside if it satisfies all half-planes: dot(normal, p) >= offset
 * for every plane. Normals must point inward. Suitable for authored rooms,
 * corridors, and any convex spatial zone. Non-convex volumes are composed
 * via union_region3 / subtract_region3.
 *
 * @param planes Pairs of (inward normal, signed offset). Copied into closure.
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
convex_region(std::span<const std::pair<glm::vec3, float>> planes)
{
    std::vector<std::pair<glm::vec3, float>> ps(planes.begin(), planes.end());
    return [ps = std::move(ps)](const glm::vec3& p) {
        return std::ranges::all_of(
            ps,
            [&p](const auto& plane) {
                const auto& [n, d] = plane;
                return glm::dot(n, p) >= d;
            });
    };
}

/**
 * @brief Containment test for a vertically extruded 2D polygon.
 *
 * The footprint is tested with the winding number algorithm in the XZ plane.
 * The Y axis is the vertical: the point must satisfy y_min <= p.y <= y_max.
 * Covers authored rooms, columns, corridors where the cross-section is
 * constant along Y.
 *
 * @param footprint Polygon vertices in XZ. Copied into closure.
 * @param y_min     Lower Y bound (inclusive).
 * @param y_max     Upper Y bound (inclusive).
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
extruded_polygon_region(
    std::span<const glm::vec2> footprint,
    float y_min,
    float y_max)
{
    std::vector<glm::vec2> verts(footprint.begin(), footprint.end());
    return [verts = std::move(verts), y_min, y_max](const glm::vec3& p) {
        if (p.y < y_min || p.y > y_max)
            return false;
        glm::vec2 q { p.x, p.z };
        int winding = 0;
        const size_t n = verts.size();
        for (size_t i = 0; i < n; ++i) {
            glm::vec2 a = verts[i];
            glm::vec2 b = verts[(i + 1) % n];
            if (a.y <= q.y) {
                if (b.y > q.y) {
                    float cross = (b.x - a.x) * (q.y - a.y)
                        - (b.y - a.y) * (q.x - a.x);
                    if (cross > 0.0F)
                        ++winding;
                }
            } else {
                if (b.y <= q.y) {
                    float cross = (b.x - a.x) * (q.y - a.y)
                        - (b.y - a.y) * (q.x - a.x);
                    if (cross < 0.0F)
                        --winding;
                }
            }
        }
        return winding != 0;
    };
}

// =============================================================================
// Combinators
// =============================================================================

/**
 * @brief Union of two containment tests. Point is inside if either returns true.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
union_bounds(
    std::function<bool(glm::vec2)> a,
    std::function<bool(glm::vec2)> b)
{
    return [a = std::move(a), b = std::move(b)](glm::vec2 p) {
        return a(p) || b(p);
    };
}

/**
 * @brief Intersection of two containment tests. Point is inside only if both return true.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
intersect_bounds(
    std::function<bool(glm::vec2)> a,
    std::function<bool(glm::vec2)> b)
{
    return [a = std::move(a), b = std::move(b)](glm::vec2 p) {
        return a(p) && b(p);
    };
}

/**
 * @brief Difference of two containment tests. Point is inside if @p a is true and @p b is false.
 *        Useful for donuts, cutouts, and exclusion zones.
 */
[[nodiscard]] inline std::function<bool(glm::vec2)>
subtract_bounds(
    std::function<bool(glm::vec2)> a,
    std::function<bool(glm::vec2)> b)
{
    return [a = std::move(a), b = std::move(b)](glm::vec2 p) {
        return a(p) && !b(p);
    };
}

/**
 * @brief Convert an NDC AABB's extent to integer pixel dimensions.
 *
 * Equivalent to ndc_size_to_pixels(region.max - region.min, width, height).
 *
 * @param region NDC axis-aligned bounding box.
 * @param width  Window width in pixels.
 * @param height Window height in pixels.
 * @return Integer pixel dimensions of the region's extent.
 */
[[nodiscard]] inline glm::uvec2 ndc_size_to_pixels(
    const AABB2D& region,
    uint32_t width, uint32_t height)
{
    return ndc_size_to_pixels(region.max - region.min, width, height);
}

// =============================================================================
// 3D combinators
// =============================================================================

/**
 * @brief Union of two 3D containment tests.
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
union_bounds(
    std::function<bool(glm::vec3)> a,
    std::function<bool(glm::vec3)> b)
{
    return [a = std::move(a), b = std::move(b)](const glm::vec3& p) {
        return a(p) || b(p);
    };
}

/**
 * @brief Intersection of two 3D containment tests.
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
intersect_bounds(
    std::function<bool(glm::vec3)> a,
    std::function<bool(glm::vec3)> b)
{
    return [a = std::move(a), b = std::move(b)](const glm::vec3& p) {
        return a(p) && b(p);
    };
}

/**
 * @brief Difference of two 3D containment tests. Inside @p a but not @p b.
 */
[[nodiscard]] inline std::function<bool(glm::vec3)>
subtract_bounds(
    std::function<bool(glm::vec3)> a,
    std::function<bool(glm::vec3)> b)
{
    return [a = std::move(a), b = std::move(b)](const glm::vec3& p) {
        return a(p) && !b(p);
    };
}

} // namespace MayaFlux::Kinesis
