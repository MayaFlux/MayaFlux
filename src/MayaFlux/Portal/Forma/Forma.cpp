#include "Forma.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

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

    std::shared_ptr<Core::Window> g_inspect_nodes_window;
    std::shared_ptr<Core::Window> g_inspect_buffers_window;
    std::shared_ptr<Core::Window> g_inspect_scheduler_window;
    std::shared_ptr<Core::Window> g_inspect_events_window;

    constexpr uint32_t k_inspect_w = 480;
    constexpr uint32_t k_inspect_h = 900;

} // namespace

namespace internal {

    std::shared_ptr<Buffers::FormaBuffer> create_buffer_impl(
        std::shared_ptr<Core::Window> window,
        size_t capacity,
        Graphics::PrimitiveTopology topology,
        const std::string& texture_binding,
        std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures)
    {
        auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);
        g_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

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
} // namespace internal

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

    g_inspect_nodes_window.reset();
    g_inspect_buffers_window.reset();
    g_inspect_scheduler_window.reset();
    g_inspect_events_window.reset();

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
    Graphics::PrimitiveTopology topology,
    const std::string& texture_binding)
{
    return internal::create_buffer_impl(std::move(window), internal::k_capacity_bytes, topology, texture_binding);
}

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    Graphics::PrimitiveTopology topology,
    std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures)
{
    return internal::create_buffer_impl(std::move(window), internal::k_capacity_bytes, topology, {}, std::move(additional_textures));
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
            surface.layer(), window,
            *spec.background_fn,
            0.F,
            Graphics::PrimitiveTopology::TRIANGLE_STRIP,
            static_cast<size_t>(4) * Kakshya::VertexLayout::for_meshes().stride_bytes);

        auto bg_id = bg.element.id;
        auto buf = internal::create_buffer_impl(window, spec.capacity_for(N), spec.topology);
        auto mapped = Plot::place(surface, std::move(buf), std::move(spec), std::move(container));
        surface.layer().relate(mapped.element.id, bg_id);
        surface.layer().send_to_back(bg_id);
        return { std::move(mapped), std::move(surface) };
    }

    auto buf = internal::create_buffer_impl(window, spec.capacity_for(N), spec.topology);

    return { Plot::place(surface, std::move(buf), std::move(spec), std::move(container)), std::move(surface) };
}

// =============================================================================
// Bridge
// =============================================================================

Bridge& bridge()
{
    return *g_bridge;
}

void inspect_node_graph()
{
    if (g_inspect_nodes_window) {
        g_inspect_nodes_window->show();
        return;
    }
    g_inspect_nodes_window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = "NodeGraphManager", .width = k_inspect_w, .height = k_inspect_h });
    g_inspect_nodes_window->show();
    auto surface = create_surface(g_inspect_nodes_window, "NodeGraphManager");
    LayoutCursor cursor;
    auto& result = g_inspect->node_graph_manager(surface, cursor);
    g_bridge->spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_buffers()
{
    if (g_inspect_buffers_window) {
        g_inspect_buffers_window->show();
        return;
    }
    g_inspect_buffers_window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = "BufferManager", .width = k_inspect_w, .height = k_inspect_h });
    g_inspect_buffers_window->show();
    auto surface = create_surface(g_inspect_buffers_window, "BufferManager");
    LayoutCursor cursor;
    auto& result = g_inspect->buffer_manager(surface, cursor);
    g_bridge->spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_scheduler()
{
    if (g_inspect_scheduler_window) {
        g_inspect_scheduler_window->show();
        return;
    }
    g_inspect_scheduler_window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = "TaskScheduler", .width = k_inspect_w, .height = k_inspect_h });
    g_inspect_scheduler_window->show();
    auto surface = create_surface(g_inspect_scheduler_window, "TaskScheduler");
    LayoutCursor cursor;
    auto& result = g_inspect->scheduler(surface, cursor);
    g_bridge->spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_events()
{
    if (g_inspect_events_window) {
        g_inspect_events_window->show();
        return;
    }
    g_inspect_events_window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = "EventManager", .width = k_inspect_w, .height = k_inspect_h });
    g_inspect_events_window->show();
    auto surface = create_surface(g_inspect_events_window, "EventManager");
    LayoutCursor cursor;
    auto& result = g_inspect->event_manager(surface, cursor);
    g_bridge->spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Node>& node)
{
    const std::string title = Reflect::short_dynamic_type_name(node);
    auto window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });
    window->show();
    auto surface = create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(g_inspect->node(node, surface, cursor));
    g_bridge->spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Buffers::Buffer>& buf)
{
    const std::string title = Reflect::short_dynamic_type_name(buf);
    auto window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });
    window->show();
    auto surface = create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(g_inspect->buffer(buf, surface, cursor));
    g_bridge->spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Network::NodeNetwork>& net)
{
    auto window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = "NodeNetwork", .width = k_inspect_w, .height = k_inspect_h });
    window->show();
    auto surface = create_surface(window, "NodeNetwork");
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(g_inspect->node_network(net, surface, cursor));
    g_bridge->spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Vruta::Event>& ev, std::string_view name)
{
    const std::string title = name.empty() ? "Event" : "Event: " + std::string(name);
    auto window = g_window_manager->create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });
    window->show();
    auto surface = create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(g_inspect->event(ev, name, surface, cursor));
    g_bridge->spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

} // namespace MayaFlux::Portal::Forma
