#include "TextRun.hpp"

#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Text {

namespace {
    constexpr uint32_t k_tab_width = 4;
}

TextRun::TextRun(PressParams params)
    : m_params(std::move(params))
{
}

void TextRun::set(std::string_view text)
{
    m_text = text;
    lay_out_text();
}

void TextRun::set_params(PressParams params)
{
    m_params = std::move(params);
    lay_out_text();
}

GlyphAtlas* TextRun::resolve_atlas() const
{
    return m_params.atlas ? m_params.atlas : TypeFaceFoundry::instance().get_default_glyph_atlas();
}

bool TextRun::stale() const
{
    const GlyphAtlas* atlas = resolve_atlas();
    return atlas != m_atlas || (atlas && atlas->atlas_size() != m_atlas_size);
}

void TextRun::lay_out_text()
{
    GlyphAtlas* atlas = resolve_atlas();
    if (!atlas) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "TextRun: no atlas available, call set_default_font first");
        m_layout = {};
        m_atlas = nullptr;
        m_atlas_size = 0;
        return;
    }

    m_layout = Text::lay_out(
        m_text, *atlas, 0.F, static_cast<float>(atlas->ascender()),
        m_params.render_bounds.x, k_tab_width);
    m_atlas = atlas;
    m_atlas_size = atlas->atlas_size();
}

float TextRun::content_height() const
{
    const GlyphAtlas* atlas = m_atlas;
    if (!atlas) {
        return 0.F;
    }
    return m_layout.final_pen_y - static_cast<float>(atlas->ascender())
        + static_cast<float>(atlas->line_height());
}

std::vector<Kakshya::MeshVertex> TextRun::vertices(const GlyphMap& map, uint32_t slot)
{
    if (stale()) {
        lay_out_text();
    }

    std::vector<Kakshya::MeshVertex> out;

    if (m_params.background.a > 0.F) {
        const glm::vec2 a = map({ 0.F, 0.F });
        const glm::vec2 b = map(glm::vec2(m_params.render_bounds));
        const auto quad = Kinesis::textured_mesh_rect(
            { .min = glm::min(a, b), .max = glm::max(a, b) },
            0.F, glm::vec3(m_params.background));
        out.insert(out.end(), quad.begin(), quad.end());
    }

    const auto glyphs = glyph_vertices(m_layout.quads, m_params.color, m_params.spans, map, slot);
    out.insert(out.end(), glyphs.begin(), glyphs.end());
    return out;
}

Kinesis::AABB2D TextRun::caret(size_t byte_offset, float width_px)
{
    if (stale()) {
        lay_out_text();
    }
    if (!m_atlas) {
        return {};
    }

    const auto ascender = static_cast<float>(m_atlas->ascender());
    GlyphAtlas& atlas = *resolve_atlas();
    const PenPosition pen = x_at(
        m_text, atlas, byte_offset, 0.F, ascender, m_params.render_bounds.x, k_tab_width);

    const float top = pen.y - ascender;
    return {
        .min = { pen.x, top },
        .max = { pen.x + width_px, top + static_cast<float>(atlas.line_height()) },
    };
}

size_t TextRun::index_at(glm::vec2 px)
{
    if (stale()) {
        lay_out_text();
    }
    if (!m_atlas) {
        return 0;
    }

    GlyphAtlas& atlas = *resolve_atlas();
    return Text::index_at(
        m_text, m_layout, atlas, px.x, px.y,
        0.F, static_cast<float>(atlas.ascender()), m_params.render_bounds.x, k_tab_width);
}

} // namespace MayaFlux::Portal::Text
