#pragma once

#include "FontFace.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace MayaFlux::Portal::Text {

/**
 * @struct GlyphMetrics
 * @brief Per-glyph layout and UV data produced by GlyphAtlas.
 *
 * UV coordinates are in normalised [0, 1] atlas space.
 * Bearing and size are in pixels at the atlas's declared pixel_size.
 *
 * face and glyph_index identify which FontFace actually produced this
 * glyph (the primary face, or a fallback on a primary miss) and its glyph
 * index within that specific face. Glyph indices are only meaningful
 * relative to the face that produced them: a consumer doing anything
 * face-sensitive (kerning) must compare face identity, not just index.
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

    const FontFace* face { nullptr }; ///< Face that produced this glyph (primary or fallback).
    FT_UInt glyph_index { 0 }; ///< Glyph index within `face` (from FT_Get_Char_Index).
};

/**
 * @class GlyphAtlas
 * @brief Rasterizes and packs glyphs from a FontFace into a TextureContainer.
 *
 * One atlas corresponds to one (FontFace, pixel_size) pair.  The atlas
 * texture is R8 (single-channel coverage); colour is applied in the shader.
 *
 * Glyphs are rasterized on first request via get_or_rasterize() and packed
 * into the atlas using a simple shelf packing algorithm. On shelf overflow
 * the atlas texture doubles in size and every previously cached glyph is
 * re-rasterized into the new texture from its owning face; rasterization
 * never permanently refuses for lack of room.
 *
 * The primary face is fixed at construction. A codepoint the primary face
 * lacks is looked up in the fallback chain (add_fallback()), in order, and
 * the first face reporting a glyph produces it; a codepoint absent from
 * every face fails as before. The atlas's cache is keyed on (face,
 * glyph_index) rather than glyph_index alone, since glyph indices are only
 * unique within the face that assigned them: the same numeric index from
 * two different faces names two unrelated glyphs.
 *
 * Callers obtain glyph indices via FT_Get_Char_Index on the FontFace. This
 * keeps the path open for HarfBuzz, which outputs glyph indices directly
 * (against the primary face; HarfBuzz does not participate in fallback).
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
     * @brief Access the backing FontFace.  Callers may query FT_Face properties
     * or call FT_Get_Char_Index directly for HarfBuzz integration.
     */
    [[nodiscard]] FontFace& get_face() { return m_face; }
    [[nodiscard]] const FontFace& get_face() const { return m_face; }

    /**
     * @brief Convenience: look up by Unicode codepoint.
     *
     * Tries the primary face first, then each fallback face in registration
     * order, returning the first hit. Prefer the glyph_index overload when
     * integrating with HarfBuzz output (primary face only, no fallback).
     *
     * @param codepoint  Unicode codepoint (e.g. U+0041 for 'A').
     * @return Pointer to cached GlyphMetrics, or nullptr if no face (primary
     *         or fallback) has this codepoint.
     */
    const GlyphMetrics* get_or_rasterize(FT_ULong codepoint);

    /**
     * @brief Register a fallback face, tried in order after the primary on a miss.
     *
     * @param face  Loaded FontFace. Must outlive this atlas. Sized to this
     *              atlas's pixel_size lazily, on first use, same as the
     *              primary face.
     */
    void add_fallback(FontFace& face);

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

    /**
     * @brief Line advance in pixels for this atlas's pixel_size.
     *
     * Derived from FT_Face metrics: (ascender - descender) in 26.6 fixed-point,
     * shifted right by 6. Returns pixel_size as a safe fallback if the face has
     * not yet been sized (i.e. before the first get_or_rasterize() call).
     */
    [[nodiscard]] uint32_t line_height() const;

    /// @brief Ascender in pixels for this atlas's pixel_size.
    [[nodiscard]] uint32_t ascender() const;

private:
    const GlyphMetrics* get_or_rasterize_from(FontFace& face, FT_UInt glyph_index);
    bool rasterize(FontFace& face, FT_UInt glyph_index);

    /// @brief Double the atlas texture and re-rasterize every cached glyph into it.
    void grow();

    FontFace& m_face;
    std::vector<FontFace*> m_fallbacks;
    uint32_t m_pixel_size;
    uint32_t m_atlas_size;

    std::unique_ptr<Kakshya::TextureContainer> m_texture;

    /// @brief Per-face glyph cache. Outer key is the producing FontFace,
    ///        since glyph indices are only unique within one face.
    std::unordered_map<FontFace*, std::unordered_map<FT_UInt, GlyphMetrics>> m_cache;

    uint32_t m_cursor_x { 0 };
    uint32_t m_cursor_y { 0 };
    uint32_t m_shelf_height { 0 };

    bool m_dirty { false };
};

} // namespace MayaFlux::Portal::Text
