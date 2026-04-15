#include "FontFace.hpp"

#include "FreeTypeContext.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

bool FontFace::load(const std::string& path, FT_Long index)
{
    unload();

    auto& ctx = FreeTypeContext::instance();
    if (!ctx.is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FontFace::load called before FreeTypeContext is initialized");
        return false;
    }

    if (const FT_Error err = FT_New_Face(ctx.get_library(), path.c_str(), index, &m_face);
        err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FT_New_Face failed for '{}' (index {}) with error {}",
            path, index, static_cast<int>(err));
        m_face = nullptr;
        return false;
    }

    m_path = path;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "FontFace loaded: '{}' ({} glyphs)", path, m_face->num_glyphs);
    return true;
}

void FontFace::unload()
{
    if (!m_face) {
        return;
    }

    FT_Done_Face(m_face);
    m_face = nullptr;
    m_path.clear();
}

} // namespace MayaFlux::Portal::Text
