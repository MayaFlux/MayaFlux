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
    uint32_t codepoint { 0 }; ///< Unicode codepoint that produced this quad.
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
    float pen_y = 0.F,
    uint32_t wrap_w = 0);

/**
 * @brief Encode a Unicode codepoint as a UTF-8 byte sequence.
 *
 * Counterpart to the decode step lay_out() performs via utf8proc_iterate.
 * Invalid codepoints (surrogates, > 0x10FFFF) return an empty string.
 *
 * @param codepoint  Unicode codepoint to encode.
 * @return           1-4 byte UTF-8 sequence, or empty on invalid input.
 */
[[nodiscard]] MAYAFLUX_API std::string encode_utf8(uint32_t codepoint);

/**
 * @brief Byte offset of the codepoint boundary after @p byte_offset.
 *
 * Steps forward exactly one codepoint using the same utf8proc decode lay_out()
 * uses, so malformed sequences are skipped identically rather than by a
 * separate continuation-byte mask. Returns text.size() if already at or past
 * the end.
 *
 * @param text         UTF-8 encoded string.
 * @param byte_offset  Current byte offset, must be a codepoint boundary.
 * @return             Byte offset of the next codepoint boundary.
 */
[[nodiscard]] MAYAFLUX_API size_t next_codepoint_offset(std::string_view text, size_t byte_offset);

/**
 * @brief Byte offset of the codepoint boundary before @p byte_offset.
 *
 * Walks backward over continuation-pattern bytes to the start of the
 * preceding codepoint. Returns 0 if @p byte_offset is already 0.
 *
 * @param text         UTF-8 encoded string.
 * @param byte_offset  Current byte offset, must be a codepoint boundary.
 * @return             Byte offset of the previous codepoint boundary.
 */
[[nodiscard]] MAYAFLUX_API size_t previous_codepoint_offset(std::string_view text, size_t byte_offset);

} // namespace MayaFlux::Portal::Text
