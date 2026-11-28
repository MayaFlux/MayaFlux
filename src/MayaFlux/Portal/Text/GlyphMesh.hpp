#pragma once

#include "InkPress.hpp"
#include "TypeSetter.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @struct GlyphMap
 * @brief Linear map from layout pixels to whatever space the vertices are drawn in.
 *
 * lay_out() places glyphs in pixels with y growing downward. A map takes such
 * a point to output space as origin + pixel * scale, so one set of quads can
 * be drawn in NDC, a world plane or any other space by choosing the map.
 * Glyphs land on whole output pixels when scale is one pixel of that space
 * and origin lies on a pixel boundary.
 */
struct MAYAFLUX_API GlyphMap {
    glm::vec2 origin { 0.F };
    glm::vec2 scale { 1.F };

    /**
     * @brief Map layout pixels [0, source_px] onto @p region, y flipped.
     *
     * Pixel (0, 0) lands on the region's top left and @p source_px on its
     * bottom right, the same placement a texture of that size gets on a quad.
     *
     * @param region    Destination bounds, y up.
     * @param source_px Layout size in pixels. Components below one count as one.
     */
    [[nodiscard]] static GlyphMap fit(Kinesis::AABB2D region, glm::vec2 source_px) noexcept;

    [[nodiscard]] glm::vec2 operator()(glm::vec2 px) const noexcept { return origin + px * scale; }
};

/**
 * @brief MeshVertex::weight that makes a fragment shader read a texture slot
 *        as glyph coverage rather than as an image.
 *
 * Positive weights select a slot as an image, zero is a flat colour, and a
 * negative weight selects a slot as coverage: the shader outputs the vertex
 * colour with the slot's red channel, times @p alpha, as alpha. The whole part
 * of -weight is the slot plus one and the fraction carries the alpha (see
 * data/shaders/include/glyph_coverage.glsl).
 *
 * @param slot Zero based index of the atlas image in the shader's texture array.
 * @param alpha Opacity of the glyph, clamped to [0, 1].
 */
[[nodiscard]] constexpr float coverage_weight(uint32_t slot, float alpha = 1.0F) noexcept
{
    return -static_cast<float>(slot + 1) - (1.0F - std::clamp(alpha, 0.0F, 1.0F)) * 0.49F;
}

/**
 * @brief Triangle list vertices for glyph quads, six per quad.
 *
 * Positions come from @p map, UVs from the quads, and colour from the last
 * entry of @p spans whose byte range holds the quad's byte_offset, else
 * @p color. Every vertex's weight is coverage_weight(@p slot) with that
 * colour's alpha.
 *
 * @param quads Quads from lay_out().
 * @param color Colour of quads no span covers.
 * @param spans Optional per-byte-range colours, offsets into the laid out text.
 * @param map   Layout pixels to output space.
 * @param slot  Atlas image slot, see coverage_weight().
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::MeshVertex> glyph_vertices(
    std::span<const GlyphQuad> quads,
    glm::vec4 color,
    std::span<const StyleSpan> spans,
    const GlyphMap& map,
    uint32_t slot);

} // namespace MayaFlux::Portal::Text
