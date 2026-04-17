#pragma once

#include "MayaFlux/Portal/Text/TypeSetter.hpp"

namespace MayaFlux::Core {
struct TextConfig;
}

namespace MayaFlux::Portal::Text {

class GlyphAtlas;

/**
 * @brief Initialize Portal::Text.
 *
 * Initialises TypeFaceFoundry.  Must be called before constructing any
 * FontFace or GlyphAtlas.  Safe to call after Portal::Graphics::initialize().
 *
 * @return true on success.
 */
MAYAFLUX_API bool initialize(std::optional<Core::TextConfig> config);

/**
 * @brief Shutdown Portal::Text.
 *
 * Releases the FreeType library handle and destroys the default atlas if
 * one was set.  All FontFace and GlyphAtlas instances must be destroyed
 * before calling this.
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Returns true after a successful initialize() call.
 */
MAYAFLUX_API bool is_initialized();

/**
 * @brief Load a font file and create the default GlyphAtlas at a given size.
 *
 * Replaces any previously set default font.  Must be called after
 * initialize().
 *
 * @param font_path   Path to a TTF or OTF file.
 * @param pixel_size  Glyph height in pixels.
 * @param atlas_size  Atlas texture dimension (power of two, default 512).
 * @return true on success.
 */
MAYAFLUX_API bool set_default_font(
    const std::string& font_path,
    uint32_t pixel_size = 24,
    uint32_t atlas_size = 512);

/**
 * @brief Locate a system font by family and style, then load it as the default.
 *
 * Delegates font path resolution to Platform::find_font, then calls the
 * path-based overload.  Returns false and logs if the family cannot be
 * located on the current platform.
 *
 * @param family      Font family name, e.g. "JetBrains Mono".
 * @param style       Style hint, e.g. "Medium", "Bold".
 * @param pixel_size  Glyph height in pixels.
 * @param atlas_size  Atlas texture dimension (power of two, default 512).
 * @return true on success.
 */
MAYAFLUX_API bool set_default_font(
    std::string_view family,
    std::string_view style,
    uint32_t pixel_size,
    uint32_t atlas_size = 512);

/**
 * @brief Lay out a UTF-8 string into screen-space glyph quads using the default atlas.
 *
 * Each quad in the returned LayoutResult carries pixel-space position, atlas
 * UV coordinates, and the Unicode codepoint that produced it. The caller may
 * mutate the quads freely before passing them to rasterize_quads().
 *
 * @param text    UTF-8 encoded input string.
 * @param pen_x   Starting horizontal pen position in pixels.
 * @param pen_y   Starting vertical pen position in pixels (baseline).
 * @param wrap_w  Wrap boundary in pixels. Zero disables wrapping.
 * @return        LayoutResult containing quads and final pen position.
 * @pre           set_default_font() must have been called successfully.
 */
[[nodiscard]] MAYAFLUX_API LayoutResult create_layout(
    std::string_view text,
    float pen_x = 0.F,
    float pen_y = 0.F,
    uint32_t wrap_w = 0);

/**
 * @brief Return the default GlyphAtlas, or nullptr if set_default_font()
 *        has not been called successfully.
 */
MAYAFLUX_API GlyphAtlas& get_default_atlas();

} // namespace MayaFlux::Portal::Text
