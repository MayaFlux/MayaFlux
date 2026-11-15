#include "Geometry2D.hpp"

namespace MayaFlux::Kinesis {

namespace {

    constexpr uint32_t k_min_segments = 3;
    constexpr uint32_t k_min_sides = 3;

    [[nodiscard]] Kakshya::Vertex vert2(glm::vec2 p, glm::vec4 c) noexcept
    {
        return Kakshya::Vertex {
            .position = { p.x, p.y, 0.F },
            .color = c,
        };
    }

    [[nodiscard]] Kakshya::LineVertex lvert2(glm::vec2 p, glm::vec4 c, float t) noexcept
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

    [[nodiscard]] float signed_area(std::span<const glm::vec2> pts, const std::vector<uint32_t>& idx) noexcept
    {
        float twice = 0.F;
        for (size_t i = 0; i < idx.size(); ++i) {
            const glm::vec2 p = pts[idx[i]];
            const glm::vec2 q = pts[idx[(i + 1) % idx.size()]];
            twice += p.x * q.y - q.x * p.y;
        }
        return 0.5F * twice;
    }

    [[nodiscard]] float turn(glm::vec2 a, glm::vec2 b, glm::vec2 c) noexcept
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    [[nodiscard]] bool inside_ccw(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) noexcept
    {
        return turn(a, b, p) >= 0.F && turn(b, c, p) >= 0.F && turn(c, a, p) >= 0.F;
    }

    [[nodiscard]] bool within_box(glm::vec2 a, glm::vec2 b, glm::vec2 p) noexcept
    {
        return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x)
            && std::min(a.y, b.y) <= p.y && p.y <= std::max(a.y, b.y);
    }

    [[nodiscard]] bool segments_meet(glm::vec2 p1, glm::vec2 p2, glm::vec2 q1, glm::vec2 q2) noexcept
    {
        const float d1 = turn(q1, q2, p1);
        const float d2 = turn(q1, q2, p2);
        const float d3 = turn(p1, p2, q1);
        const float d4 = turn(p1, p2, q2);

        const bool crosses = ((d1 > 0.F && d2 < 0.F) || (d1 < 0.F && d2 > 0.F))
            && ((d3 > 0.F && d4 < 0.F) || (d3 < 0.F && d4 > 0.F));
        if (crosses) {
            return true;
        }
        return (d1 == 0.F && within_box(q1, q2, p1))
            || (d2 == 0.F && within_box(q1, q2, p2))
            || (d3 == 0.F && within_box(p1, p2, q1))
            || (d4 == 0.F && within_box(p1, p2, q2));
    }

    [[nodiscard]] bool is_simple(std::span<const glm::vec2> pts) noexcept
    {
        const size_t n = pts.size();
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 2; j < n; ++j) {
                if (i == 0 && j == n - 1) {
                    continue;
                }
                if (segments_meet(pts[i], pts[(i + 1) % n], pts[j], pts[(j + 1) % n])) {
                    return false;
                }
            }
        }
        return true;
    }

} // namespace

// =============================================================================
// Filled shapes
// =============================================================================

std::vector<Kakshya::Vertex> filled_circle(
    glm::vec2 center, float radius, uint32_t segments, glm::vec4 color)
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
    uint32_t segments, glm::vec4 color)
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
    float rotation_rad, glm::vec4 color)
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
    glm::vec4 color_bl, glm::vec4 color_br,
    glm::vec4 color_tl, glm::vec4 color_tr)
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
    uint32_t corner_segments, glm::vec4 color)
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
    uint32_t segments, glm::vec4 color)
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
    glm::vec4 color, float thickness)
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
    uint32_t segments, glm::vec4 color, float thickness)
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
    AABB2D region, glm::vec4 color, float thickness)
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
    std::span<const glm::vec2> pts, glm::vec4 color, float thickness)
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
    std::span<const glm::vec4> colors,
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
    float rotation_rad, glm::vec4 color, float thickness)
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

namespace {

    struct Bridge {
        size_t a;
        size_t b;
    };

