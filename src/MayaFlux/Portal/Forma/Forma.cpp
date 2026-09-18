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

    std::optional<Surface> g_inspect_nodes_surface;
    std::optional<Surface> g_inspect_buffers_surface;
    std::optional<Surface> g_inspect_scheduler_surface;
    std::optional<Surface> g_inspect_events_surface;
    std::vector<std::unique_ptr<Surface>> g_inspect_surfaces;

    constexpr uint32_t k_inspect_w = 480;
    constexpr uint32_t k_inspect_h = 900;

} // namespace

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
    return internal::atelier().initialize(
        std::move(node_graph_manager), std::move(buffer_manager),
        std::move(scheduler), std::move(event_manager), std::move(window_manager));
}

void shutdown()
{
    g_inspect_nodes_surface.reset();
    g_inspect_buffers_surface.reset();
    g_inspect_scheduler_surface.reset();
    g_inspect_events_surface.reset();
    g_inspect_surfaces.clear();

    internal::atelier().shutdown();
}

bool is_initialized() { return internal::atelier().is_initialized(); }

Bridge& bridge() { return internal::atelier().bridge(); }

Inspector& inspector() { return internal::atelier().inspector(); }

// =============================================================================
// Layer
// =============================================================================

std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
create_layer(const std::shared_ptr<Core::Window>& window, std::string name)
{
    return internal::atelier().create_layer(window, std::move(name));
}

Surface create_surface(std::shared_ptr<Core::Window> window, std::string name)
{
    return internal::atelier().create_surface(std::move(window), std::move(name));
}

void destroy(Surface& surface, uint32_t id)
{
    for (uint32_t cid : surface.layer().closure(id)) {
        surface.ctx().unbind(cid);
        bridge().unbind(cid);
    }
    surface.layer().remove(id);
}

// =============================================================================
// Standalone buffer
// =============================================================================

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    Graphics::PrimitiveTopology topology,
    const std::string& texture_binding)
{
    return internal::atelier().create_buffer(
        std::move(window), internal::k_capacity_bytes, topology, texture_binding);
}

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    Graphics::PrimitiveTopology topology,
    std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures)
{
    return internal::atelier().create_buffer(
        std::move(window), internal::k_capacity_bytes, topology, {},
        std::move(additional_textures));
}

// =============================================================================
// Text field
// =============================================================================

namespace {

    std::shared_ptr<Buffers::FormaBuffer> make_text_buffer(Surface& surface)
    {
        return create_buffer(
            surface.window(),
            Graphics::PrimitiveTopology::TRIANGLE_LIST,
            std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> {
                { "text", nullptr } });
    }

} // namespace

TextField create_text_field(
    Surface& surface,
    Kinesis::AABB2D bounds,
    std::shared_ptr<Portal::Text::PressParams> params,
    std::string initial_text,
    bool scrollable)
{
    if (!scrollable) {
        return TextField {}.place(
            make_text_buffer(surface), surface, bounds, std::move(params), std::move(initial_text));
    }

    auto viewport_buf = create_buffer(surface.window(), Graphics::PrimitiveTopology::TRIANGLE_STRIP);
    auto viewport = Scrollable {}.place(std::move(viewport_buf), surface, bounds);

    return TextField {}.scrollable(
        make_text_buffer(surface), surface, viewport, std::move(params), std::move(initial_text));
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

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = std::move(title), .width = width, .height = height });

    auto surface = atelier.create_surface(window, window->get_create_info().title);

    if (spec.background_fn) {
        auto bg = atelier.create_element<float>(
            surface.layer(), window,
            *spec.background_fn,
            0.F,
            Graphics::PrimitiveTopology::TRIANGLE_STRIP,
            static_cast<size_t>(4) * Kakshya::VertexLayout::for_meshes().stride_bytes);

        const auto bg_id = bg.element.id;
        auto buf = atelier.create_buffer(window, spec.capacity_for(N), spec.topology);
        auto mapped = Plot::place(surface, std::move(buf), spec, std::move(container));
        surface.layer().relate(mapped.element.id, bg_id);
        surface.layer().send_to_back(bg_id);

        internal::atelier().place_adornments(surface, spec, mapped.element.id);

        return { std::move(mapped), std::move(surface) };
    }

    auto buf = atelier.create_buffer(window, spec.capacity_for(N), spec.topology);
    auto mapped = Plot::place(surface, std::move(buf), spec, std::move(container));

    internal::atelier().place_adornments(surface, spec, mapped.element.id);

    return { std::move(mapped), std::move(surface) };
}

