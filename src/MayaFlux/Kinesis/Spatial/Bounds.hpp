#pragma once

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

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

} // namespace MayaFlux::Kinesis
