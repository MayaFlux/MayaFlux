#include "Atelier.hpp"

#include "MayaFlux/Portal/Forma/Inspect/Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Portal::Forma::internal {

// =============================================================================
// Lifecycle
// =============================================================================

Atelier::Atelier() = default;
Atelier::~Atelier() = default;

bool Atelier::initialize(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager,
    std::shared_ptr<Core::WindowManager> window_manager)
{
    if (m_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::Forma already initialized");
        return true;
    }

    m_node_graph_manager = std::move(node_graph_manager);
    m_buffer_manager = std::move(buffer_manager);
    m_scheduler = std::move(scheduler);
    m_event_manager = std::move(event_manager);
    m_window_manager = std::move(window_manager);

    m_bridge = std::make_unique<Bridge>(*m_scheduler, *m_buffer_manager);
    m_inspect = std::make_unique<Inspector>(
        *m_node_graph_manager, *m_buffer_manager, *m_scheduler, *m_event_manager);

    m_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Forma initialized");
    return true;
}

void Atelier::shutdown()
{
    if (!m_initialized)
        return;

    m_bridge.reset();
    m_inspect.reset();

    m_node_graph_manager = nullptr;
    m_buffer_manager = nullptr;
    m_scheduler = nullptr;
    m_event_manager = nullptr;
    m_window_manager = nullptr;

    m_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Forma shutdown");
}

Bridge& Atelier::bridge()
{
    if (!m_initialized) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(),
            "Portal::Forma not initialized - cannot get bridge");
    }
    return *m_bridge;
}

Inspector& Atelier::inspector()
{
    if (!m_initialized) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(),
            "Portal::Forma not initialized - cannot get inspector");
    }
    return *m_inspect;
}

// =============================================================================
// Construction
// =============================================================================

std::shared_ptr<Core::Window> Atelier::create_window(const Core::WindowCreateInfo& info)
{
    auto window = m_window_manager->create_window(info);
    window->show();
    return window;
}

std::shared_ptr<Buffers::FormaBuffer> Atelier::create_buffer(
    std::shared_ptr<Core::Window> window,
    size_t capacity,
    Graphics::PrimitiveTopology topology,
    const std::string& texture_binding,
    std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures)
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);
    m_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    if (!additional_textures.empty()) {
        buf->setup_rendering({
            .target_window = std::move(window),
            .additional_textures = std::move(additional_textures),
        });
    } else if (!texture_binding.empty()) {
        buf->setup_rendering({
            .target_window = std::move(window),
            .default_texture_binding = texture_binding,
        });
    } else {
        buf->setup_rendering({ .target_window = std::move(window) });
    }

    return buf;
}

std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
Atelier::create_layer(const std::shared_ptr<Core::Window>& window, std::string name)
{
    auto layer = std::make_shared<Layer>();
    auto ctx = std::make_shared<Context>(layer, window, *m_event_manager, std::move(name));

    MayaFlux::store(ctx);

    return { std::move(layer), std::move(ctx) };
}

Surface Atelier::create_surface(std::shared_ptr<Core::Window> window, std::string name)
{
    auto [layer, ctx] = create_layer(window, std::move(name));
    return { std::move(window), std::move(layer), std::move(ctx) };
}

} // namespace MayaFlux::Portal::Forma::internal
