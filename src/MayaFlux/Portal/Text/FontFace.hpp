#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>

namespace MayaFlux::Portal::Text {

/**
 * @class FontFace
 * @brief Owns a single FT_Face loaded from a file path.
 *
 * A FontFace is the source from which GlyphAtlas instances at specific
 * pixel sizes are constructed.  One FontFace may serve any number of
 * atlases; the FT_Face handle is shared by reference and must not be
 * used concurrently from multiple threads without external locking.
 *
 * The face index parameter handles font collections (.ttc, .otc); pass
 * 0 for single-face files.
 *
 * FontFace does not call FT_Set_Pixel_Sizes -- that is GlyphAtlas's
 * responsibility immediately before each rasterization call, ensuring
 * the face is configured for the atlas's declared size.
 */
class MAYAFLUX_API FontFace {
public:
    FontFace() = default;
    ~FontFace() { unload(); }

    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
    FontFace(FontFace&&) = delete;
    FontFace& operator=(FontFace&&) = delete;

    /**
     * @brief Load a font file from disk.
     * @param path  Absolute or relative path to a TTF, OTF, or collection file.
     * @param index Face index within a collection; 0 for ordinary font files.
     * @return true on success.
     * @pre FreeTypeContext::instance().is_initialized() must be true.
     */
    bool load(const std::string& path, FT_Long index = 0);

    /**
     * @brief Release the FT_Face handle.
     */
    void unload();

    /**
     * @brief Returns true after a successful load() call.
     */
    [[nodiscard]] bool is_loaded() const { return m_face != nullptr; }

    /**
     * @brief Raw FT_Face handle for use by GlyphAtlas.
     * @pre is_loaded() must be true.
     */
    [[nodiscard]] FT_Face get_face() const { return m_face; }

    /**
     * @brief Path passed to load(), empty if not yet loaded.
     */
    [[nodiscard]] const std::string& path() const { return m_path; }

private:
    FT_Face m_face { nullptr };
    std::string m_path;
};

} // namespace MayaFlux::Portal::Text
