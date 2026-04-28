#include "Forma.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Forma {

namespace {
    bool g_initialized {};
    std::shared_ptr<Buffers::BufferManager> g_buffer_manager;
    std::shared_ptr<Vruta::TaskScheduler> g_scheduler;
    std::shared_ptr<Vruta::EventManager> g_event_manager;
}

// =============================================================================
// Lifecycle
// =============================================================================

bool initialize(
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager)
{
    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::Forma already initialized");
        return true;
    }

    g_buffer_manager = std::move(buffer_manager);
    g_scheduler = std::move(scheduler);
    g_event_manager = std::move(event_manager);
    g_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Forma initialized");
    return true;
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    g_buffer_manager = nullptr;
    g_scheduler = nullptr;
    g_event_manager = nullptr;
    g_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Forma shutdown");
}

bool is_initialized()
{
    return g_initialized;
}

// =============================================================================
// Layer
// =============================================================================

std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
create_layer(
    const std::shared_ptr<Core::Window>& window,
    std::string name)
{
    auto layer = std::make_shared<Layer>();
    auto ctx = std::make_shared<Context>(layer, window, *g_event_manager, std::move(name));

    MayaFlux::store(ctx);

    return { std::move(layer), std::move(ctx) };
}

// =============================================================================
// Standalone buffer
// =============================================================================

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    size_t capacity,
    Graphics::PrimitiveTopology topology)
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);

    g_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    buf->setup_rendering({ .target_window = std::move(window) });

    return buf;
}

// =============================================================================
// Bridge
// =============================================================================

Bridge create_bridge()
{
    return { *g_scheduler, *g_buffer_manager };
}

} // namespace MayaFlux::Portal::Forma
