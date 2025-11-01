#include "Graphics.hpp"

#include "ComputePress.hpp"
#include "RenderFlow.hpp"
#include "SamplerForge.hpp"
#include "TextureLoom.hpp"

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

    if (!TextureLoom::instance().initialize(backend)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize TextureLoom");
        return false;
    }

    if (!SamplerForge::instance().initialize(backend)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize SamplerFactory");
        return false;
    }

    if (!ShaderFoundry::instance().initialize(backend)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize ShaderFoundry");
        return false;
    }

    if (!ComputePress::instance().initialize()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize ComputePress");
        return false;
    }

    if (!RenderFlow::instance().initialize()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Failed to initialize RenderFlow");
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

    TextureLoom::instance().shutdown();
    SamplerForge::instance().shutdown();

    g_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Graphics shutdown complete");
}

bool is_initialized()
{
    return g_initialized;
}

} // namespace MayaFlux::Portal::Graphics
