#include "NodeTextureBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NodeTextureBuffer::NodeTextureBuffer(
    std::shared_ptr<Nodes::GpuSync::TextureNode> node,
    std::string binding_name)
    : VKBuffer(
          calculate_quad_vertex_size(),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_texture_node(std::move(node))
    , m_binding_name(std::move(binding_name))
{
    if (!m_texture_node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create NodeTextureBuffer with null TextureNode");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created NodeTextureBuffer '{}' for {}x{} texture ({} bytes)",
        m_binding_name,
        m_texture_node->get_width(),
        m_texture_node->get_height(),
        get_size_bytes());
}

void NodeTextureBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<NodeTextureBuffer>(shared_from_this());

    generate_fullscreen_quad();

    auto& loom = Portal::Graphics::get_texture_manager();
    m_gpu_texture = loom.create_2d(
        m_texture_node->get_width(),
        m_texture_node->get_height(),
        Portal::Graphics::ImageFormat::RGBA32F,
        nullptr,
        1);

    if (!m_gpu_texture) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Failed to create GPU texture for NodeTextureBuffer '{}'",
            m_binding_name);
    }

    m_texture_processor = std::make_shared<NodeTextureProcessor>();
    m_texture_processor->set_processing_token(token);

    m_texture_processor->bind_texture_node(
        m_binding_name,
        m_texture_node,
        m_gpu_texture);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    set_default_processor(m_texture_processor);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "NodeTextureBuffer '{}' ready: staging={} bytes, GPU texture={}x{}",
        m_binding_name,
        get_size_bytes(),
        m_texture_node->get_width(),
        m_texture_node->get_height());
}

void NodeTextureBuffer::setup_rendering(const RenderConfig& config)
{
    if (!m_render_processor) {
        ShaderProcessorConfig shader_config { config.vertex_shader };
        shader_config.bindings[config.default_texture_binding] = ShaderBinding(
            0, 0, vk::DescriptorType::eCombinedImageSampler);

        uint32_t binding_index = 1;
        for (const auto& [name, _] : config.additional_textures) {
            shader_config.bindings[name] = ShaderBinding(
                0, binding_index++, vk::DescriptorType::eCombinedImageSampler);
        }

        m_render_processor = std::make_shared<RenderProcessor>(shader_config);
    }

    m_render_processor->set_fragment_shader(config.fragment_shader);
    m_render_processor->set_target_window(config.target_window);
    m_render_processor->set_primitive_topology(config.topology);

    m_render_processor->bind_texture(config.default_texture_binding, get_gpu_texture());

    for (const auto& [name, texture] : config.additional_textures) {
        m_render_processor->bind_texture(name, texture);
    }

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "NodeTextureBuffer '{}' rendering configured: shader={}, topology={}",
        m_binding_name, config.fragment_shader,
        static_cast<int>(config.topology));
}

size_t NodeTextureBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::GpuSync::TextureNode>& node)
{
    if (!node) {
        return 0;
    }

    size_t size = static_cast<size_t>(node->get_width())
        * static_cast<size_t>(node->get_height())
        * 4
        * sizeof(float);

    if (size == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "TextureNode has zero dimensions. Using minimum buffer size.");
        return 4096;
    }

    return size;
}

void NodeTextureBuffer::generate_fullscreen_quad()
{
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texcoord;
    };

    const std::vector<QuadVertex> quad = {
        { { -1.0F, -1.0F, 0.0F }, { 0.0F, 1.0F } },
        { { 1.0F, -1.0F, 0.0F }, { 1.0F, 1.0F } },
        { { -1.0F, 1.0F, 0.0F }, { 0.0F, 0.0F } },
        { { 1.0F, 1.0F, 0.0F }, { 1.0F, 0.0F } }
    };

    m_vertex_bytes.resize(quad.size() * sizeof(QuadVertex));
    std::memcpy(m_vertex_bytes.data(), quad.data(), m_vertex_bytes.size());

    // Set vertex layout
    Kakshya::VertexLayout vertex_layout {
        .vertex_count = 4,
        .stride_bytes = sizeof(QuadVertex),
        .attributes = {
            { .component_modality = Kakshya::DataModality::VERTEX_POSITIONS_3D,
                .offset_in_vertex = offsetof(QuadVertex, position),
                .name = "position" },
            { .component_modality = Kakshya::DataModality::TEXTURE_COORDS_2D,
                .offset_in_vertex = offsetof(QuadVertex, texcoord),
                .name = "texcoord" } }
    };

    set_vertex_layout(vertex_layout);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "NodeTextureBuffer: generated fullscreen quad");
}

size_t NodeTextureBuffer::calculate_quad_vertex_size()
{
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texcoord;
    };
    return 4 * sizeof(QuadVertex);
}

} // namespace MayaFlux::Buffers