// =============================================================================
// Bridge
// =============================================================================

void inspect_node_graph()
{
    if (g_inspect_nodes_surface && g_inspect_nodes_surface->window()) {
        g_inspect_nodes_surface->window()->show();
        return;
    }

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "NodeGraphManager", .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, "NodeGraphManager");
    g_inspect_nodes_surface.emplace(window, std::move(layer), std::move(ctx));
    LayoutCursor cursor;
    auto& result = atelier.inspector().node_graph_manager(*g_inspect_nodes_surface, cursor);
    atelier.bridge().spawn_sync(g_inspect_nodes_surface->layer(),
        result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_buffers()
{
    if (g_inspect_buffers_surface && g_inspect_buffers_surface->window()) {
        g_inspect_buffers_surface->window()->show();
        return;
    }

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "BufferManager", .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, "BufferManager");
    g_inspect_buffers_surface.emplace(window, std::move(layer), std::move(ctx));
    LayoutCursor cursor;
    auto& result = atelier.inspector().buffer_manager(*g_inspect_buffers_surface, cursor);
    atelier.bridge().spawn_sync(g_inspect_buffers_surface->layer(),
        result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_scheduler()
{
    if (g_inspect_scheduler_surface && g_inspect_scheduler_surface->window()) {
        g_inspect_scheduler_surface->window()->show();
        return;
    }

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "TaskScheduler", .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, "TaskScheduler");
    g_inspect_scheduler_surface.emplace(window, std::move(layer), std::move(ctx));
    LayoutCursor cursor;
    auto& result = atelier.inspector().scheduler(*g_inspect_scheduler_surface, cursor);
    atelier.bridge().spawn_sync(g_inspect_scheduler_surface->layer(),
        result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_events()
{
    if (g_inspect_events_surface && g_inspect_events_surface->window()) {
        g_inspect_events_surface->window()->show();
        return;
    }

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "EventManager", .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, "EventManager");
    g_inspect_events_surface.emplace(window, std::move(layer), std::move(ctx));
    LayoutCursor cursor;
    auto& result = atelier.inspector().event_manager(*g_inspect_events_surface, cursor);
    atelier.bridge().spawn_sync(g_inspect_events_surface->layer(),
        result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Node>& node)
{
    auto& atelier = internal::atelier();
    const std::string title = Reflect::short_dynamic_type_name(node);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, title);
    g_inspect_surfaces.push_back(std::make_unique<Surface>(
        window, std::move(layer), std::move(ctx)));
    auto& surface = *g_inspect_surfaces.back();
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().node(node, surface, cursor));
    atelier.bridge().spawn_sync(surface.layer(),
        result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Buffers::Buffer>& buf)
{
    auto& atelier = internal::atelier();
    const std::string title = Reflect::short_dynamic_type_name(buf);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, title);
    g_inspect_surfaces.push_back(std::make_unique<Surface>(
        window, std::move(layer), std::move(ctx)));
    auto& surface = *g_inspect_surfaces.back();
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().buffer(buf, surface, cursor));
    atelier.bridge().spawn_sync(surface.layer(),
        result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Network::NodeNetwork>& net)
{
    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "NodeNetwork", .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, "NodeNetwork");
    g_inspect_surfaces.push_back(std::make_unique<Surface>(
        window, std::move(layer), std::move(ctx)));
    auto& surface = *g_inspect_surfaces.back();
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().node_network(net, surface, cursor));
    atelier.bridge().spawn_sync(surface.layer(),
        result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Vruta::Event>& ev, std::string_view name)
{
    auto& atelier = internal::atelier();
    const std::string title = name.empty() ? "Event" : "Event: " + std::string(name);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto [layer, ctx] = atelier.create_layer(window, title);
    g_inspect_surfaces.push_back(std::make_unique<Surface>(
        window, std::move(layer), std::move(ctx)));
    auto& surface = *g_inspect_surfaces.back();
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().event(ev, name, surface, cursor));
    atelier.bridge().spawn_sync(surface.layer(),
        result->group.header.header_id, [result] { result->tap_all(); });
}

} // namespace MayaFlux::Portal::Forma
