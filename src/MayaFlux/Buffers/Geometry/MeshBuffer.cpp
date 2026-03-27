#include "MeshBuffer.hpp"
#include "MeshProcessor.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// =============================================================================
// Construction
// =============================================================================

MeshBuffer::MeshBuffer(Kakshya::MeshData data)
    : VKBuffer(
          [&data]() -> size_t {
              const auto* vb = std::get_if<std::vector<uint8_t>>(
                  &data.vertex_variant);
              return vb ? vb->size() : 0;
          }(),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_mesh_data(std::move(data))
{
    if (!m_mesh_data.is_valid()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "MeshBuffer constructed with invalid MeshData — "
            "GPU upload will be skipped until valid data is provided");
    }

    RenderConfig defaults;
    defaults.vertex_shader = "triangle.vert.spv";
    defaults.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
    set_default_render_config(defaults);

    set_needs_depth_attachment(true);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshBuffer: {} vertices, {} indices ({} faces), stride {} bytes",
        m_mesh_data.vertex_count(),
        get_index_count(),
        m_mesh_data.face_count(),
        m_mesh_data.layout.stride_bytes);
}

// =============================================================================
// setup_processors
// =============================================================================

void MeshBuffer::setup_processors(ProcessingToken token)
{
    m_mesh_processor = std::make_shared<MeshProcessor>();
    m_mesh_processor->set_processing_token(token);
    set_default_processor(m_mesh_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "MeshBuffer::setup_processors with token {}",
        static_cast<int>(token));
}

// =============================================================================
// setup_rendering
// =============================================================================

void MeshBuffer::setup_rendering(const RenderConfig& config)
{
    if (!config.vertex_shader.empty()) {
        m_render_config.vertex_shader = config.vertex_shader;
    }
    if (!config.fragment_shader.empty()) {
        m_render_config.fragment_shader = config.fragment_shader;
    }
    if (!config.default_texture_binding.empty()) {
        m_render_config.default_texture_binding = config.default_texture_binding;
    }

    if (!m_render_config.default_texture_binding.empty()
        && m_diffuse_binding != m_render_config.default_texture_binding) {
        m_diffuse_binding = m_render_config.default_texture_binding;
    }

    m_render_config.target_window = config.target_window;
    m_render_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;

    if (!config.additional_textures.empty()) {
        for (const auto& [name, texture] : config.additional_textures) {
            m_render_config.additional_textures.emplace_back(name, texture);
        }
    }

    const bool textured = m_diffuse_texture != nullptr
        || !m_render_config.default_texture_binding.empty();

    if (m_render_config.fragment_shader.empty()) {
        m_render_config.fragment_shader = textured
            ? "mesh_textured.frag.spv"
            : "triangle.frag.spv";
    }

    if (!m_render_processor) {
        ShaderConfig sc { m_render_config.vertex_shader };

        if (textured && !m_render_config.default_texture_binding.empty()) {
            sc.bindings[m_render_config.default_texture_binding] = ShaderBinding(
                0, 0, vk::DescriptorType::eCombinedImageSampler);
        }

        uint32_t binding_index = 1;
        for (const auto& [name, _] : m_render_config.additional_textures) {
            sc.bindings[name] = ShaderBinding(
                0, binding_index++, vk::DescriptorType::eCombinedImageSampler);
        }

        m_render_processor = std::make_shared<RenderProcessor>(sc);
    }

    m_render_processor->set_fragment_shader(m_render_config.fragment_shader);
    m_render_processor->set_target_window(
        m_render_config.target_window,
        std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(m_render_config.topology);

    if (m_diffuse_texture && !m_render_config.default_texture_binding.empty()) {
        m_render_processor->bind_texture(
            m_render_config.default_texture_binding,
            m_diffuse_texture);
    }

    for (const auto& [name, texture] : m_render_config.additional_textures) {
        m_render_processor->bind_texture(name, texture);
    }

    get_processing_chain()->add_final_processor(
        m_render_processor,
        shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshBuffer::setup_rendering: vert={} frag={} textured={}",
        m_render_config.vertex_shader,
        m_render_config.fragment_shader,
        textured);
}

// =============================================================================
// bind_diffuse_texture
// =============================================================================

void MeshBuffer::bind_diffuse_texture(
    std::shared_ptr<Core::VKImage> image,
    std::string_view binding_name)
{
    m_diffuse_texture = std::move(image);
    m_diffuse_binding = std::string(binding_name);

    if (m_render_processor) {
        if (m_render_config.default_texture_binding == m_diffuse_binding) {
            m_render_processor->bind_texture(m_diffuse_binding, m_diffuse_texture);
        } else {
            MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "MeshBuffer::bind_diffuse_texture: pipeline was created without "
                "binding '{}' — call before setup_rendering() for correct results",
                m_diffuse_binding);
        }
    }
}

// =============================================================================
// Mutation
// =============================================================================

void MeshBuffer::set_vertex_data(std::span<const uint8_t> bytes)
{
    if (m_mesh_data.layout.stride_bytes > 0
        && bytes.size() % m_mesh_data.layout.stride_bytes != 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshBuffer::set_vertex_data: byte count {} not a multiple of stride {}",
            bytes.size(), m_mesh_data.layout.stride_bytes);
        return;
    }

    m_mesh_data.vertex_variant = std::vector<uint8_t>(bytes.begin(), bytes.end());
    m_mesh_data.layout.vertex_count = m_mesh_data.layout.stride_bytes > 0
        ? static_cast<uint32_t>(bytes.size() / m_mesh_data.layout.stride_bytes)
        : 0;

    m_vertices_dirty.store(true, std::memory_order_release);
}

void MeshBuffer::set_index_data(std::span<const uint32_t> indices)
{
    if (indices.size() % 3 != 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshBuffer::set_index_data: index count {} is not a multiple of 3",
            indices.size());
        return;
    }

    m_mesh_data.index_variant = std::vector<uint32_t>(indices.begin(), indices.end());
    m_indices_dirty.store(true, std::memory_order_release);
}

void MeshBuffer::set_mesh_data(Kakshya::MeshData data)
{
    m_mesh_data = std::move(data);
    m_vertices_dirty.store(true, std::memory_order_release);
    m_indices_dirty.store(true, std::memory_order_release);
}

} // namespace MayaFlux::Buffers
