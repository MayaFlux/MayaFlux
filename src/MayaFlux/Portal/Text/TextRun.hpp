#pragma once

#include "GlyphMesh.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @class TextRun
 * @brief One string laid out as glyph quads, ready to draw as vertices.
 *
 * Owns the text, its PressParams and the layout, and turns them into
 * triangle list vertices through a GlyphMap, so the same run can draw in NDC,
 * a world plane or any other space. It is the vertex counterpart of press():
 * no pixels are rasterized, the glyphs are drawn from the atlas image (see
 * AtlasImage) as coverage.
 *
 * Layout runs from the pen origin press() uses for a VKImage: x at zero,
 * baseline at the atlas ascender, wrapping at render_bounds.x. Of PressParams
 * it reads atlas, color, spans, background and render_bounds; budget_h has no
 * meaning here. A background with alpha above zero is drawn as one opaque flat
 * quad over render_bounds beneath the glyphs.
 *
 * The layout bakes in atlas UVs. When the atlas is replaced or grows, the run
 * is stale; vertices() lays out again first, so its output is always current.
 * The caller binds the atlas image at the slot it passes.
 */
class MAYAFLUX_API TextRun {
public:
    explicit TextRun(PressParams params = {});

    /// @brief Replace the text and lay it out.
    void set(std::string_view text);

    /// @brief Replace the parameters and lay the current text out again.
    void set_params(PressParams params);

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] const PressParams& params() const noexcept { return m_params; }

    /// @brief The layout of the current text, in pixels with y down.
    [[nodiscard]] const LayoutResult& layout() const noexcept { return m_layout; }

    /**
     * @brief True when the atlas the layout was built against has since been
     *        replaced or has grown, so its UVs no longer match.
     */
    [[nodiscard]] bool stale() const;

    /**
     * @brief Height in pixels the text occupies, from the top of the first line
     *        to the bottom of the last.
     */
    [[nodiscard]] float content_height() const;

    /**
     * @brief Vertices for the background, if any, then every glyph.
     *
     * @param map  Layout pixels to output space.
     * @param slot Atlas image slot in the shader's texture array.
     */
    [[nodiscard]] std::vector<Kakshya::MeshVertex> vertices(const GlyphMap& map, uint32_t slot);

    /**
     * @brief Pixel rectangle of a caret at a byte offset of the text.
     *
     * Spans one line height on the line the offset falls on, in layout pixels
     * with y down, so min.y is the top.
     *
     * @param byte_offset Codepoint boundary in the text.
     * @param width_px    Caret width in pixels.
     */
    [[nodiscard]] Kinesis::AABB2D caret(size_t byte_offset, float width_px);

    /**
     * @brief Byte offset of the codepoint boundary nearest a layout pixel.
     *
     * @param px Point in layout pixels with y down.
     */
    [[nodiscard]] size_t index_at(glm::vec2 px);

private:
    [[nodiscard]] GlyphAtlas* resolve_atlas() const;
    void lay_out_text();

    PressParams m_params;
    std::string m_text;
    LayoutResult m_layout;
    const GlyphAtlas* m_atlas { nullptr };
    uint32_t m_atlas_size { 0 };
};

} // namespace MayaFlux::Portal::Text
