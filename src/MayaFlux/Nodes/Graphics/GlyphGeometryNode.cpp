#include "GlyphGeometryNode.hpp"

#include "MayaFlux/Portal/Text/FontFace.hpp"
#include "MayaFlux/Portal/Text/GlyphAtlas.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

#include <utf8proc.h>

namespace MayaFlux::Nodes::GpuSync {

GlyphGeometryNode::GlyphGeometryNode(
    std::string text,
    float pen_x,
    float pen_y,
    float tolerance)
    : GeometryWriterNode(0)
    , m_text(std::move(text))
    , m_pen_x(pen_x)
    , m_pen_y(pen_y)
    , m_tolerance(tolerance)
{
    set_vertex_stride(sizeof(Kakshya::LineVertex));
    set_vertex_layout(Kakshya::VertexLayout::for_lines(sizeof(Kakshya::LineVertex)));
    rebuild_outlines();
}

GlyphGeometryNode::GlyphGeometryNode(
    Portal::Text::FontFace& face,
    uint32_t pixel_size,
    std::string text,
    float pen_x,
    float pen_y,
    float tolerance)
    : GeometryWriterNode(0)
    , m_face(&face)
    , m_pixel_size(pixel_size)
    , m_text(std::move(text))
    , m_pen_x(pen_x)
    , m_pen_y(pen_y)
    , m_tolerance(tolerance)
{
    set_vertex_stride(sizeof(Kakshya::LineVertex));
    set_vertex_layout(Kakshya::VertexLayout::for_lines(sizeof(Kakshya::LineVertex)));
    rebuild_outlines();
}

void GlyphGeometryNode::set_text(std::string text)
{
    m_text = std::move(text);
    rebuild_outlines();
}

void GlyphGeometryNode::set_pen(float pen_x, float pen_y)
{
    m_pen_x = pen_x;
    m_pen_y = pen_y;
    m_center.reset();
    m_dirty = true;
}

void GlyphGeometryNode::set_center(std::optional<glm::vec2> center)
{
    m_center = center;
    m_dirty = true;
}

void GlyphGeometryNode::set_color(glm::vec3 color)
{
    m_color = color;
    m_dirty = true;
}

void GlyphGeometryNode::set_thickness(float thickness)
{
    m_thickness = thickness;
    m_dirty = true;
}

void GlyphGeometryNode::rebuild_outlines()
{
    m_outlines.clear();
    m_pen_offsets.clear();

    if (m_text.empty()) {
        resize_vertex_buffer(0, false);
        m_vertex_data_dirty = true;
        return;
    }

    float cursor_x = m_pen_x;
    float cursor_y = m_pen_y;

    const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(m_text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(m_text.size());
    utf8proc_ssize_t offset = 0;

    while (offset < remaining) {
        utf8proc_int32_t cp = 0;
        const utf8proc_ssize_t n = utf8proc_iterate(bytes + offset, remaining - offset, &cp);

        if (n <= 0) {
            offset += 1;
            continue;
        }
        offset += n;

        if (cp < 0)
            continue;

        if (cp == '\n') {
            const uint32_t px = m_face
                ? m_pixel_size
                : (Portal::Text::TypeFaceFoundry::instance().get_default_glyph_atlas()
                          ? Portal::Text::TypeFaceFoundry::instance().get_default_glyph_atlas()->pixel_size()
                          : 0);
            cursor_x = m_pen_x;
            cursor_y += static_cast<float>(px);
            continue;
        }

        if (cp == '\r')
            continue;

        Portal::Text::GlyphOutline outline = m_face
            ? Portal::Text::decompose_glyph(*m_face, static_cast<uint32_t>(cp), m_pixel_size, m_tolerance)
            : Portal::Text::decompose_glyph(static_cast<uint32_t>(cp), m_tolerance);

        m_pen_offsets.emplace_back(cursor_x, cursor_y);
        m_outlines.push_back(std::move(outline));

        cursor_x += static_cast<float>(m_outlines.back().advance_x);
    }

    write_vertices();
}

void GlyphGeometryNode::set_pen_offsets(std::vector<glm::vec2> offsets)
{
    if (offsets.size() != m_outlines.size())
        return;
    m_pen_offsets = std::move(offsets);
    m_vertex_data_dirty = true;
}

void GlyphGeometryNode::write_vertices()
{
    size_t total_pairs = 0;

    for (const auto& ol : m_outlines) {
        uint32_t prev_end = 0;
        for (uint32_t end : ol.contour_ends) {
            const uint32_t count = end - prev_end;
            if (count >= 2)
                total_pairs += count - 1;
            prev_end = end;
        }
    }

    std::vector<Kakshya::LineVertex> verts;
    verts.reserve(total_pairs * 2);

    for (size_t gi = 0; gi < m_outlines.size(); ++gi) {
        const auto& ol = m_outlines[gi];
        const glm::vec2 off = m_pen_offsets[gi];

        uint32_t prev_end = 0;
        for (uint32_t end : ol.contour_ends) {
            const uint32_t count = end - prev_end;
            if (count < 2) {
                prev_end = end;
                continue;
            }

            for (uint32_t pi = prev_end; pi < end - 1; ++pi) {
                const glm::vec2 a = ol.points[pi] + off;
                const glm::vec2 b = ol.points[pi + 1] + off;
                verts.push_back({ .position = { a.x, a.y, 0.F }, .color = m_color, .thickness = m_thickness });
                verts.push_back({ .position = { b.x, b.y, 0.F }, .color = m_color, .thickness = m_thickness });
            }

            prev_end = end;
        }
    }

    set_vertices<Kakshya::LineVertex>(std::span { verts.data(), verts.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(verts.size());
    set_vertex_layout(*layout);

    m_vertex_data_dirty = true;
    m_dirty = false;
}

void GlyphGeometryNode::compute_frame()
{
    if (!m_vertex_data_dirty && !m_dirty)
        return;
    write_vertices();
}

} // namespace MayaFlux::Nodes::GpuSync
