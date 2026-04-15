#include "Text.hpp"

#include "FreeTypeContext.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

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

    FreeTypeContext::instance().shutdown();

    g_initialized = false;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Text shutdown");
}

bool is_initialized()
{
    return g_initialized;
}

} // namespace MayaFlux::Portal::Text
