#include "Geometry2D.hpp"

namespace MayaFlux::Kinesis {

namespace {

    constexpr uint32_t k_min_segments = 3;
    constexpr uint32_t k_min_sides = 3;

    [[nodiscard]] Kakshya::Vertex vert2(glm::vec2 p, glm::vec3 c) noexcept
    {
        return Kakshya::Vertex {
            .position = { p.x, p.y, 0.F },
            .color = c,
        };
    }

    [[nodiscard]] Kakshya::LineVertex lvert2(glm::vec2 p, glm::vec3 c, float t) noexcept
    {
        return Kakshya::LineVertex {
            .position = { p.x, p.y, 0.F },
            .color = c,
            .thickness = t,
        };
    }

    [[nodiscard]] glm::vec2 ring_point(glm::vec2 center, float r, float angle) noexcept
    {
        return center + r * glm::vec2(std::cos(angle), std::sin(angle));
    }

} // namespace

// =============================================================================
// Filled shapes
// =============================================================================

std::vector<Kakshya::Vertex> filled_circle(
    glm::vec2 center, float radius, uint32_t segments, glm::vec3 color)
{
    segments = std::max<uint32_t>(segments, k_min_segments);
    const float step = glm::two_pi<float>() / static_cast<float>(segments);

    std::vector<Kakshya::Vertex> out;
    out.reserve(static_cast<size_t>(segments) * 3);

    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) * step;
        const float a1 = static_cast<float>(i + 1) * step;
        out.push_back(vert2(center, color));
        out.push_back(vert2(ring_point(center, radius, a0), color));
        out.push_back(vert2(ring_point(center, radius, a1), color));
    }

    return out;
}

std::vector<Kakshya::Vertex> filled_ring(
    glm::vec2 center, float inner_r, float outer_r,
    uint32_t segments, glm::vec3 color)
{
    segments = std::max<uint32_t>(segments, k_min_segments);
    const float step = glm::two_pi<float>() / static_cast<float>(segments);

    std::vector<Kakshya::Vertex> out;
    out.reserve(static_cast<size_t>(segments) * 6);

    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) * step;
        const float a1 = static_cast<float>(i + 1) * step;

        const glm::vec2 oi0 = ring_point(center, inner_r, a0);
        const glm::vec2 oo0 = ring_point(center, outer_r, a0);
        const glm::vec2 oi1 = ring_point(center, inner_r, a1);
        const glm::vec2 oo1 = ring_point(center, outer_r, a1);

        out.push_back(vert2(oi0, color));
        out.push_back(vert2(oo0, color));
        out.push_back(vert2(oo1, color));

        out.push_back(vert2(oi0, color));
        out.push_back(vert2(oo1, color));
        out.push_back(vert2(oi1, color));
    }

    return out;
}

std::vector<Kakshya::Vertex> filled_polygon(
    glm::vec2 center, float radius, uint32_t sides,
    float rotation_rad, glm::vec3 color)
{
    sides = std::max<uint32_t>(sides, k_min_sides);
    const float step = glm::two_pi<float>() / static_cast<float>(sides);

    std::vector<Kakshya::Vertex> out;
    out.reserve((size_t)sides * 3);

    for (uint32_t i = 0; i < sides; ++i) {
        const float a0 = rotation_rad + static_cast<float>(i) * step;
        const float a1 = rotation_rad + static_cast<float>(i + 1) * step;
        out.push_back(vert2(center, color));
        out.push_back(vert2(ring_point(center, radius, a0), color));
        out.push_back(vert2(ring_point(center, radius, a1), color));
    }

    return out;
}

std::array<Kakshya::Vertex, 4> filled_rect_gradient(
    AABB2D region,
    glm::vec3 color_bl, glm::vec3 color_br,
    glm::vec3 color_tl, glm::vec3 color_tr)
{
    return { {
        { .position = { region.min.x, region.min.y, 0.F }, .color = color_bl },
        { .position = { region.min.x, region.max.y, 0.F }, .color = color_tl },
        { .position = { region.max.x, region.min.y, 0.F }, .color = color_br },
        { .position = { region.max.x, region.max.y, 0.F }, .color = color_tr },
    } };
}

