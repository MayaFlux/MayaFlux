#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkGeometryBuffer::NetworkGeometryBuffer(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    const std::string& binding_name,
    float over_allocate_factor)
    : VKBuffer(
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
}

void NetworkGeometryBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved_config = config;

    if (config.vertex_shader.empty() || config.fragment_shader.empty()) {
        switch (config.topology) {
        case Portal::Graphics::PrimitiveTopology::POINT_LIST:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "point.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "point.frag.spv";
            break;
        case Portal::Graphics::PrimitiveTopology::LINE_LIST:
        case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "line.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "line.frag.spv";
            if (config.geometry_shader.empty())
                resolved_config.geometry_shader = "line.geom.spv";
            break;
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "triangle.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "triangle.frag.spv";
            break;
        default:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "point.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "point.frag.spv";
        }
    }

    if (!m_render_processor) {
        m_render_processor = std::make_shared<RenderProcessor>(
            ShaderConfig { resolved_config.vertex_shader });
    } else {
        m_render_processor->set_shader(resolved_config.vertex_shader);
    }

    m_render_processor->set_fragment_shader(resolved_config.fragment_shader);
    if (!resolved_config.geometry_shader.empty()) {
        m_render_processor->set_geometry_shader(resolved_config.geometry_shader);
    }
    m_render_processor->set_target_window(config.target_window, std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(config.topology);
    m_render_processor->set_polygon_mode(config.polygon_mode);
    m_render_processor->set_cull_mode(config.cull_mode);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
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
        size_t vertex_size = sizeof(Nodes::PointVertex);
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

} // namespace MayaFlux::Buffers
