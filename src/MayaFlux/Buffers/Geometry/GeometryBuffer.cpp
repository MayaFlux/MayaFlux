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

    for (auto& [img, binding] : m_pending_textures)
        m_bindings_processor->set_texture(std::move(img), std::move(binding));
    m_pending_textures.clear();
}

void GeometryBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved_config = config;

    const bool textured = m_diffuse_texture != nullptr
        || !resolved_config.default_texture_binding.empty();

    if (resolved_config.topology == Portal::Graphics::PrimitiveTopology::POINT_LIST
        && m_geometry_node->get_primitive_topology()
            != Portal::Graphics::PrimitiveTopology::POINT_LIST) {
        resolved_config.topology = m_geometry_node->get_primitive_topology();
    }

    switch (resolved_config.topology) {
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

#ifndef MAYAFLUX_PLATFORM_MACOS
        if (config.geometry_shader.empty())
            resolved_config.geometry_shader = "line.geom.spv";
#else
        resolved_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif

        break;

    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "triangle.vert.spv";
        if (config.fragment_shader.empty()) {
            resolved_config.fragment_shader = textured
                ? "mesh_textured.frag.spv"
                : "triangle.frag.spv";
        }
        break;

    default:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "point.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "point.frag.spv";
    }

    const bool frag_samples_texture = resolved_config.fragment_shader.find("textured") != std::string::npos;

    if (textured && !frag_samples_texture) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "GeometryBuffer::setup_rendering: texture supplied but fragment shader '{}' "
            "does not sample a texture — texture will be ignored",
            resolved_config.fragment_shader);
    }

    const bool apply_texture = textured && frag_samples_texture;

    ShaderConfig sc { resolved_config.vertex_shader };
    if (apply_texture) {
        const std::string slot = resolved_config.default_texture_binding.empty()
            ? m_diffuse_binding
            : resolved_config.default_texture_binding;
        sc.bindings[slot] = ShaderBinding(0, 1, vk::DescriptorType::eCombinedImageSampler);
    }

    apply_render_config(resolved_config, sc);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    if (apply_texture && m_diffuse_texture)
        set_texture(m_diffuse_texture, m_diffuse_binding);

    set_default_render_config(resolved_config);
}

void GeometryBuffer::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    m_diffuse_texture = image;
    m_diffuse_binding = binding;

    if (!m_bindings_processor) {
        m_pending_textures.emplace_back(std::move(image), std::move(binding));
        return;
    }
    m_bindings_processor->set_texture(std::move(image), std::move(binding));
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
