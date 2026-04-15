#pragma once

#include "FontFace.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace MayaFlux::Portal::Text {

/**
 * @struct GlyphMetrics
 * @brief Per-glyph layout and UV data produced by GlyphAtlas.
 *
 * UV coordinates are in normalised [0, 1] atlas space.
 * Bearing and size are in pixels at the atlas's declared pixel_size.
 *
 * When HarfBuzz is introduced it will supply glyph_index and position
 * offsets directly; this struct remains the downstream currency.
 */
struct GlyphMetrics {
    float uv_x0 { 0.F }; ///< Left UV edge in atlas texture.
    float uv_y0 { 0.F }; ///< Top UV edge in atlas texture.
    float uv_x1 { 0.F }; ///< Right UV edge in atlas texture.
    float uv_y1 { 0.F }; ///< Bottom UV edge in atlas texture.

    int32_t bearing_x { 0 }; ///< Horizontal bearing in pixels (from FT_GlyphSlot).
    int32_t bearing_y { 0 }; ///< Vertical bearing in pixels (from FT_GlyphSlot).
    uint32_t width { 0 }; ///< Glyph bitmap width in pixels.
    uint32_t height { 0 }; ///< Glyph bitmap height in pixels.
    int32_t advance_x { 0 }; ///< Horizontal advance in pixels (26.6 fixed-point >> 6).
};

/**
 * @class GlyphAtlas
 * @brief Rasterizes and packs glyphs from a FontFace into a TextureContainer.
 *
 * One atlas corresponds to one (FontFace, pixel_size) pair.  The atlas
 * texture is R8 (single-channel coverage); colour is applied in the shader.
 *
 * Glyphs are rasterized on first request via get_or_rasterize() and packed
 * into the atlas using a simple shelf packing algorithm.  The atlas texture
 * is rebuilt whenever a new glyph causes a shelf overflow.
 *
 * The atlas is keyed on FT_UInt glyph index, not Unicode codepoint.
 * Callers obtain glyph indices via FT_Get_Char_Index on the FontFace.
 * This keeps the path open for HarfBuzz, which outputs glyph indices directly.
 *
 * Thread safety: not thread-safe.  Rasterization must occur on the thread
 * that owns the FontFace.
 */
class MAYAFLUX_API GlyphAtlas {
public:
    /**
     * @brief Construct an atlas for a specific face and pixel size.
     * @param face        Loaded FontFace.  Must outlive this atlas.
     * @param pixel_size  Glyph height in pixels (width is derived by FreeType).
     * @param atlas_size  Width and height of the atlas texture in pixels.
     *                    Must be a power of two.  Default 512 is sufficient
     *                    for ASCII + extended Latin at sizes up to ~48px.
     */
    explicit GlyphAtlas(
        FontFace& face,
        uint32_t pixel_size,
        uint32_t atlas_size = 512);

    ~GlyphAtlas() = default;

    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;
    GlyphAtlas(GlyphAtlas&&) = delete;
    GlyphAtlas& operator=(GlyphAtlas&&) = delete;

    /**
     * @brief Return metrics for a glyph, rasterizing it into the atlas if needed.
     * @param glyph_index  FT_UInt glyph index (from FT_Get_Char_Index).
     * @return Pointer to cached GlyphMetrics, or nullptr if rasterization fails.
     */
    const GlyphMetrics* get_or_rasterize(FT_UInt glyph_index);

    /**
     * @brief Convenience: look up by Unicode codepoint.
     *
     * Calls FT_Get_Char_Index internally.  Prefer the glyph_index overload
     * when integrating with HarfBuzz output.
     *
     * @param codepoint  Unicode codepoint (e.g. U+0041 for 'A').
     * @return Pointer to cached GlyphMetrics, or nullptr on failure.
     */
    const GlyphMetrics* get_or_rasterize(FT_ULong codepoint);

    /**
     * @brief The atlas texture as a TextureContainer (R8, atlas_size x atlas_size).
     *
     * The container is valid after construction.  Its pixel data is updated
     * in-place as new glyphs are rasterized; callers must re-upload to GPU
     * after any rasterization call that returns non-null.
     */
    [[nodiscard]] const Kakshya::TextureContainer& texture() const { return *m_texture; }
    [[nodiscard]] Kakshya::TextureContainer& texture() { return *m_texture; }

    /**
     * @brief Returns true if at least one glyph was rasterized since the last
     *        call to clear_dirty().  Use to decide whether to re-upload to GPU.
     */
    [[nodiscard]] bool is_dirty() const { return m_dirty; }

    /**
     * @brief Clear the dirty flag after re-uploading the atlas to GPU.
     */
    void clear_dirty() { m_dirty = false; }

    /**
     * @brief Pixel size passed at construction.
     */
    [[nodiscard]] uint32_t pixel_size() const { return m_pixel_size; }

    /**
     * @brief Atlas texture dimension (width == height == atlas_size).
     */
    [[nodiscard]] uint32_t atlas_size() const { return m_atlas_size; }

private:
    bool rasterize(FT_UInt glyph_index);

    FontFace& m_face;
    uint32_t m_pixel_size;
    uint32_t m_atlas_size;

    std::unique_ptr<Kakshya::TextureContainer> m_texture;

    std::unordered_map<FT_UInt, GlyphMetrics> m_cache;

    uint32_t m_cursor_x { 0 };
    uint32_t m_cursor_y { 0 };
    uint32_t m_shelf_height { 0 };

    bool m_dirty { false };
};

} // namespace MayaFlux::Portal::Text
