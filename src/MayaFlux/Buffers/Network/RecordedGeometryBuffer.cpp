#include "RecordedGeometryBuffer.hpp"

#include "GeometryRenderSetup.hpp"
#include "RecordedGeometryProcessor.hpp"
#include "SpatialFieldWiring.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

RecordedGeometryBuffer::RecordedGeometryBuffer(
    uint32_t capacity,
    Portal::Graphics::PrimitiveTopology topology,
    Kinesis::SamplerBounds bounds,
    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op,
    Kakshya::VertexAccessConfig access)
    : StateFieldBuffer(
          static_cast<size_t>(std::max(capacity, 1U)) * sizeof(Kakshya::Vertex),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_capacity(capacity)
    , m_topology(topology)
    , m_bounds(bounds)
    , m_field_op(std::move(field_op))
    , m_access(access)
{
    if (m_capacity == 0) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create RecordedGeometryBuffer with zero capacity");
    }

    if (m_topology != Portal::Graphics::PrimitiveTopology::POINT_LIST
        && m_topology != Portal::Graphics::PrimitiveTopology::LINE_STRIP
        && m_topology != Portal::Graphics::PrimitiveTopology::LINE_LIST) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "RecordedGeometryBuffer draws points or lines, not triangles");
    }
}

void RecordedGeometryBuffer::setup_processors(ProcessingToken token)
{
    m_processor = std::make_shared<RecordedGeometryProcessor>(m_access, m_field_op);
    m_processor->set_processing_token(token);
    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    wire_field();
}

void RecordedGeometryBuffer::wire_field()
{
    if (!m_field_op) {
        return;
    }

    const auto& layout = m_field_op->get_layout();
    if (layout.stride_bytes != sizeof(Kakshya::Vertex)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "RecordedGeometryBuffer: the field operator's layout is {} bytes per vertex, "
            "the buffer's records are {}; no stage is wired",
            layout.stride_bytes, sizeof(Kakshya::Vertex));
        return;
    }

    const auto& config = m_field_op->get_field_config();

    std::optional<SpatialHashConfig> hash;
    if (wants_spatial_stages(config)) {
        if (!config.cell_size.has_value()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "RecordedGeometryBuffer: SpatialFieldConfig has no cell_size, and nothing "
                "here can derive one");
        } else {
            hash = SpatialHashConfig::from_bounds(m_bounds, layout, m_capacity, *config.cell_size);
        }
    }

    wire_field_stages(
        std::dynamic_pointer_cast<StateFieldBuffer>(shared_from_this()),
        m_field_op, hash, {}, nullptr, true);
}

void RecordedGeometryBuffer::setup_rendering(const RenderConfig& config)
{
    const std::array run_topologies { m_topology };
    const auto resolved_config = resolve_geometry_render_config(config, run_topologies);

    apply_render_config(resolved_config, ShaderConfig { resolved_config.vertex_shader });
    tune_point_mill(m_render_processor, resolved_config);

    get_processing_chain()->add_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
    m_applied_runs.clear();
    m_applied_count.reset();
    apply_draw();
}

void RecordedGeometryBuffer::set_placement(const glm::mat4& placement)
{
    if (!m_placement) {
        m_placement = std::make_shared<Memory::SnapshotCell<glm::mat4>>(placement);
    } else {
        bool changed = true;
        if (const auto held = m_placement->read()) {
            changed = *held != placement;
        }
        if (changed) {
            m_placement->publish(placement);
        }
    }

    auto renderers = get_additional_render_processors();
    if (auto primary = get_render_processor()) {
        renderers.push_back(std::move(primary));
    }

    for (const auto& renderer : renderers) {
        const bool known = std::ranges::any_of(m_placed,
            [&renderer](const auto& placed) { return placed.lock() == renderer; });
        if (known) {
            continue;
        }

        std::function<glm::mat4()> outer = renderer->get_geometry_transform_source();
        if (!outer) {
            if (const auto& fixed = renderer->get_geometry_transform()) {
                outer = [matrix = *fixed] { return matrix; };
            }
        }
        renderer->set_geometry_transform_source([cell = m_placement, outer = std::move(outer)] {
            glm::mat4 own { 1.0F };
            if (const auto held = cell->read()) {
                own = *held;
            }
            return outer ? outer() * own : own;
        });
        m_placed.emplace_back(renderer);
    }
}

void RecordedGeometryBuffer::write_frame(std::shared_ptr<const Kakshya::SpatialData> frame)
{
    m_pending.publish(std::move(frame));
}

std::shared_ptr<const Kakshya::SpatialData> RecordedGeometryBuffer::current_frame() const
{
    if (auto held = m_pending.read()) {
        return *held;
    }
    return nullptr;
}

void RecordedGeometryBuffer::update_draw(
    uint32_t vertex_count,
    std::vector<Portal::Graphics::DrawRun> runs)
{
    m_draw_count = vertex_count;
    m_draw_runs = std::move(runs);

    if (m_layout_count != vertex_count) {
        auto layout = m_topology == Portal::Graphics::PrimitiveTopology::POINT_LIST
            ? Kakshya::VertexLayout::for_points()
            : Kakshya::VertexLayout::for_lines();
        layout.vertex_count = vertex_count;
        set_vertex_layout(layout);
        m_layout_count = vertex_count;
    }

    apply_draw();
}

void RecordedGeometryBuffer::apply_draw()
{
    if (!m_render_processor) {
        return;
    }

    if (m_applied_count != m_draw_count) {
        m_render_processor->set_vertex_range(0, m_draw_count);
        m_render_processor->set_visible(m_draw_count > 0, std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
        m_applied_count = m_draw_count;
    }

    apply_draw_runs(m_render_processor, get_render_config(), m_applied_runs, m_draw_runs);
}

} // namespace MayaFlux::Buffers
