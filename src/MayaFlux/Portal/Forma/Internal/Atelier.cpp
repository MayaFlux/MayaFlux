#include "Atelier.hpp"

#include "MayaFlux/Portal/Forma/Inspect/Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
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

Atelier& Atelier::instance()
{
    static Atelier a;
    return a;
}

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
    m_atlas_images.clear();

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
    if (!m_initialized) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(),
            "Portal::Forma not initialized - cannot create window");
    }

    auto window = m_window_manager->create_window(info);
    window->show();
    return window;
}

std::shared_ptr<Buffers::FormaBuffer> Atelier::create_buffer(Graphics::RenderConfig render)
{
    if (!m_initialized) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(),
            "Portal::Forma not initialized - cannot create buffer");
    }

    if (!render.target_window) {
        error<std::invalid_argument>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(),
            "Portal::Forma create_buffer requires a target window");
    }

    render.topology = Graphics::PrimitiveTopology::TRIANGLE_LIST;
    render.triangulate = true;

    auto buf = std::make_shared<Buffers::FormaBuffer>(k_capacity_bytes, render.topology);
    m_buffer_manager->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    const auto height = std::max(render.target_window->get_create_info().height, 1U);
    buf->setup_rendering(render);

    if (auto processor = buf->get_render_processor()) {
        Graphics::MillSpec spec = processor->get_mill_spec();
        spec.ribbon = Graphics::MillSpec::Ribbon::WorldPlane;
        spec.width_scale = 2.0F / static_cast<float>(height);
        processor->set_mill_spec(spec);
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

Surface Atelier::create_surface(SurfaceConfig config)
{
    if (!config.layer && config.ctx) {
        config.layer = config.ctx->layer();
    } else if (config.layer && !config.ctx) {
        config.ctx = std::make_shared<Context>(config.layer, config.window, *m_event_manager, config.name);
        MayaFlux::store(config.ctx);
    } else if (!config.layer) {
        auto [layer, ctx] = create_layer(config.window, config.name);
        config.layer = std::move(layer);
        config.ctx = std::move(ctx);
    }
    return Surface(std::move(config));
}

Surface Atelier::create_surface(const Core::WindowCreateInfo& window_info, std::string name)
{
    return create_surface(SurfaceConfig {
        .window = create_window(window_info),
        .name = std::move(name),
    });
}

void Atelier::place_adornments(
    Surface& surface,
    const Plot::SeriesSpec& spec,
    uint32_t relate_to,
    const std::shared_ptr<Buffers::FormaBuffer>& batch)
{
    const bool has_labels = !spec.labels.empty() || !spec.tick_labels.empty()
        || (spec.legend && !spec.legend->entries.empty());
    std::shared_ptr<Buffers::FormaBuffer> owned_batch;
    const auto target_of = [&](Graphics::PrimitiveTopology topology) -> FormaTarget {
        if (!batch && !owned_batch) {
            Graphics::RenderConfig render = surface.render_config();
            if (has_labels) {
                render.fragment_shader = "forma_plot.frag.spv";
                render.additional_textures.emplace_back("text", nullptr);
            }
            owned_batch = create_buffer(std::move(render));
        }
        const auto& target_batch = batch ? batch : owned_batch;
        return FormaTarget::slice_of(target_batch, topology);
    };

    const auto legend = spec.legend
        ? std::optional<Plot::LegendLayout>(Plot::layout_legend(*spec.legend))
        : std::nullopt;
    if (legend) {
        for (const auto& swatch : legend->swatches) {
            (void)Plot::place_rect(surface,
                target_of(Graphics::PrimitiveTopology::TRIANGLE_STRIP), swatch, relate_to);
        }
    }

    for (const auto& label : spec.labels) {
        (void)Plot::place_label(surface,
            target_of(Graphics::PrimitiveTopology::TRIANGLE_LIST), label, relate_to);
    }

    for (const auto& ticks : spec.tick_labels) {
        for (const auto& label : Plot::plot_tick_labels(ticks)) {
            (void)Plot::place_label(surface,
                target_of(Graphics::PrimitiveTopology::TRIANGLE_LIST), label, relate_to);
        }
    }

    if (legend) {
        for (const auto& label : legend->labels) {
            (void)Plot::place_label(surface,
                target_of(Graphics::PrimitiveTopology::TRIANGLE_LIST), label, relate_to);
        }
    }
}

} // namespace MayaFlux::Portal::Forma::internal
