#pragma once

#include "GlyphAtlas.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @brief Render a UTF-8 string into a TextureBuffer ready for display.
 *
 * Calls lay_out() to produce GlyphQuad geometry, composites the glyph
 * bitmaps from the atlas into a fresh R8 TextureContainer sized to the
 * tight bounding box of the laid-out text, then wraps that in a
 * TextureBuffer.
 *
 * The returned TextureBuffer uses the default texture.vert.spv /
 * texture.frag.spv shaders.  The glyph coverage from the R8 atlas is
 * pre-multiplied by @p color and baked into an RGBA8 pixel buffer, so no
 * custom shader or push constants are required.  The caller only needs to
 * call setup_rendering() with a target window.
 *
 * Calling render_text() again with a different color is cheap after atlas
 * warm-up: glyph rasterization is cached, so only the bounding box
 * calculation and memcpy composite run on subsequent calls.
 *
 * @param text        UTF-8 string to render.
 * @param atlas       GlyphAtlas to source glyph bitmaps from.
 * @param color       RGBA text color passed as push constant to text.frag.
 * @return            Initialized TextureBuffer, or nullptr if text is empty
 *                    or no glyphs could be laid out.
 */
[[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> render_text(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

} // namespace MayaFlux::Portal::Text
