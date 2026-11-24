#include "NetworkGeometryBuffer.hpp"

#include "GeometryRenderSetup.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

NetworkGeometryBuffer::NetworkGeometryBuffer(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    const std::string& binding_name,
    float over_allocate_factor)
    : StateFieldBuffer(
          calculate_buffer_size(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(std::move(network))
    , m_binding_name(binding_name)
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create NetworkGeometryBuffer with null NodeNetwork");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created NetworkGeometryBuffer '{}' for {} nodes ({} bytes estimated)",
        m_binding_name,
        m_network->get_node_count(),
        get_size_bytes());
}

void NetworkGeometryBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<NetworkGeometryBuffer>(shared_from_this());

    m_processor = std::make_shared<NetworkGeometryProcessor>();
    m_processor->set_processing_token(token);
    m_processor->bind_network(
        m_binding_name,
        m_network,
        self);

    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Setup NetworkGeometryProcessor for '{}' with token {}",
        m_binding_name, static_cast<int>(token));

    wire_field_operators();
}

void NetworkGeometryBuffer::setup_rendering(const RenderConfig& config)
{
    using Portal::Graphics::PrimitiveTopology;

    std::vector<Portal::Graphics::DrawRun> draw_runs;
    std::vector<PrimitiveTopology> run_topologies;
    if (auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(
            m_network->get_operator())) {
        draw_runs = graphics_op->topology_runs();
        for (const auto& run : draw_runs)
            run_topologies.push_back(run.topology);
    }

    const auto resolved_config = resolve_geometry_render_config(config, run_topologies);

    apply_render_config(resolved_config, ShaderConfig { resolved_config.vertex_shader });
    tune_point_mill(m_render_processor, resolved_config);

    get_processing_chain()->add_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
    m_applied_runs.clear();
    update_render_runs(std::move(draw_runs));
}

uint32_t NetworkGeometryBuffer::get_vertex_count() const
{
    if (!m_network) {
        return 0;
    }

    auto* operator_ptr = m_network->get_operator();
    if (operator_ptr) {
        auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(operator_ptr);
        if (graphics_op) {
            return static_cast<uint32_t>(graphics_op->get_vertex_count());
        }
    }

    return static_cast<uint32_t>(m_network->get_node_count());
}

size_t NetworkGeometryBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    float over_allocate_factor)
{
    if (!network) {
        return 0;
    }

    size_t node_count = network->get_node_count();
    if (node_count == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NodeNetwork has zero nodes. Buffer will be created with minimum size.");
        return 4096;
    }

    size_t base_size = 0;

    if (auto* operator_ptr = network->get_operator()) {
        if (auto graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(operator_ptr)) {
            size_t vertex_count = graphics_op->get_vertex_count();
            auto layout = graphics_op->get_vertex_layout();

            if (vertex_count > 0 && layout.stride_bytes > 0) {
                base_size = vertex_count * layout.stride_bytes;

                MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
                    "Network geometry buffer sizing: {} vertices × {} bytes = {} bytes (operator: {})",
                    vertex_count, layout.stride_bytes, base_size, operator_ptr->get_type_name());
            }
        }
    }

    if (base_size == 0) {
        size_t vertex_size = sizeof(Kakshya::PointVertex);
        base_size = node_count * vertex_size;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Network geometry buffer fallback sizing: {} nodes × {} bytes = {} bytes",
            node_count, vertex_size, base_size);
    }

    auto allocated_size = static_cast<size_t>(
        static_cast<float>(base_size) * over_allocate_factor);

    if (over_allocate_factor > 1.0F) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Over-allocated by {}x: {} → {} bytes",
            over_allocate_factor, base_size, allocated_size);
    }

    return allocated_size;
}

void NetworkGeometryBuffer::add_chain_operator_rendering(const RenderConfig& config)
{
    const auto resolved_config = resolve_geometry_render_config(config, {});
    auto self = std::dynamic_pointer_cast<VKBuffer>(shared_from_this());

    std::shared_ptr<RenderProcessor> render;
    apply_render_config(render, resolved_config, ShaderConfig { resolved_config.vertex_shader });

    get_processing_chain()->add_processor(render, shared_from_this());
    render->set_buffer_vertex_layout(self, Kakshya::VertexLayout::for_lines());
    render->set_vertex_range(0, 0);

    m_chain_render_processors.push_back({ .render_processor = render });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Added chain render processor #{} to NetworkGeometryBuffer",
        m_chain_render_processors.size());
}

std::shared_ptr<RenderProcessor> NetworkGeometryBuffer::get_chain_render_processor(size_t index) const
{
    if (index >= m_chain_render_processors.size())
        return nullptr;
    return m_chain_render_processors[index].render_processor;
}

void NetworkGeometryBuffer::update_chain_render_range(
    size_t index,
    uint32_t vertex_offset,
    uint32_t vertex_count,
    const std::optional<Kakshya::VertexLayout>& layout,
    std::vector<Portal::Graphics::DrawRun> runs)
{
    if (index == 0) {
        if (m_render_processor) {
            m_render_processor->set_vertex_range(vertex_offset, vertex_count);
            update_render_runs(std::move(runs));
        }
        return;
    }

    const size_t ci = index - 1;
    if (ci >= m_chain_render_processors.size())
        return;

    auto& entry = m_chain_render_processors[ci];
    entry.vertex_offset = vertex_offset;
    entry.vertex_count = vertex_count;
    entry.render_processor->set_vertex_range(vertex_offset, vertex_count);
    entry.render_processor->set_runs(std::move(runs));

    if (layout) {
        auto self = std::dynamic_pointer_cast<VKBuffer>(shared_from_this());
        entry.render_processor->set_buffer_vertex_layout(self, *layout);
    }
}

void NetworkGeometryBuffer::update_render_runs(std::vector<Portal::Graphics::DrawRun> runs)
{
    apply_draw_runs(m_render_processor, get_render_config(), m_applied_runs, std::move(runs));
}

//-----------------------------------------------------------------------------
// Auxiliary state
//-----------------------------------------------------------------------------

bool NetworkGeometryBuffer::ensure_cluster_ids()
{
    if (has_state("hash_cluster_id")) {
        return true;
    }

    auto* graphics_op = m_network
        ? dynamic_cast<Nodes::Network::GraphicsOperator*>(m_network->get_operator())
        : nullptr;
    if (!graphics_op) {
        return false;
    }

    const size_t vertex_count = graphics_op->get_vertex_count();
    if (vertex_count == 0) {
        return false;
    }

    if (!declare_state("hash_cluster_id", vertex_count, sizeof(uint32_t), false)) {
        return false;
    }

    auto cluster_ids = graphics_op->build_cluster_ids();
    if (cluster_ids.size() != vertex_count) {
        cluster_ids.assign(vertex_count, 0U);
    }

    std::shared_ptr<VKBuffer> staging;
    upload_back_buffer(
        write_state_slot("hash_cluster_id"),
        cluster_ids.data(),
        cluster_ids.size() * sizeof(uint32_t),
        staging);

    return true;
}

} // namespace MayaFlux::Buffers
