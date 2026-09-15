#include "GlyphAtlas.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

GlyphAtlas::GlyphAtlas(FontFace& face, uint32_t pixel_size, uint32_t atlas_size)
    : m_face(face)
    , m_pixel_size(pixel_size)
    , m_atlas_size(atlas_size)
{
    m_texture = std::make_unique<Kakshya::TextureContainer>(
        atlas_size, atlas_size, Portal::Graphics::ImageFormat::R8);

    FT_Set_Pixel_Sizes(m_face.get_face(), 0, m_pixel_size);
}

const GlyphMetrics* GlyphAtlas::get_or_rasterize(FT_UInt glyph_index)
{
    return get_or_rasterize_from(m_face, glyph_index);
}

const GlyphMetrics* GlyphAtlas::get_or_rasterize(FT_ULong codepoint)
{
    if (!m_face.is_loaded()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas::get_or_rasterize: FontFace not loaded");
        return nullptr;
    }

    if (const FT_UInt idx = FT_Get_Char_Index(m_face.get_face(), codepoint); idx != 0) {
        return get_or_rasterize_from(m_face, idx);
    }

    for (FontFace* fallback : m_fallbacks) {
        if (!fallback || !fallback->is_loaded()) {
            continue;
        }
        if (const FT_UInt idx = FT_Get_Char_Index(fallback->get_face(), codepoint); idx != 0) {
            return get_or_rasterize_from(*fallback, idx);
        }
    }

    MF_WARN(Journal::Component::Portal, Journal::Context::API,
        "GlyphAtlas: no glyph for codepoint U+{:04X} in primary or {} fallback face(s)",
        static_cast<uint32_t>(codepoint), m_fallbacks.size());
    return nullptr;
}

const GlyphMetrics* GlyphAtlas::get_or_rasterize_from(FontFace& face, FT_UInt glyph_index)
{
    auto& face_cache = m_cache[&face];
    if (auto it = face_cache.find(glyph_index); it != face_cache.end()) {
        return &it->second;
    }

    if (!rasterize(face, glyph_index)) {
        return nullptr;
    }

    return &m_cache.at(&face).at(glyph_index);
}

void GlyphAtlas::add_fallback(FontFace& face)
{
    m_fallbacks.push_back(&face);
}

[[nodiscard]] uint32_t GlyphAtlas::line_height() const
{
    FT_Face face = m_face.get_face();
    if (!face || face->size == nullptr) {
        return m_pixel_size;
    }
    const int32_t h = (face->size->metrics.ascender - face->size->metrics.descender) >> 6;
    return h > 0 ? static_cast<uint32_t>(h) : m_pixel_size;
}

[[nodiscard]] uint32_t GlyphAtlas::ascender() const
{
    FT_Face face = m_face.get_face();
    if (!face || face->size == nullptr)
        return m_pixel_size;
    const int32_t a = face->size->metrics.ascender >> 6;
    return a > 0 ? static_cast<uint32_t>(a) : m_pixel_size;
}

void GlyphAtlas::grow()
{
    const uint32_t new_size = m_atlas_size * 2;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "GlyphAtlas: growing {}x{} -> {}x{} (pixel_size={})",
        m_atlas_size, m_atlas_size, new_size, new_size, m_pixel_size);

    struct CachedGlyph {
        FontFace* face;
        FT_UInt glyph_index;
    };

    std::vector<CachedGlyph> cached;
    for (const auto& [face, glyphs] : m_cache) {
        for (const auto& glyph_entry : glyphs) {
            cached.push_back({ face, glyph_entry.first });
        }
    }

    m_atlas_size = new_size;
    m_texture = std::make_unique<Kakshya::TextureContainer>(
        new_size, new_size, Portal::Graphics::ImageFormat::R8);

    m_cursor_x = 0;
    m_cursor_y = 0;
    m_shelf_height = 0;
    m_cache.clear();

    for (const auto& c : cached) {
        rasterize(*c.face, c.glyph_index);
    }

    m_dirty = true;
}

