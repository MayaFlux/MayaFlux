#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

namespace MayaFlux::Portal::Text {

class FontFace;
class GlyphAtlas;

/**
 * @class TypeFaceFoundry
 * @brief Singleton owner of the FT_Library handle.
 *
 * Initialised once by Portal::Text::initialize() and shut down by
 * Portal::Text::shutdown().  All FontFace instances borrow the library
 * handle via get_library(); they must not outlive this singleton.
 */
class MAYAFLUX_API TypeFaceFoundry {
public:
    static TypeFaceFoundry& instance()
    {
        static TypeFaceFoundry ctx;
        return ctx;
    }

    TypeFaceFoundry(const TypeFaceFoundry&) = delete;
    TypeFaceFoundry& operator=(const TypeFaceFoundry&) = delete;
    TypeFaceFoundry(TypeFaceFoundry&&) = delete;
    TypeFaceFoundry& operator=(TypeFaceFoundry&&) = delete;

    /**
     * @brief Initialise the FreeType library.
     * @return true on success, false if FT_Init_FreeType fails.
     */
    bool initialize();

    /**
     * @brief Release the FreeType library handle.
     */
    void shutdown();

    /**
     * @brief Returns true after a successful initialize() call.
     */
    [[nodiscard]] bool is_initialized() const { return m_library != nullptr; }

    /**
     * @brief Raw FT_Library handle for use by FontFace.
     * @pre is_initialized() must be true.
     */
    [[nodiscard]] FT_Library get_library() const { return m_library; }

    /**
     * @brief Locate a system font by family and style, then load it as the default.
     *
     * Delegates font path resolution to find_font, then calls the
     * path-based overload.  Returns false and logs if the family cannot be
     * located on the current platform.
     *
     * @param family      Font family name, e.g. "JetBrains Mono".
     * @param style       Style hint, e.g. "Medium", "Bold".
     * @param pixel_size  Glyph height in pixels.
     * @param atlas_size  Atlas texture dimension (power of two, default 512).
     * @return true on success.
     */
    bool set_default_font(const std::string& path, uint32_t pixel_size, uint32_t atlas_size = 512);

    /**
     * @brief Return the default GlyphAtlas, or nullptr if set_default_font()
     *        has not been called successfully.
     */
    [[nodiscard]] GlyphAtlas* get_default_glyph_atlas() const { return m_default_atlas.get(); }

    /**
     * @brief Return the default FontFace, or nullptr if set_default_font() has
     *        not been called successfully.
     */
    [[nodiscard]] FontFace* get_default_face() const { return m_default_face.get(); }

    /**
     * @brief Load a font file and register it as a fallback on the default atlas.
     *
     * Tried, in registration order, whenever the default atlas's primary
     * face lacks a codepoint. Requires set_default_font() to have been
     * called first. Fallback faces are retained across a later
     * set_default_font() call and re-registered on whatever new atlas that
     * call constructs, so a font switch doesn't silently drop them.
     *
     * @param font_path  Path to a TTF or OTF file.
     * @return true on success.
     */
    bool add_fallback_font(const std::string& font_path);

private:
    TypeFaceFoundry();
    ~TypeFaceFoundry();

    FT_Library m_library { nullptr };

    std::unique_ptr<FontFace> m_default_face;
    std::unique_ptr<GlyphAtlas> m_default_atlas;
    std::vector<std::unique_ptr<FontFace>> m_fallback_faces;
};

} // namespace MayaFlux::Portal::Text
