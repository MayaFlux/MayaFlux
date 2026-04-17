#include "Text.hpp"

#include "GlyphAtlas.hpp"
#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Transitive/Platform/FontDiscovery.hpp"

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

namespace {
    bool g_initialized {};
}

bool initialize(std::optional<Core::TextConfig> config)
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

    if (config) {
        const auto& [family, style, pixel_size, atlas_size] = *config;
        if (!set_default_font(family, style, pixel_size, atlas_size)) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
                "Failed to set default font '{}{}{}'", family, style.empty() ? "" : " ", style);
        }
    } else {
        MF_INFO(Journal::Component::Portal, Journal::Context::API,
            "No default font configured for Portal::Text");
    }

    g_initialized = true;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Text initialized");
    return true;
}

std::shared_ptr<LayoutResult> create_layout(
    std::string_view text,
    float pen_x,
    float pen_y,
    uint32_t wrap_w)
{
    return std::make_shared<LayoutResult>(lay_out(text, get_default_atlas(), pen_x, pen_y, wrap_w));
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

GlyphAtlas& get_default_atlas()
{
    GlyphAtlas* atlas = TypeFaceFoundry::instance().get_default_glyph_atlas();
    MF_ASSERT(Journal::Component::Portal, Journal::Context::API,
        atlas != nullptr, "call set_default_font before get_default_atlas");
    return *atlas;
}

} // namespace MayaFlux::Portal::Text
