#pragma once

#include "GlyphAtlas.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @struct GlyphQuad
 * @brief Screen-space quad for one rasterized glyph.
 *
 * All coordinates are in pixels relative to the pen origin passed to
 * lay_out().  The caller is responsible for converting to NDC or whatever
 * coordinate system the render path expects.
 *
 * UV coordinates are in normalised [0, 1] atlas space, matching the values
 * stored in GlyphMetrics.
 */
struct GlyphQuad {
    float x0, y0; ///< Top-left pixel position.
    float x1, y1; ///< Bottom-right pixel position.
    float uv_x0, uv_y0; ///< Top-left UV in atlas space.
    float uv_x1, uv_y1; ///< Bottom-right UV in atlas space.
};

/**
 * @brief Result of lay_out(), carrying the quads and the final pen position.
 *
 * final_pen_x and final_pen_y are the pen coordinates immediately after the
 * last codepoint was processed. impress() uses these directly to update the
 * TextBuffer cursor, avoiding re-deriving the position from quad geometry
 * (which is incorrect after a newline resets pen_x mid-run).
 */
struct LayoutResult {
    std::vector<GlyphQuad> quads;
    float final_pen_x { 0.F };
    float final_pen_y { 0.F };
};

/**
 * @brief Lay out a UTF-8 string into a sequence of screen-space quads.
 *
 * Handles \n (advance pen_y by atlas.line_height(), reset pen_x to the
 * initial value) and \r (consumed silently). All other control codepoints
 * with no glyph in the face are skipped without advancing the pen.
 *
 * Bidirectional reordering and shaping are not performed. HarfBuzz slots in
 * before the glyph index step when needed; the quad assembly loop and
 * GlyphAtlas remain unchanged.
 *
 * @param text   UTF-8 encoded input string.
 * @param atlas  GlyphAtlas to query and populate. May be modified (dirty flag
 *               set) if new glyphs are rasterized.
 * @param pen_x  Starting horizontal pen position in pixels.
 * @param pen_y  Starting vertical pen position in pixels (baseline).
 * @return       LayoutResult containing quads and final pen position.
 */
[[nodiscard]] LayoutResult lay_out(
    std::string_view text,
    GlyphAtlas& atlas,
    float pen_x = 0.F,
    float pen_y = 0.F);

} // namespace MayaFlux::Portal::Text
