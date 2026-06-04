#pragma once

#include "FontFace.hpp"

#include <glm/vec2.hpp>

namespace MayaFlux::Portal::Text {

/**
 * @struct GlyphOutline
 * @brief Vector outline for a single glyph as a flat polyline sequence.
 *
 * Each contour in the glyph outline is appended in order. Contour boundaries
 * are recorded in @p contour_ends: contour_ends[i] is the exclusive end index
 * into @p points for contour i. Points within a contour are in the winding
 * order specified by the font (counter-clockwise for filled regions, clockwise
 * for holes in TrueType; the caller interprets winding as needed).
 *
 * All coordinates are in pixels at the pixel_size passed to decompose_glyph(),
 * with Y increasing downward (matching GlyphQuad convention). The origin is the
 * glyph's pen origin: (0, 0) is the advance origin on the baseline.
 *
 * advance_x mirrors GlyphMetrics::advance_x for layout without a GlyphAtlas.
 */
struct GlyphOutline {
    uint32_t codepoint { 0 };
    std::vector<glm::vec2> points;
    std::vector<uint32_t> contour_ends;
    int32_t advance_x { 0 };
};

/**
 * @brief Decompose a Unicode codepoint into a tessellated polyline outline.
 *
 * Loads the glyph via FT_LOAD_NO_BITMAP, walks the FT_Outline with
 * FT_Outline_Decompose, and flattens conic and cubic Bezier segments into
 * line segments at the given flatness tolerance.
 *
 * The returned outline may be empty (points and contour_ends both empty) for
 * whitespace codepoints or when the codepoint has no outline (e.g. space, tab).
 * advance_x is still populated in that case so layout can proceed.
 *
 * @param face            Loaded FontFace. Must outlive this call.
 * @param codepoint       Unicode codepoint.
 * @param pixel_size      Glyph size in pixels. Passed to FT_Set_Pixel_Sizes.
 * @param tolerance       Flatness tolerance in pixels. Smaller values produce
 *                        more segments and higher fidelity curves.
 * @return                GlyphOutline with pixel-space polyline data.
 */
[[nodiscard]] MAYAFLUX_API GlyphOutline decompose_glyph(
    FontFace& face,
    uint32_t codepoint,
    uint32_t pixel_size,
    float tolerance = 0.5F);

/**
 * @brief Decompose a Unicode codepoint using the default atlas pixel size.
 *
 * Convenience overload that reads pixel_size from TypeFaceFoundry's default
 * atlas. Requires set_default_font() to have been called.
 *
 * @param codepoint  Unicode codepoint.
 * @param tolerance  Flatness tolerance in pixels.
 * @return           GlyphOutline, or empty outline on failure.
 */
[[nodiscard]] MAYAFLUX_API GlyphOutline decompose_glyph(
    uint32_t codepoint,
    float tolerance = 0.5F);

} // namespace MayaFlux::Portal::Text
