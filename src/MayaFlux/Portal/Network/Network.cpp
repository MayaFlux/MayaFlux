#include "Network.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Network {

namespace {
    bool g_initialized {};
}

bool initialize(Registry::Service::NetworkService* service)
{
    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
            "Portal::Network already initialized");
        return true;
    }

    if (!service) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Networking,
            "Cannot initialize Portal::Network: NetworkService is null");
        return false;
    }

    // NetworkFoundry::instance().initialize(service);
    // StreamForge::instance().initialize();
    // PacketFlow::instance().initialize();

    g_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::Networking,
        "Portal::Network initialized");
    return true;
}

void stop()
{
    if (!g_initialized) {
        return;
    }

    // PacketFlow::instance().stop();
    // StreamForge::instance().stop();
    // NetworkFoundry::instance().stop();

    MF_INFO(Journal::Component::Portal, Journal::Context::Networking,
        "Portal::Network stopped");
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    // PacketFlow::instance().shutdown();
    // StreamForge::instance().shutdown();
    // NetworkFoundry::instance().shutdown();

    g_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::Networking,
        "Portal::Network shutdown complete");
}

bool is_initialized()
{
    return g_initialized;
}

} // namespace MayaFlux::Portal::Network