std::vector<Kakshya::Vertex> filled_rounded_rect(
    AABB2D region, float corner_radius,
    uint32_t corner_segments, glm::vec3 color)
{
    corner_segments = std::max<uint32_t>(corner_segments, 1U);
    const float max_r = std::min(region.width(), region.height()) * 0.5F;
    const float r = std::min(corner_radius, max_r);

    const glm::vec2 bl { region.min.x + r, region.min.y + r };
    const glm::vec2 br { region.max.x - r, region.min.y + r };
    const glm::vec2 tl { region.min.x + r, region.max.y - r };
    const glm::vec2 tr { region.max.x - r, region.max.y - r };

    std::vector<Kakshya::Vertex> out;
    // 3 body rects (6 tris each) + 4 corner fans (corner_segments tris each)
    out.reserve(18 + 4 * (size_t)corner_segments * 3);

    // center rect
    auto push_quad = [&](glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d) {
        out.push_back(vert2(a, color));
        out.push_back(vert2(b, color));
        out.push_back(vert2(c, color));
        out.push_back(vert2(b, color));
        out.push_back(vert2(d, color));
        out.push_back(vert2(c, color));
    };

    push_quad({ bl.x, region.min.y }, { tl.x, region.max.y }, { tr.x, region.min.y }, { tr.x, region.max.y });
    push_quad({ region.min.x, bl.y }, { region.min.x, tl.y }, { bl.x, bl.y }, { bl.x, tl.y });
    push_quad({ tr.x, br.y }, { tr.x, tr.y }, { region.max.x, br.y }, { region.max.x, tr.y });

    // corner fans: BL, BR, TL, TR
    struct Corner {
        glm::vec2 c;
        float a0;
    };
    const auto half_pi = glm::half_pi<float>();
    const Corner corners[4] = {
        { .c = bl, .a0 = glm::pi<float>() },
        { .c = br, .a0 = -half_pi },
        { .c = tl, .a0 = half_pi },
        { .c = tr, .a0 = 0.F },
    };

    const float step = half_pi / static_cast<float>(corner_segments);
    for (const auto& [c, a0] : corners) {
        for (uint32_t i = 0; i < corner_segments; ++i) {
            const float a1 = a0 + static_cast<float>(i) * step;
            const float a2 = a0 + static_cast<float>(i + 1) * step;
            out.push_back(vert2(c, color));
            out.push_back(vert2(ring_point(c, r, a1), color));
            out.push_back(vert2(ring_point(c, r, a2), color));
        }
    }

    return out;
}

std::vector<Kakshya::Vertex> filled_arc(
    glm::vec2 center, float radius,
    float angle_start, float angle_end,
    uint32_t segments, glm::vec3 color)
{
    segments = std::max<uint32_t>(segments, 1U);
    const float step = (angle_end - angle_start) / static_cast<float>(segments);

    std::vector<Kakshya::Vertex> out;
    out.reserve(static_cast<size_t>(segments) * 3);

    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = angle_start + static_cast<float>(i) * step;
        const float a1 = angle_start + static_cast<float>(i + 1) * step;
        out.push_back(vert2(center, color));
        out.push_back(vert2(ring_point(center, radius, a0), color));
        out.push_back(vert2(ring_point(center, radius, a1), color));
    }

    return out;
}

std::vector<glm::vec2> arc_path(
    glm::vec2 center,
    float radius_x, float radius_y,
    float angle_start, float angle_end,
    uint32_t segments)
{
    segments = std::max<uint32_t>(segments, 1U);
    const float step = (angle_end - angle_start) / static_cast<float>(segments);

    std::vector<glm::vec2> out;
    out.reserve(static_cast<size_t>(segments) + 1);
    for (uint32_t i = 0; i <= segments; ++i) {
        const float a = angle_start + static_cast<float>(i) * step;
        out.push_back(center + glm::vec2(radius_x * std::cos(a), radius_y * std::sin(a)));
    }
    return out;
}