bool GlyphAtlas::rasterize(FontFace& face_ref, FT_UInt glyph_index)
{
    FT_Face face = face_ref.get_face();

    if (const FT_Error err = FT_Set_Pixel_Sizes(face, 0, m_pixel_size); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FT_Set_Pixel_Sizes({}) failed: {}", m_pixel_size, static_cast<int>(err));
        return false;
    }

    if (const FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FT_Load_Glyph({}) failed: {}", glyph_index, static_cast<int>(err));
        return false;
    }

    const FT_Bitmap* bmp = &face->glyph->bitmap;
    uint32_t gw = bmp->width;
    uint32_t gh = bmp->rows;

    constexpr uint32_t k_pad = 1;

    if (m_cursor_x + gw + k_pad > m_atlas_size) {
        m_cursor_x = 0;
        m_cursor_y += m_shelf_height + k_pad;
        m_shelf_height = 0;
    }

    if (m_cursor_y + gh + k_pad > m_atlas_size) {
        grow();

        // grow() re-rasterized every previously cached glyph, possibly from
        // other faces, which overwrites face->glyph in between; reload
        // this glyph before placing it in the now-larger atlas.
        if (const FT_Error err = FT_Set_Pixel_Sizes(face, 0, m_pixel_size); err != 0) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "FT_Set_Pixel_Sizes({}) failed after growth: {}", m_pixel_size, static_cast<int>(err));
            return false;
        }
        if (const FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); err != 0) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "FT_Load_Glyph({}) failed after growth: {}", glyph_index, static_cast<int>(err));
            return false;
        }

        bmp = &face->glyph->bitmap;
        gw = bmp->width;
        gh = bmp->rows;

        if (m_cursor_x + gw + k_pad > m_atlas_size) {
            m_cursor_x = 0;
            m_cursor_y += m_shelf_height + k_pad;
            m_shelf_height = 0;
        }

        if (m_cursor_y + gh + k_pad > m_atlas_size) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "GlyphAtlas still full after growth to {}x{} at pixel_size={}",
                m_atlas_size, m_atlas_size, m_pixel_size);
            return false;
        }
    }

    const std::span<uint8_t> pixels = m_texture->pixel_bytes(0);
    if (pixels.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas: TextureContainer pixel_bytes returned empty span");
        return false;
    }

    const uint32_t stride = m_atlas_size;

    for (uint32_t row = 0; row < gh; ++row) {
        const uint8_t* src = bmp->buffer + static_cast<size_t>(row * static_cast<uint32_t>(std::abs(bmp->pitch)));
        uint8_t* dst = pixels.data() + static_cast<size_t>((m_cursor_y + row) * stride) + m_cursor_x;
        std::memcpy(dst, src, gw);
    }

    const float inv = 1.F / static_cast<float>(m_atlas_size);

    GlyphMetrics m;
    m.uv_x0 = static_cast<float>(m_cursor_x) * inv;
    m.uv_y0 = static_cast<float>(m_cursor_y) * inv;
    m.uv_x1 = static_cast<float>(m_cursor_x + gw) * inv;
    m.uv_y1 = static_cast<float>(m_cursor_y + gh) * inv;
    m.bearing_x = face->glyph->bitmap_left;
    m.bearing_y = face->glyph->bitmap_top;
    m.width = gw;
    m.height = gh;
    m.advance_x = static_cast<int32_t>(face->glyph->advance.x >> 6);
    m.face = &face_ref;
    m.glyph_index = glyph_index;

    m_cache[&face_ref].emplace(glyph_index, m);

    m_cursor_x += gw + k_pad;
    if (gh > m_shelf_height) {
        m_shelf_height = gh;
    }

    m_dirty = true;
    return true;
}

} // namespace MayaFlux::Portal::Text
