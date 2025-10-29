#include "Graphics.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

namespace {
    bool g_initialized {};
}

bool initialize(const std::shared_ptr<Core::VulkanBackend>& backend)
{
    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Cannot initialize Portal::Graphics with null backend");
        return false;
    }

    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::Graphics already initialized");
        return true;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Initializing Portal::Graphics...");

    if (!TextureManager::instance().initialize(backend)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize TextureManager");
        return false;
    }

    g_initialized = true;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Graphics initialized successfully");
    return true;
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Shutting down Portal::Graphics...");

    TextureManager::instance().shutdown();
    g_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Graphics shutdown complete");
}

bool is_initialized()
{
    return g_initialized;
}

} // namespace MayaFlux::Portal::Graphics
