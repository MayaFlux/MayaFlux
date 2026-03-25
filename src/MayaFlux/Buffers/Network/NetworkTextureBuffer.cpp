#include "NetworkTextureBuffer.hpp"

#include "MayaFlux/Buffers/Shaders/UVFieldProcessor.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkTextureBuffer::NetworkTextureBuffer(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    std::shared_ptr<Core::VKImage> texture,
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
            "Cannot create NetworkTextureBuffer with null NodeNetwork");
    }

    m_uv_processor = std::make_shared<UVFieldProcessor>();

    if (texture)
        m_uv_processor->set_texture(std::move(texture));

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created NetworkTextureBuffer '{}' for {} nodes ({} bytes estimated)",
        m_binding_name,
        m_network->get_node_count(),
        get_size_bytes());
}

// =============================================================================
// Processor setup
// =============================================================================

void NetworkTextureBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<NetworkTextureBuffer>(shared_from_this());

    m_geometry_processor = std::make_shared<NetworkGeometryProcessor>();
    m_geometry_processor->set_processing_token(token);
    m_geometry_processor->bind_network(m_binding_name, m_network, self);

    set_default_processor(m_geometry_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);
    chain->add_postprocessor(m_uv_processor, self);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "NetworkTextureBuffer '{}': geometry processor + UV field postprocessor wired",
        m_binding_name);
}

// =============================================================================
// Rendering setup  — mirrors NetworkGeometryBuffer exactly
// =============================================================================

void NetworkTextureBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;

    switch (config.topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "point.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "point.frag.spv";
        break;
    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
#ifndef MAYAFLUX_PLATFORM_MACOS
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "line.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "line.frag.spv";
        if (resolved.geometry_shader.empty())
            resolved.geometry_shader = "line.geom.spv";
#else
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "line_fallback.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "line_fallback.frag.spv";
        resolved.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif
        break;
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "triangle.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "triangle.frag.spv";
        break;
    default:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "point.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "point.frag.spv";
        break;
    }

    if (!m_render_processor) {
        m_render_processor = std::make_shared<RenderProcessor>(
            ShaderConfig { resolved.vertex_shader });
    } else {
        m_render_processor->set_shader(resolved.vertex_shader);
    }

    m_render_processor->set_fragment_shader(resolved.fragment_shader);
    if (!resolved.geometry_shader.empty())
        m_render_processor->set_geometry_shader(resolved.geometry_shader);

    m_render_processor->set_target_window(
        config.target_window,
        std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(resolved.topology);
    m_render_processor->set_polygon_mode(config.polygon_mode);
    m_render_processor->set_cull_mode(config.cull_mode);

    get_processing_chain()->add_final_processor(
        m_render_processor,
        std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));

    set_default_render_config(resolved);
}

// =============================================================================
// Runtime texture swap
// =============================================================================

void NetworkTextureBuffer::set_texture(
    std::shared_ptr<Core::VKImage> image,
    const Portal::Graphics::SamplerConfig& config)
{
    m_uv_processor->set_texture(std::move(image), config);
}

// =============================================================================
// Vertex count
// =============================================================================

uint32_t NetworkTextureBuffer::get_vertex_count() const
{
    if (!m_network)
        return 0;

    auto* op = m_network->get_operator();
    if (op) {
        auto* gop = dynamic_cast<Nodes::Network::GraphicsOperator*>(op);
        if (gop)
            return static_cast<uint32_t>(gop->get_vertex_count());
    }

    return static_cast<uint32_t>(m_network->get_node_count());
}

// =============================================================================
// Buffer sizing
// =============================================================================

size_t NetworkTextureBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    float over_allocate_factor)
{
    if (!network)
        return 0;

    size_t count = network->get_node_count();
    if (count == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NodeNetwork has zero nodes, using minimum buffer size");
        return 4096;
    }

    constexpr size_t k_vertex_stride = 60;
    return static_cast<size_t>(
        static_cast<float>(count * k_vertex_stride) * over_allocate_factor);
}

} // namespace MayaFlux::Buffers
