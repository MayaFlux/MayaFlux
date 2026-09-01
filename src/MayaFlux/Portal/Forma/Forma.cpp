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

    std::shared_ptr<Core::Window> g_inspect_nodes_window;
    std::shared_ptr<Core::Window> g_inspect_buffers_window;
    std::shared_ptr<Core::Window> g_inspect_scheduler_window;
    std::shared_ptr<Core::Window> g_inspect_events_window;

    constexpr uint32_t k_inspect_w = 480;
    constexpr uint32_t k_inspect_h = 900;

    constexpr size_t k_text_label_capacity = static_cast<size_t>(6) * sizeof(Kakshya::MeshVertex);
    constexpr size_t k_rect_capacity = static_cast<size_t>(4) * sizeof(Kakshya::Vertex);

    void place_plot_adornments(
        Surface& surface,
        const Plot::SeriesSpec& spec,
        uint32_t relate_to)
    {
        const auto& win = surface.window();
        auto& atelier = internal::atelier();

        for (const auto& label : spec.labels) {
            auto buf = atelier.create_buffer(
                win,
                k_text_label_capacity,
                Graphics::PrimitiveTopology::TRIANGLE_LIST,
                {},
                { { "text", nullptr } });
            (void)Plot::place_label(surface, std::move(buf), label, relate_to);
        }

        for (const auto& ticks : spec.tick_labels) {
            for (const auto& label : Plot::plot_tick_labels(ticks)) {
                auto buf = atelier.create_buffer(
                    win,
                    k_text_label_capacity,
                    Graphics::PrimitiveTopology::TRIANGLE_LIST,
                    {},
                    { { "text", nullptr } });
                (void)Plot::place_label(surface, std::move(buf), label, relate_to);
            }
        }

        if (spec.legend) {
            auto layout = Plot::layout_legend(*spec.legend);

            for (const auto& swatch : layout.swatches) {
                auto buf = atelier.create_buffer(
                    win,
                    k_rect_capacity,
                    Graphics::PrimitiveTopology::TRIANGLE_STRIP);
                (void)Plot::place_rect(surface, std::move(buf), swatch, relate_to);
            }

            for (const auto& label : layout.labels) {
                auto buf = atelier.create_buffer(
                    win,
                    k_text_label_capacity,
                    Graphics::PrimitiveTopology::TRIANGLE_LIST,
                    {},
                    { { "text", nullptr } });
                (void)Plot::place_label(surface, std::move(buf), label, relate_to);
            }
        }
    }

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
    internal::atelier().shutdown();

    g_inspect_nodes_window.reset();
    g_inspect_buffers_window.reset();
    g_inspect_scheduler_window.reset();
    g_inspect_events_window.reset();
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

        place_plot_adornments(surface, spec, mapped.element.id);

        return { std::move(mapped), std::move(surface) };
    }

    auto buf = atelier.create_buffer(window, spec.capacity_for(N), spec.topology);
    auto mapped = Plot::place(surface, std::move(buf), spec, std::move(container));

    place_plot_adornments(surface, spec, mapped.element.id);

    return { std::move(mapped), std::move(surface) };
}

// =============================================================================
// Bridge
// =============================================================================

void inspect_node_graph()
{
    if (g_inspect_nodes_window) {
        g_inspect_nodes_window->show();
        return;
    }

    auto& atelier = internal::atelier();

    g_inspect_nodes_window = atelier.create_window(
        Core::WindowCreateInfo { .title = "NodeGraphManager", .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(g_inspect_nodes_window, "NodeGraphManager");
    LayoutCursor cursor;
    auto& result = atelier.inspector().node_graph_manager(surface, cursor);
    atelier.bridge().spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_buffers()
{
    if (g_inspect_buffers_window) {
        g_inspect_buffers_window->show();
        return;
    }

    auto& atelier = internal::atelier();

    g_inspect_buffers_window = atelier.create_window(
        Core::WindowCreateInfo { .title = "BufferManager", .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(g_inspect_buffers_window, "BufferManager");
    LayoutCursor cursor;
    auto& result = atelier.inspector().buffer_manager(surface, cursor);
    atelier.bridge().spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_scheduler()
{
    if (g_inspect_scheduler_window) {
        g_inspect_scheduler_window->show();
        return;
    }

    auto& atelier = internal::atelier();

    g_inspect_scheduler_window = atelier.create_window(
        Core::WindowCreateInfo { .title = "TaskScheduler", .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(g_inspect_scheduler_window, "TaskScheduler");
    LayoutCursor cursor;
    auto& result = atelier.inspector().scheduler(surface, cursor);
    atelier.bridge().spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect_events()
{
    if (g_inspect_events_window) {
        g_inspect_events_window->show();
        return;
    }

    auto& atelier = internal::atelier();

    g_inspect_events_window = atelier.create_window(
        Core::WindowCreateInfo { .title = "EventManager", .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(g_inspect_events_window, "EventManager");
    LayoutCursor cursor;
    auto& result = atelier.inspector().event_manager(surface, cursor);
    atelier.bridge().spawn_sync(result.group.header.header_id, [&result] { result.tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Node>& node)
{
    auto& atelier = internal::atelier();
    const std::string title = Reflect::short_dynamic_type_name(node);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().node(node, surface, cursor));
    atelier.bridge().spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Buffers::Buffer>& buf)
{
    auto& atelier = internal::atelier();
    const std::string title = Reflect::short_dynamic_type_name(buf);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().buffer(buf, surface, cursor));
    atelier.bridge().spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Nodes::Network::NodeNetwork>& net)
{
    auto& atelier = internal::atelier();

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = "NodeNetwork", .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(window, "NodeNetwork");
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().node_network(net, surface, cursor));
    atelier.bridge().spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

void inspect(const std::shared_ptr<Vruta::Event>& ev, std::string_view name)
{
    auto& atelier = internal::atelier();
    const std::string title = name.empty() ? "Event" : "Event: " + std::string(name);

    auto window = atelier.create_window(
        Core::WindowCreateInfo { .title = title, .width = k_inspect_w, .height = k_inspect_h });

    auto surface = atelier.create_surface(window, title);
    LayoutCursor cursor;
    auto result = std::make_shared<InspectResult>(atelier.inspector().event(ev, name, surface, cursor));
    atelier.bridge().spawn_sync(result->group.header.header_id, [result] { result->tap_all(); });
}

} // namespace MayaFlux::Portal::Forma
