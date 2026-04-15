#include "Text.hpp"

#include "FontFace.hpp"
#include "GlyphAtlas.hpp"
#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Transitive/Platform/FontDiscovery.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {
// remove:
namespace {
    bool g_initialized {};
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

    if (!TypeFaceFoundry::instance().initialize()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize TypeFaceFoundry");
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

    TypeFaceFoundry::instance().shutdown();

    g_initialized = false;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Text shutdown");
}

bool is_initialized()
{
    return g_initialized;
}

bool set_default_font(
    std::string_view family,
    std::string_view style,
    uint32_t pixel_size,
    uint32_t atlas_size)
{
    const auto path = Platform::find_font(family, style);
    if (!path) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "set_default_font: could not locate '{}{}{}' on this system",
            family,
            style.empty() ? "" : " ",
            style);
        return false;
    }
    return set_default_font(*path, pixel_size, atlas_size);
}

bool set_default_font(const std::string& path, uint32_t pixel_size, uint32_t atlas_size)
{
    return TypeFaceFoundry::instance().set_default_font(path, pixel_size, atlas_size);
}

GlyphAtlas* get_atlas()
{
    return TypeFaceFoundry::instance().get_default_glyph_atlas();
}

} // namespace MayaFlux::Portal::Text
