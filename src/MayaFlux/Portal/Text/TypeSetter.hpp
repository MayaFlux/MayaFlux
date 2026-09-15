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
    size_t byte_offset { 0 }; ///< Byte offset of codepoint's first byte in the source text passed to lay_out().
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
 * Every codepoint this text needs is rasterized (cached, if not already)
 * before any quad is emitted, so a GlyphAtlas growth triggered partway
 * through this text cannot invalidate the UV coordinates already baked into
 * quads emitted earlier in the same call. This guarantee is per-call only:
 * a LayoutResult held across later, unrelated lay_out() calls on the same
 * atlas is not protected if the atlas grows afterward from something else.
 *
 * Every codepoint for which is_control() is true is never rasterized or
 * counted as printable content. Among those, \t advances pen_x to the next
 * tab stop and \n advances pen_y by atlas.line_height() and resets pen_x;
 * every other control codepoint (including \r) is skipped with no side
 * effect. This is the same is_control() identification raw-string text
 * handling uses; only the reaction to it differs here.
 *
 * Consecutive non-control glyphs are kerned via FT_Get_Kerning() when the
 * producing face (GlyphMetrics::face) has a kerning table. The previous
 * glyph index resets to none across any control codepoint, across a
 * wrap-induced line break, and whenever the producing face changes between
 * consecutive glyphs: a glyph index is only meaningful within the face
 * that assigned it, so a codepoint resolved through GlyphAtlas's fallback
 * chain never gets kerned against a glyph from a different face.
 *
 * Wrapping at wrap_w is a pure render-bounds guarantee, not word-aware line
 * breaking: it breaks mid-word freely, with no whitespace lookback. This is
 * deliberate - correctness here means no glyph ever renders past wrap_w, not
 * that words stay intact. Each glyph is checked before it is placed, against
 * its own advance width times a 1.5x margin, so the decision to wrap accounts
 * for the glyph about to be drawn rather than reacting one glyph late to
 * cumulative pen_x history - the margin absorbs the gap between a glyph's
 * advance and its true rendered width so a glyph is never placed half past
 * the boundary. A tab is checked the same way against its own tab-stop
 * advance (no margin, since it draws nothing itself) immediately after
 * computing its destination, not deferred to the next glyph's turn.
 *
 * Bidirectional reordering and shaping are not performed. HarfBuzz slots in
 * before the glyph index step when needed; the quad assembly loop and
 * GlyphAtlas remain unchanged.
 *
 * Each quad's byte_offset is the position of its codepoint's first byte in
 * @p text, letting a caller correlate a quad back to source text (styling,
 * hit-testing, extraction) without re-decoding UTF-8 itself.
 *
 * @param text       UTF-8 encoded input string.
 * @param atlas      GlyphAtlas to query and populate. May be modified (dirty
 *                   flag set) if new glyphs are rasterized.
 * @param pen_x      Starting horizontal pen position in pixels.
 * @param pen_y      Starting vertical pen position in pixels (baseline).
 * @param wrap_w     Column to wrap at in pixels, or 0 to disable wrapping.
 * @param tab_width  Tab stop width in space-glyph-widths.
 * @return           LayoutResult containing quads and final pen position.
 */
[[nodiscard]] LayoutResult lay_out(
    std::string_view text,
    GlyphAtlas& atlas,
    float pen_x = 0.F,
    float pen_y = 0.F,
    uint32_t wrap_w = 0,
    uint32_t tab_width = 4);

/**
 * @brief True if a codepoint must never be rasterized or treated as
 *        printable content.
 *
 * Covers Unicode categories Cc (control), Cf (format), Cs (surrogate), and
 * Co (private use) via utf8proc_category(). This is the one fact every
 * Portal::Text consumer (layout, raw-string editing) shares about a
 * codepoint; what each does in response - skip, reposition a pen, refuse an
 * insertion - is theirs to decide.
 *
 * @param codepoint  Unicode codepoint to classify.
 * @return           true if the codepoint must never produce a glyph.
 */
[[nodiscard]] MAYAFLUX_API bool is_control(uint32_t codepoint) noexcept;

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

/**
 * @struct CopiedText
 * @brief Result of copy(): a byte range's text plus the quads that render it.
 */
struct CopiedText {
    std::string text; ///< Substring of the source text in [start, end).
    size_t start { 0 }; ///< Normalized, clamped byte range actually used.
    size_t end { 0 };
    std::vector<GlyphQuad> quads; ///< Matching quads, at their original layout positions.
};

