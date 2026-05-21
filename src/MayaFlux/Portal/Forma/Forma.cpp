#include "Forma.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "Inspect/Inspector.hpp"

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Forma {

namespace {
    bool g_initialized {};
    std::shared_ptr<Nodes::NodeGraphManager> g_node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> g_buffer_manager;
    std::shared_ptr<Vruta::TaskScheduler> g_scheduler;
    std::shared_ptr<Vruta::EventManager> g_event_manager;
    std::shared_ptr<Core::WindowManager> g_window_manager;
    std::unique_ptr<Bridge> g_bridge;
    std::unique_ptr<Inspector> g_inspect;
}

// =============================================================================
// Lifecycle
// =============================================================================

bool initialize(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager,
    std::shared_ptr<Core::WindowManager> window_manager)
{
    if (g_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Portal::Forma already initialized");
        return true;
    }

    g_node_graph_manager = std::move(node_graph_manager);
    g_buffer_manager = std::move(buffer_manager);
    g_scheduler = std::move(scheduler);
    g_event_manager = std::move(event_manager);
    g_window_manager = std::move(window_manager);
    g_bridge = std::make_unique<Bridge>(*g_scheduler, *g_buffer_manager);
    g_inspect = std::make_unique<Inspector>(*g_node_graph_manager, *g_buffer_manager, *g_scheduler, *g_event_manager);
    g_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "Portal::Forma initialized");
    return true;
}

Inspector& inspector()
{
    if (!g_initialized) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::API, std::source_location::current(),
            "Portal::Forma not initialized - cannot get inspector");
    }
    return *g_inspect;
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    g_bridge.reset();
    g_inspect.reset();
    g_node_graph_manager = nullptr;
    g_buffer_manager = nullptr;
    g_scheduler = nullptr;
    g_event_manager = nullptr;
    g_window_manager = nullptr;
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
    Graphics::PrimitiveTopology topology,
    const std::string& texture_binding)
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);

    g_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    if (!texture_binding.empty()) {
        buf->setup_rendering({ .target_window = std::move(window), .default_texture_binding = texture_binding });
    } else {
        buf->setup_rendering({ .target_window = std::move(window) });
    }

    return buf;
}

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    size_t capacity,
    Graphics::PrimitiveTopology topology,
    std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures)
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);
    g_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    buf->setup_rendering({
        .target_window = std::move(window),
        .additional_textures = std::move(additional_textures),
    });
    return buf;
}

Surface create_surface(
    std::shared_ptr<Core::Window> window,
    std::string name)
{
    auto [layer, ctx] = create_layer(window, std::move(name));
    return { std::move(window), std::move(layer), std::move(ctx) };
}

// =============================================================================
// Plot
// =============================================================================

std::pair<Mapped<std::shared_ptr<Kakshya::PlotContainer>>, Surface>
plot(
    std::string title,
    uint32_t width,
    uint32_t height,
    std::shared_ptr<Kakshya::PlotContainer> container,
    Plot::SeriesSpec spec)
{
    const uint64_t N = container->series_count() > 0
        ? container->series_size(0)
        : 0;

    auto window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = std::move(title), .width = width, .height = height });
    window->show();

    auto surface = create_surface(window, window->get_create_info().title);

    if (spec.background_fn) {
        auto bg = create_element<float>(
            surface,
            *spec.background_fn,
            0.F,
            static_cast<size_t>(4) * Kakshya::VertexLayout::for_meshes().stride_bytes,
            Graphics::PrimitiveTopology::TRIANGLE_STRIP);
        auto bg_id = bg.element.id;
        auto buf = create_buffer(window, spec.capacity_for(N), spec.topology);
        auto mapped = Plot::place(surface, std::move(buf), std::move(spec), std::move(container));
        surface.layer().relate(mapped.element.id, bg_id);
        surface.layer().send_to_back(bg_id);
        return { std::move(mapped), std::move(surface) };
    }

    auto buf = create_buffer(window, spec.capacity_for(N), spec.topology);

    return { Plot::place(surface, std::move(buf), std::move(spec), std::move(container)), std::move(surface) };
}

// =============================================================================
// Bridge
// =============================================================================

Bridge& bridge()
{
    return *g_bridge;
}

} // namespace MayaFlux::Portal::Forma
