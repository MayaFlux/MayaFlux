#include "System.hpp"

#include "MayaFlux/Core/Backends/System/SystemBackend.hpp"

#ifdef MAYAFLUX_PLATFORM_LINUX
#include "MayaFlux/Core/Backends/System/DBusBackend.hpp"
#elif defined(MAYAFLUX_PLATFORM_WINDOWS)
#include "MayaFlux/Core/Backends/System/COMBackend.hpp"
#elif defined(MAYAFLUX_PLATFORM_MACOS)
#include "MayaFlux/Core/Backends/System/NSBackend.hpp"
#endif

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::System {

namespace {
    bool g_initialized {};
    std::unique_ptr<Core::SystemBackend> g_backend;
}

Core::SystemBackend* get_backend()
{
    return g_backend.get();
}

bool initialize()
{
    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::System already initialized");
        return true;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Initializing Portal::System...");

#ifdef MAYAFLUX_PLATFORM_LINUX
    g_backend = std::make_unique<Core::DBusBackend>();
#elif defined(MAYAFLUX_PLATFORM_WINDOWS)
    g_backend = std::make_unique<Core::COMBackend>();
#elif defined(MAYAFLUX_PLATFORM_MACOS)
    g_backend = std::make_unique<Core::NSBackend>();
#endif

    if (!g_backend || !g_backend->initialize()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Portal::System: backend initialization failed");
        g_backend.reset();
        return false;
    }

    g_initialized = true;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::System initialized");
    return true;
}

void stop()
{
    if (!g_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::System stopped");
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    if (g_backend) {
        g_backend->shutdown();
        g_backend.reset();
    }

    g_initialized = false;
    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::System shutdown complete");
}

bool is_initialized()
{
    return g_initialized;
}

} // namespace MayaFlux::Portal::System