// =============================================================================
// Outlines
// =============================================================================

std::vector<Kakshya::LineVertex> circle_outline(
    glm::vec2 center, float radius, uint32_t segments,
    glm::vec3 color, float thickness)
{
    segments = std::max<uint32_t>(segments, k_min_segments);
    const float step = glm::two_pi<float>() / static_cast<float>(segments);

    std::vector<Kakshya::LineVertex> out;
    out.reserve(static_cast<size_t>(segments) * 2);

    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) * step;
        const float a1 = static_cast<float>((i + 1) % segments) * step;
        out.push_back(lvert2(ring_point(center, radius, a0), color, thickness));
        out.push_back(lvert2(ring_point(center, radius, a1), color, thickness));
    }

    return out;
}

std::vector<Kakshya::LineVertex> arc_outline(
    glm::vec2 center, float radius,
    float angle_start, float angle_end,
    uint32_t segments, glm::vec3 color, float thickness)
{
    segments = std::max<uint32_t>(segments, 1U);
    const float step = (angle_end - angle_start) / static_cast<float>(segments);

    std::vector<Kakshya::LineVertex> out;
    out.reserve(static_cast<size_t>(segments) * 2);

    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = angle_start + static_cast<float>(i) * step;
        const float a1 = angle_start + static_cast<float>(i + 1) * step;
        out.push_back(lvert2(ring_point(center, radius, a0), color, thickness));
        out.push_back(lvert2(ring_point(center, radius, a1), color, thickness));
    }

    return out;
}

std::array<Kakshya::LineVertex, 8> rect_outline(
    AABB2D region, glm::vec3 color, float thickness)
{
    const glm::vec2 bl { region.min.x, region.min.y };
    const glm::vec2 br { region.max.x, region.min.y };
    const glm::vec2 tl { region.min.x, region.max.y };
    const glm::vec2 tr { region.max.x, region.max.y };

    return { {
        lvert2(bl, color, thickness),
        lvert2(br, color, thickness),
        lvert2(br, color, thickness),
        lvert2(tr, color, thickness),
        lvert2(tr, color, thickness),
        lvert2(tl, color, thickness),
        lvert2(tl, color, thickness),
        lvert2(bl, color, thickness),
    } };
}

std::vector<Kakshya::LineVertex> polyline(
    std::span<const glm::vec2> pts, glm::vec3 color, float thickness)
{
    if (pts.size() < 2)
        return {};

    std::vector<Kakshya::LineVertex> out;
    out.reserve((pts.size() - 1) * 2);

    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        out.push_back(lvert2(pts[i], color, thickness));
        out.push_back(lvert2(pts[i + 1], color, thickness));
    }

    return out;
}

std::vector<Kakshya::LineVertex> polyline_colored(
    std::span<const glm::vec2> pts,
    std::span<const glm::vec3> colors,
    float thickness)
{
    if (pts.size() < 2 || colors.size() != pts.size())
        return {};

    std::vector<Kakshya::LineVertex> out;
    out.reserve((pts.size() - 1) * 2);

    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        out.push_back(lvert2(pts[i], colors[i], thickness));
        out.push_back(lvert2(pts[i + 1], colors[i + 1], thickness));
    }

    return out;
}

std::vector<Kakshya::LineVertex> polygon_outline(
    glm::vec2 center, float radius, uint32_t sides,
    float rotation_rad, glm::vec3 color, float thickness)
{
    sides = std::max<uint32_t>(sides, k_min_sides);
    const float step = glm::two_pi<float>() / static_cast<float>(sides);

    std::vector<Kakshya::LineVertex> out;
    out.reserve((size_t)sides * 2);

    for (uint32_t i = 0; i < sides; ++i) {
        const float a0 = rotation_rad + static_cast<float>(i) * step;
        const float a1 = rotation_rad + static_cast<float>((i + 1) % sides) * step;
        out.push_back(lvert2(ring_point(center, radius, a0), color, thickness));
        out.push_back(lvert2(ring_point(center, radius, a1), color, thickness));
    }

    return out;
}

} // namespace MayaFlux::Kinesis
