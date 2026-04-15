#include "Text.hpp"

#include "FontFace.hpp"
#include "FreeTypeContext.hpp"
#include "GlyphAtlas.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

namespace {
    bool g_initialized {};
    std::unique_ptr<FontFace> g_default_face;
    std::unique_ptr<GlyphAtlas> g_default_atlas;
}

bool initialize()
{
    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::Text already initialized");
        return true;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Initializing Portal::Text...");

    if (!FreeTypeContext::instance().initialize()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize FreeTypeContext");
        return false;
    }

    g_initialized = true;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Text initialized");
    return true;
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Shutting down Portal::Text...");

    g_default_atlas.reset();
    g_default_face.reset();

    FreeTypeContext::instance().shutdown();

    g_initialized = false;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Text shutdown");
}

bool is_initialized()
{
    return g_initialized;
}

bool set_default_font(const std::string& font_path, uint32_t pixel_size, uint32_t atlas_size)
{
    if (!g_initialized) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Portal::Text::set_default_font called before initialize()");
        return false;
    }

    auto face = std::make_unique<FontFace>();
    if (!face->load(font_path)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "set_default_font: failed to load '{}'", font_path);
        return false;
    }

    auto atlas = std::make_unique<GlyphAtlas>(*face, pixel_size, atlas_size);

    g_default_atlas = std::move(atlas);
    g_default_face = std::move(face);

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Default font set: '{}' {}px atlas {}px", font_path, pixel_size, atlas_size);
    return true;
}

GlyphAtlas* get_default_glyph_atlas()
{
    return g_default_atlas.get();
}

} // namespace MayaFlux::Portal::Text
