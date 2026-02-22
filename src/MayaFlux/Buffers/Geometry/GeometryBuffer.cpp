#include "GeometryBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

GeometryBuffer::GeometryBuffer(
    std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> node,
    const std::string& binding_name,
    float over_allocate_factor)
    : VKBuffer(
          calculate_buffer_size(node, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_geometry_node(std::move(node))
    , m_binding_name(binding_name)
{
    if (!m_geometry_node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create GeometryBuffer with null GeometryWriterNode");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created GeometryBuffer '{}' for {} vertices ({} bytes, stride: {})",
        m_binding_name,
        m_geometry_node->get_vertex_count(),
        get_size_bytes(),
        m_geometry_node->get_vertex_stride());
}

void GeometryBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<GeometryBuffer>(shared_from_this());

    m_bindings_processor = std::make_shared<GeometryBindingsProcessor>();
    m_bindings_processor->set_processing_token(token);
    m_bindings_processor->bind_geometry_node(
        m_binding_name,
        m_geometry_node,
        self);

    set_default_processor(m_bindings_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);
}

void GeometryBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved_config = config;

    switch (config.topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "point.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "point.frag.spv";
        break;

    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
#ifndef MAYAFLUX_PLATFORM_MACOS
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "line.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "line.frag.spv";
        if (config.geometry_shader.empty())
            resolved_config.geometry_shader = "line.geom.spv";
#else
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "line_fallback.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "line_fallback.frag.spv";

        resolved_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif
        break;

    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        resolved_config.vertex_shader = "triangle.vert.spv";
        resolved_config.fragment_shader = "triangle.frag.spv";
        break;

    default:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "point.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "point.frag.spv";
    }

    if (!m_render_processor) {
        m_render_processor = std::make_shared<RenderProcessor>(ShaderConfig { resolved_config.vertex_shader });
    } else {
        m_render_processor->set_shader(resolved_config.vertex_shader);
    }

    m_render_processor->set_fragment_shader(resolved_config.fragment_shader);
    if (!resolved_config.geometry_shader.empty()) {
        m_render_processor->set_geometry_shader(resolved_config.geometry_shader);
    }
    m_render_processor->set_target_window(config.target_window, std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(resolved_config.topology);
    m_render_processor->set_polygon_mode(config.polygon_mode);
    m_render_processor->set_cull_mode(config.cull_mode);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
}

size_t GeometryBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
    float over_allocate_factor)
{
    if (!node) {
        return 0;
    }

    size_t base_size = node->get_vertex_buffer_size_bytes();

    if (base_size == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "GeometryWriterNode has zero-size vertex buffer. "
            "Did you forget to call set_vertex_stride() or resize_vertex_buffer()?");
        return 4096;
    }

    auto allocated_size = static_cast<size_t>(
        static_cast<float>(base_size) * over_allocate_factor);

    if (over_allocate_factor > 1.0F) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Over-allocated geometry buffer: {} → {} bytes ({}x)",
            base_size, allocated_size, over_allocate_factor);
    }

    return allocated_size;
}

} // namespace MayaFlux::Buffers
