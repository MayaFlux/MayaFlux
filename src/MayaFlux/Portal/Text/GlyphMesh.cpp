#include "GlyphMesh.hpp"

namespace MayaFlux::Portal::Text {

GlyphMap GlyphMap::fit(Kinesis::AABB2D region, glm::vec2 source_px) noexcept
{
    const glm::vec2 source = glm::max(source_px, glm::vec2(1.F));
    return {
        .origin = { region.min.x, region.max.y },
        .scale = { region.width() / source.x, -region.height() / source.y },
    };
}

std::vector<Kakshya::MeshVertex> glyph_vertices(
    std::span<const GlyphQuad> quads,
    glm::vec4 color,
    std::span<const StyleSpan> spans,
    const GlyphMap& map,
    uint32_t slot)
{
    using V = Kakshya::MeshVertex;

    std::vector<V> out;
    out.reserve(quads.size() * 6);

    const float weight = coverage_weight(slot);

    for (const auto& q : quads) {
        glm::vec3 rgb { color };
        for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
            const size_t lo = std::min(it->start, it->end);
            const size_t hi = std::max(it->start, it->end);
            if (q.byte_offset >= lo && q.byte_offset < hi) {
                rgb = glm::vec3(it->color);
                break;
            }
        }

        const glm::vec2 top_left = map({ q.x0, q.y0 });
        const glm::vec2 bottom_right = map({ q.x1, q.y1 });

        const V bl { .position = { top_left.x, bottom_right.y, 0.F }, .color = rgb, .weight = weight, .uv = { q.uv_x0, q.uv_y1 } };
        const V br { .position = { bottom_right.x, bottom_right.y, 0.F }, .color = rgb, .weight = weight, .uv = { q.uv_x1, q.uv_y1 } };
        const V tl { .position = { top_left.x, top_left.y, 0.F }, .color = rgb, .weight = weight, .uv = { q.uv_x0, q.uv_y0 } };
        const V tr { .position = { bottom_right.x, top_left.y, 0.F }, .color = rgb, .weight = weight, .uv = { q.uv_x1, q.uv_y0 } };

        out.insert(out.end(), { bl, br, tl, br, tr, tl });
    }

    return out;
}

} // namespace MayaFlux::Portal::Text
