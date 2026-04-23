#include "Runtime.hpp"

#include "Lila/Commentator.hpp"
#include "Lila/Lila.hpp"

#include "MayaFlux/Transitive/Memory/Persist.hpp"

namespace MayaFlux::Host::Live {

namespace {
    std::mutex g_mutex;
    std::unique_ptr<Lila::Lila> g_instance;
    uint16_t g_port {};
}

bool start_lila(uint16_t port)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (g_instance) {
        LILA_WARN(Lila::Emitter::SYSTEM,
            "start_lila: already running on port " + std::to_string(g_port));
        return false;
    }

    auto instance = std::make_unique<Lila::Lila>();

    if (!instance->initialize(Lila::OperationMode::Both, static_cast<int>(port), true)) {
        LILA_ERROR(Lila::Emitter::SYSTEM,
            "start_lila: initialization failed: " + instance->get_last_error());
        return false;
    }

    g_instance = std::move(instance);
    g_port = port;

    LILA_INFO(Lila::Emitter::SYSTEM,
        "start_lila: running on port " + std::to_string(port));
    return true;
}

void stop_lila(bool clear_persistent_store)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (!g_instance) {
        return;
    }

    LILA_INFO(Lila::Emitter::SYSTEM,
        "stop_lila: stopping on port " + std::to_string(g_port));

    g_instance->stop_server();
    g_instance.reset();
    g_port = 0;

    if (clear_persistent_store) {
        internal::cleanup_persistent_store();
        LILA_DEBUG(Lila::Emitter::SYSTEM, "stop_lila: cleared persistent store");
    }
}

bool lila_active()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return static_cast<bool>(g_instance);
}

uint16_t lila_port()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_instance ? g_port : uint16_t {};
}

} // namespace MayaFlux::Host::Live
