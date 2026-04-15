#include "GlyphAtlas.hpp"

#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

GlyphAtlas::GlyphAtlas(FontFace& face, uint32_t pixel_size, uint32_t atlas_size)
    : m_face(face)
    , m_pixel_size(pixel_size)
    , m_atlas_size(atlas_size)
{
    m_texture = std::make_unique<Kakshya::TextureContainer>(
        atlas_size, atlas_size, Portal::Graphics::ImageFormat::R8);
}

const GlyphMetrics* GlyphAtlas::get_or_rasterize(FT_UInt glyph_index)
{
    auto it = m_cache.find(glyph_index);
    if (it != m_cache.end()) {
        return &it->second;
    }

    if (!rasterize(glyph_index)) {
        return nullptr;
    }

    return &m_cache.at(glyph_index);
}

const GlyphMetrics* GlyphAtlas::get_or_rasterize(FT_ULong codepoint)
{
    if (!m_face.is_loaded()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas::get_or_rasterize -- FontFace not loaded");
        return nullptr;
    }

    const FT_UInt idx = FT_Get_Char_Index(m_face.get_face(), codepoint);
    if (idx == 0) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas: no glyph for codepoint U+{:04X}", static_cast<uint32_t>(codepoint));
        return nullptr;
    }

    return get_or_rasterize(idx);
}

bool GlyphAtlas::rasterize(FT_UInt glyph_index)
{
    FT_Face face = m_face.get_face();

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

    const FT_Bitmap& bmp = face->glyph->bitmap;
    const uint32_t gw = bmp.width;
    const uint32_t gh = bmp.rows;

    constexpr uint32_t k_pad = 1;

    if (m_cursor_x + gw + k_pad > m_atlas_size) {
        m_cursor_x = 0;
        m_cursor_y += m_shelf_height + k_pad;
        m_shelf_height = 0;
    }

    if (m_cursor_y + gh + k_pad > m_atlas_size) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas full at pixel_size={} atlas_size={}. "
            "Construct with a larger atlas_size.",
            m_pixel_size, m_atlas_size);
        return false;
    }

    const std::span<uint8_t> pixels = m_texture->pixel_bytes(0);
    if (pixels.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "GlyphAtlas: TextureContainer pixel_bytes returned empty span");
        return false;
    }

    const uint32_t stride = m_atlas_size;

    for (uint32_t row = 0; row < gh; ++row) {
        const uint8_t* src = bmp.buffer + static_cast<size_t>(row * static_cast<uint32_t>(std::abs(bmp.pitch)));
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

    m_cache.emplace(glyph_index, m);

    m_cursor_x += gw + k_pad;
    if (gh > m_shelf_height) {
        m_shelf_height = gh;
    }

    m_dirty = true;
    return true;
}

} // namespace MayaFlux::Portal::Text
