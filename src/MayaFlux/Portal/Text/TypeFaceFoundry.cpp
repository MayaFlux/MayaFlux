#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Portal/Text/FontFace.hpp"
#include "MayaFlux/Portal/Text/GlyphAtlas.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

TypeFaceFoundry::TypeFaceFoundry() = default;

TypeFaceFoundry::~TypeFaceFoundry() { shutdown(); }

bool TypeFaceFoundry::initialize()
{
    if (m_library) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "TypeFaceFoundry already initialized");
        return true;
    }

    if (const FT_Error err = FT_Init_FreeType(&m_library); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FT_Init_FreeType failed with error {}", static_cast<int>(err));
        m_library = nullptr;
        return false;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "TypeFaceFoundry initialized");
    return true;
}

bool TypeFaceFoundry::set_default_font(const std::string& font_path, uint32_t pixel_size, uint32_t atlas_size)
{
    auto face = std::make_unique<FontFace>();
    if (!face->load(font_path)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "set_default_font: failed to load '{}'", font_path);
        return false;
    }

    auto atlas = std::make_unique<GlyphAtlas>(*face, pixel_size, atlas_size);

    m_default_atlas = std::move(atlas);
    m_default_face = std::move(face);

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Default font set: '{}' {}px atlas {}px", font_path, pixel_size, atlas_size);
    return true;
}

void TypeFaceFoundry::shutdown()
{
    if (!m_library) {
        return;
    }

    if (m_default_face) {
        m_default_face->unload();
    }

    FT_Done_FreeType(m_library);
    m_library = nullptr;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "TypeFaceFoundry shutdown");
}

} // namespace MayaFlux::Portal::Text
