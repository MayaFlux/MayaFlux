#pragma once

namespace MayaFlux::Portal::Text {

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
 * Releases the FreeType library handle.  All FontFace and GlyphAtlas
 * instances must be destroyed before calling this.
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Returns true after a successful initialize() call.
 */
MAYAFLUX_API bool is_initialized();

} // namespace MayaFlux::Portal::Text