    [[nodiscard]] size_t next_in_ring(const std::vector<size_t>& bounds, size_t ring, size_t i) noexcept
    {
        return i + 1 < bounds[ring + 1] ? i + 1 : bounds[ring];
    }

    [[nodiscard]] bool inside_range(glm::vec2 p, std::span<const glm::vec2> pts, size_t begin, size_t end) noexcept
    {
        bool inside = false;
        for (size_t i = begin, j = end - 1; i < end; j = i++) {
            const glm::vec2 a = pts[i];
            const glm::vec2 b = pts[j];
            if ((a.y > p.y) != (b.y > p.y)
                && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
                inside = !inside;
            }
        }
        return inside;
    }

    [[nodiscard]] bool rings_valid(std::span<const glm::vec2> pts, const std::vector<size_t>& bounds) noexcept
    {
        const size_t rings = bounds.size() - 1;

        for (size_t r = 0; r < rings; ++r) {
            if (!is_simple(pts.subspan(bounds[r], bounds[r + 1] - bounds[r]))) {
                return false;
            }
        }

        for (size_t r = 0; r < rings; ++r) {
            for (size_t s = r + 1; s < rings; ++s) {
                for (size_t i = bounds[r]; i < bounds[r + 1]; ++i) {
                    const size_t ni = next_in_ring(bounds, r, i);
                    for (size_t j = bounds[s]; j < bounds[s + 1]; ++j) {
                        const size_t nj = next_in_ring(bounds, s, j);
                        if (segments_meet(pts[i], pts[ni], pts[j], pts[nj])) {
                            return false;
                        }
                    }
                }
            }
        }

        for (size_t h = 1; h < rings; ++h) {
            const glm::vec2 probe = pts[bounds[h]];
            if (!inside_range(probe, pts, bounds[0], bounds[1])) {
                return false;
            }
            for (size_t g = 1; g < rings; ++g) {
                if (g != h && inside_range(probe, pts, bounds[g], bounds[g + 1])) {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool bridge_visible(std::span<const glm::vec2> pts, const std::vector<size_t>& bounds,
        size_t m, size_t v, const std::vector<Bridge>& bridges) noexcept
    {
        const size_t rings = bounds.size() - 1;

        for (size_t r = 0; r < rings; ++r) {
            for (size_t i = bounds[r]; i < bounds[r + 1]; ++i) {
                const size_t ni = next_in_ring(bounds, r, i);
                if (i == m || i == v || ni == m || ni == v) {
                    continue;
                }
                if (segments_meet(pts[m], pts[v], pts[i], pts[ni])) {
                    return false;
                }
            }
        }

        for (const Bridge& bridge : bridges) {
            if (bridge.a == m || bridge.a == v || bridge.b == m || bridge.b == v) {
                continue;
            }
            if (segments_meet(pts[m], pts[v], pts[bridge.a], pts[bridge.b])) {
                return false;
            }
        }

        const glm::vec2 mid = 0.5F * (pts[m] + pts[v]);
        if (!inside_range(mid, pts, bounds[0], bounds[1])) {
            return false;
        }
        for (size_t h = 1; h < rings; ++h) {
            if (inside_range(mid, pts, bounds[h], bounds[h + 1])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::vector<std::array<uint32_t, 3>> clip_ears(
        std::span<const glm::vec2> pts, std::vector<uint32_t> ring)
    {
        std::vector<std::array<uint32_t, 3>> tris;
        if (ring.size() < 3) {
            return tris;
        }
        tris.reserve(ring.size() - 2);

        while (ring.size() > 3) {
            const size_t n = ring.size();
            bool clipped = false;

            for (size_t i = 0; i < n && !clipped; ++i) {
                const size_t prev = (i + n - 1) % n;
                const size_t next = (i + 1) % n;
                const glm::vec2 a = pts[ring[prev]];
                const glm::vec2 b = pts[ring[i]];
                const glm::vec2 c = pts[ring[next]];
                if (turn(a, b, c) <= 0.F) {
                    continue;
                }

                bool blocked = false;
                for (size_t j = 0; j < n && !blocked; ++j) {
                    if (j == prev || j == i || j == next) {
                        continue;
                    }
                    const glm::vec2 q = pts[ring[j]];
                    if (q == a || q == b || q == c) {
                        continue;
                    }
                    blocked = inside_ccw(q, a, b, c);
                }
                if (blocked) {
                    continue;
                }

                tris.push_back({ ring[prev], ring[i], ring[next] });
                ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
            }

            if (!clipped) {
                return {};
            }
        }

        if (turn(pts[ring[0]], pts[ring[1]], pts[ring[2]]) <= 0.F) {
            return {};
        }
        tris.push_back({ ring[0], ring[1], ring[2] });
        return tris;
    }

} // namespace

std::vector<std::array<uint32_t, 3>> triangulate_polygon(
    std::span<const glm::vec2> pts, std::span<const uint32_t> hole_starts)
{
    std::vector<size_t> bounds { 0 };
    for (const uint32_t start : hole_starts) {
        bounds.push_back(start);
    }
    bounds.push_back(pts.size());

    for (size_t r = 0; r + 1 < bounds.size(); ++r) {
        if (bounds[r + 1] < bounds[r] + 3) {
            return {};
        }
    }
    if (!rings_valid(pts, bounds)) {
        return {};
    }

    const size_t rings = bounds.size() - 1;
    std::vector<std::vector<uint32_t>> ring_idx(rings);
    for (size_t r = 0; r < rings; ++r) {
        for (size_t i = bounds[r]; i < bounds[r + 1]; ++i) {
            ring_idx[r].push_back(static_cast<uint32_t>(i));
        }
    }
    if (signed_area(pts, ring_idx[0]) < 0.F) {
        std::ranges::reverse(ring_idx[0]);
    }
    for (size_t r = 1; r < rings; ++r) {
        if (signed_area(pts, ring_idx[r]) > 0.F) {
            std::ranges::reverse(ring_idx[r]);
        }
    }

    std::vector<uint32_t> merged = ring_idx[0];
    std::vector<Bridge> bridges;
    for (size_t r = 1; r < rings; ++r) {
        const std::vector<uint32_t>& hole = ring_idx[r];

        size_t m = 0;
        for (size_t t = 1; t < hole.size(); ++t) {
            if (pts[hole[t]].x > pts[hole[m]].x) {
                m = t;
            }
        }
        const size_t anchor = hole[m];

        std::optional<size_t> best;
        float best_d2 = std::numeric_limits<float>::max();
        for (size_t p = 0; p < merged.size(); ++p) {
            const glm::vec2 d = pts[anchor] - pts[merged[p]];
            const float d2 = glm::dot(d, d);
            if (d2 < best_d2 && bridge_visible(pts, bounds, anchor, merged[p], bridges)) {
                best_d2 = d2;
                best = p;
            }
        }
        if (!best) {
            return {};
        }

        const size_t p = *best;
        bridges.push_back({ .a = anchor, .b = merged[p] });

        std::vector<uint32_t> out;
        out.reserve(merged.size() + hole.size() + 2);
        out.insert(out.end(), merged.begin(), merged.begin() + static_cast<std::ptrdiff_t>(p) + 1);
        for (size_t t = 0; t <= hole.size(); ++t) {
            out.push_back(hole[(m + t) % hole.size()]);
        }
        out.push_back(merged[p]);
        out.insert(out.end(), merged.begin() + static_cast<std::ptrdiff_t>(p) + 1, merged.end());
        merged = std::move(out);
    }

    return clip_ears(pts, std::move(merged));
}

std::vector<Kakshya::Vertex> fill_polygon(std::span<const glm::vec2> pts, glm::vec4 color)
{
    const auto tris = triangulate_polygon(pts);
    std::vector<Kakshya::Vertex> out;
    out.reserve(tris.size() * 3);
    for (const auto& tri : tris) {
        for (const uint32_t k : tri) {
            out.push_back(vert2(pts[k], color));
        }
    }
    return out;
}

} // namespace MayaFlux::Kinesis
