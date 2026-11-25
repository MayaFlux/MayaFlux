#include "AssembledGeometryBuffer.hpp"

#include "GeometryRenderSetup.hpp"
#include "SpatialFieldWiring.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Nodes/Network/AssemblyNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/OperatorChain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

AssembledGeometryBuffer::AssembledGeometryBuffer(
    const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network,
    float over_allocate_factor)
    : StateFieldBuffer(
          estimate_vertex_bytes(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(network)
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "AssembledGeometryBuffer: null AssemblyNetwork");
    }
}

void AssembledGeometryBuffer::setup_processors(ProcessingToken token)
{
    m_processor = std::make_shared<AssembledGeometryProcessor>(m_network);
    m_processor->set_processing_token(token);
    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    wire_field_pass();
}

void AssembledGeometryBuffer::setup_rendering(const RenderConfig& config)
{
    std::vector<Portal::Graphics::PrimitiveTopology> topologies;
    for (const auto& item : m_network->assembly()->get_items()) {
        topologies.push_back(item->get_primitive_topology());
    }

    auto requested = config;
    requested.triangulate = true;
    const auto resolved = resolve_geometry_render_config(requested, topologies);

    apply_render_config(resolved, ShaderConfig { resolved.vertex_shader });
    tune_point_mill(m_render_processor, resolved);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved);
    m_applied_runs.clear();
    apply_draw_runs(m_render_processor, get_render_config(), m_applied_runs, m_runs);
}

void AssembledGeometryBuffer::update_draw(std::vector<Portal::Graphics::DrawRun> runs)
{
    m_runs = std::move(runs);
    if (m_render_processor) {
        apply_draw_runs(m_render_processor, get_render_config(), m_applied_runs, m_runs);
    }
}

void AssembledGeometryBuffer::wire_field_pass()
{
    const auto chain = m_network->get_operator_chain();
    if (!chain) {
        return;
    }

    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op;
    for (const auto& op : chain->operators()) {
        field_op = std::dynamic_pointer_cast<Nodes::Network::GpuFieldOperator>(op);
        if (field_op) {
            break;
        }
    }
    if (!field_op) {
        return;
    }

    if (field_op->get_layout().stride_bytes != sizeof(Kakshya::Vertex)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "AssembledGeometryBuffer: the field operator's layout is {} bytes per vertex, "
            "the buffer's records are {}; no field pass is wired",
            field_op->get_layout().stride_bytes, sizeof(Kakshya::Vertex));
        return;
    }

    if (wants_spatial_stages(field_op->get_field_config())) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "AssembledGeometryBuffer: the hash, claim, density and population stages are "
            "not wired for an AssemblyNetwork; only the field pass is");
    }

    wire_field_stages(
        std::dynamic_pointer_cast<StateFieldBuffer>(shared_from_this()),
        field_op, std::nullopt, {}, nullptr);
}

size_t AssembledGeometryBuffer::estimate_vertex_bytes(
    const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network,
    float over_allocate_factor)
{
    constexpr size_t k_min = 4096;
    if (!network) {
        return k_min;
    }

    const size_t bytes = network->assembly()->get_vertex_count() * sizeof(Kakshya::Vertex);
    return std::max(k_min,
        static_cast<size_t>(static_cast<float>(bytes) * over_allocate_factor));
}

} // namespace MayaFlux::Buffers
