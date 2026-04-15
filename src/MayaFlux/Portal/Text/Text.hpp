#pragma once

namespace MayaFlux::Portal::Text {

class GlyphAtlas;

/**
 * @brief Initialize Portal::Text.
 *
 * Initialises FreeTypeContext.  Must be called before constructing any
 * FontFace or GlyphAtlas.  Safe to call after Portal::Graphics::initialize().
 *
 * @return true on success.
 */
MAYAFLUX_API bool initialize();

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
 * @brief Return the default GlyphAtlas, or nullptr if set_default_font()
 *        has not been called successfully.
 */
MAYAFLUX_API GlyphAtlas* get_default_glyph_atlas();

} // namespace MayaFlux::Portal::Text
