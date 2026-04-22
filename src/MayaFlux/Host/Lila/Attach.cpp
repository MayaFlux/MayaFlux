#ifdef MAYAFLUX_LILA_ENABLED

#include "Attach.hpp"

#include "Lila/Lila.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"

namespace MayaFlux::Host {

namespace {
    std::mutex g_mutex;
    std::unique_ptr<Lila::Lila> g_attached;
    uint16_t g_port {};
}

bool attach_lila(uint16_t port)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (g_attached) {
        MF_WARN(Journal::Component::Core, Journal::Context::API,
            "attach_lila: already attached on port {}", g_port);
        return false;
    }

    auto instance = std::make_unique<Lila::Lila>();

    if (!instance->initialize(Lila::OperationMode::Both, static_cast<int>(port), true)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::API,
            "attach_lila: Lila initialization failed: {}", instance->get_last_error());
        return false;
    }

    g_attached = std::move(instance);
    g_port = port;

    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "attach_lila: attached on port {}", port);
    return true;
}

void detach_lila(bool clear_persistent_store)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (!g_attached) {
        return;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "detach_lila: detaching from port {}", g_port);

    g_attached->stop_server();
    g_attached.reset();
    g_port = 0;

    if (clear_persistent_store) {
        internal::cleanup_persistent_store();
        MF_DEBUG(Journal::Component::Core, Journal::Context::API,
            "detach_lila: cleared persistent store");
    }
}

bool is_lila_attached()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return static_cast<bool>(g_attached);
}

uint16_t attached_lila_port()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_attached ? g_port : uint16_t {};
}

} // namespace MayaFlux::Host

#endif // MAYAFLUX_LILA_ENABLED