/**
 * @brief Copy a byte range: the text it spans, and the quads that render it.
 *
 * start/end are normalized (min/max) and clamped to [0, text.size()]. Both
 * are assumed to already be codepoint boundaries, the same assumption
 * next_codepoint_offset()/previous_codepoint_offset() maintain for a
 * caller's cursor; this does not re-snap them to one.
 *
 * Quads are taken from @p layout, matched by GlyphQuad::byte_offset falling
 * in the normalized range, at whatever position they were laid out at.
 * Shift them yourself if the copy needs to be positioned relative to its
 * own origin rather than the source layout's. @p layout must be the result
 * of laying out this same @p text (or one whose byte offsets line up with
 * it); passing a layout for unrelated text returns a meaningless, but not
 * unsafe, quads list.
 *
 * @param text    Source UTF-8 string the byte range indexes into.
 * @param layout  LayoutResult produced from that same text.
 * @param a       One end of the byte range.
 * @param b       The other end of the byte range.
 * @return        The extracted text, its normalized range, and matching quads.
 */
[[nodiscard]] MAYAFLUX_API CopiedText copy(
    std::string_view text,
    const LayoutResult& layout,
    size_t a,
    size_t b);

/**
 * @struct PenPosition
 * @brief Pen coordinates returned by x_at(), in the same space as GlyphQuad.
 */
struct PenPosition {
    float x { 0.F };
    float y { 0.F };
};

/**
 * @brief Byte offset of the codepoint boundary nearest a screen-space point.
 *
 * Scans layout.quads for the line whose quads sit closest to @p y, then
 * within that line for the ink quad @p x falls into (resolving to its left
 * or right half), or, failing that, for the two quads bracketing @p x.
 * lay_out() never emits a quad for a space, tab, or newline, so a point
 * landing in the gap between two quads - or before the first / after the
 * last quad on a line - has no ink to test against directly: that gap is
 * resolved exactly by walking its few codepoint boundaries through x_at()
 * (same pen_x/pen_y/wrap_w/tab_width as the original lay_out() call) and
 * keeping whichever pen position lands closest to @p x, rather than
 * guessing from ink edges that were never meant to mark it.
 *
 * A line with no quads at all (blank, or entirely whitespace) has nothing
 * for the first scan to anchor on and is still not distinguishable from an
 * adjacent line by this function - a real, if narrower, residual gap.
 * Empty layout.quads (the whole text is blank or all-whitespace) returns 0.
 *
 * @param text        Source UTF-8 string layout was produced from.
 * @param layout      LayoutResult to hit-test against.
 * @param atlas       GlyphAtlas layout was produced with.
 * @param x           Screen-space x coordinate, same space as layout's quads.
 * @param y           Screen-space y coordinate, same space as layout's quads.
 * @param pen_x       Pen origin x, matching the original lay_out() call.
 * @param pen_y       Pen origin y, matching the original lay_out() call.
 * @param wrap_w      Wrap column, matching the original lay_out() call.
 * @param tab_width   Tab stop width, matching the original lay_out() call.
 * @return            Byte offset of the nearest codepoint boundary.
 */
[[nodiscard]] MAYAFLUX_API size_t index_at(
    std::string_view text,
    const LayoutResult& layout,
    GlyphAtlas& atlas,
    float x,
    float y,
    float pen_x = 0.F,
    float pen_y = 0.F,
    uint32_t wrap_w = 0,
    uint32_t tab_width = 4);

/**
 * @brief Pen position immediately before @p byte_offset, in lay_out()'s space.
 *
 * Re-lays-out the text up to @p byte_offset (clamped to text.size()) with
 * the same pen origin, wrap_w, and tab_width the caller used to produce the
 * original layout, and returns the resulting final_pen_x/final_pen_y. Wrap
 * decisions before a given offset only depend on codepoints before it, so
 * this reproduces the exact pen position lay_out() would have held at that
 * point in the original call - including on a space, tab, newline, wrapped
 * line start, or the very end of the text, none of which index_at()'s
 * quad-based sibling has a quad to answer from.
 *
 * This re-walks the prefix on every call rather than caching intermediate
 * pen positions; fine for interactive caret placement, not meant for a tight
 * per-frame loop over long text.
 *
 * @param text        Source UTF-8 string to re-lay-out a prefix of.
 * @param atlas       GlyphAtlas to query (same one the original layout used).
 * @param byte_offset Codepoint-boundary byte offset to resolve.
 * @param pen_x       Pen origin x, matching the original lay_out() call.
 * @param pen_y       Pen origin y, matching the original lay_out() call.
 * @param wrap_w      Wrap column, matching the original lay_out() call.
 * @param tab_width   Tab stop width, matching the original lay_out() call.
 * @return            Pen position immediately before byte_offset.
 */
[[nodiscard]] MAYAFLUX_API PenPosition x_at(
    std::string_view text,
    GlyphAtlas& atlas,
    size_t byte_offset,
    float pen_x = 0.F,
    float pen_y = 0.F,
    uint32_t wrap_w = 0,
    uint32_t tab_width = 4);

} // namespace MayaFlux::Portal::Text
