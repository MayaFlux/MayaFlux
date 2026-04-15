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
 * @brief Lay out a UTF-8 string into a sequence of screen-space quads.
 *
 * Iterates codepoints via utf8proc_iterate, maps each to a glyph index
 * via FT_Get_Char_Index, queries the atlas (rasterizing on first encounter),
 * and accumulates GlyphQuad entries using standard left-to-right advance
 * arithmetic.
 *
 * Codepoints with no glyph in the face (utf8proc returns 0 or
 * FT_Get_Char_Index returns 0) are silently skipped; the pen still
 * advances by zero so no gap is introduced.
 *
 * This function intentionally does no line-wrapping, no bidirectional
 * reordering, and no shaping.  Those are HarfBuzz concerns; when
 * HarfBuzz is introduced it replaces the codepoint-to-glyph-index step
 * only -- the quad assembly loop and GlyphAtlas remain unchanged.
 *
 * @param text      UTF-8 encoded input string.
 * @param atlas     GlyphAtlas to query and populate.  May be modified
 *                  (dirty flag set) if new glyphs are rasterized.
 * @param pen_x     Starting horizontal pen position in pixels.
 * @param pen_y     Starting vertical pen position in pixels (baseline).
 * @return          One GlyphQuad per successfully laid-out glyph, in
 *                  left-to-right order.
 */
[[nodiscard]] std::vector<GlyphQuad> lay_out(
    std::string_view text,
    GlyphAtlas& atlas,
    float pen_x = 0.F,
    float pen_y = 0.F);

} // namespace MayaFlux::Portal::Text
