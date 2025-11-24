#include "TextureBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "TextureProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

const std::vector<TextureBuffer::QuadVertex> base_quad = {
    { { -0.5F, -0.5F, 0.0F }, { 0.0F, 1.0F } }, // Bottom-left
    { { 0.5F, -0.5F, 0.0F }, { 1.0F, 1.0F } }, // Bottom-right
    { { -0.5F, 0.5F, 0.0F }, { 0.0F, 0.0F } }, // Top-left
    { { 0.5F, 0.5F, 0.0F }, { 1.0F, 0.0F } } // Top-right
};

TextureBuffer::TextureBuffer(
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    const void* initial_pixel_data)
    : VKBuffer(
          calculate_quad_vertex_size(),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_width(width)
    , m_height(height)
    , m_format(format)
{
    if (initial_pixel_data) {
        size_t pixel_bytes = static_cast<size_t>(width) * height * Portal::Graphics::TextureLoom::get_bytes_per_pixel(format);
        m_pixel_data.resize(pixel_bytes);
        std::memcpy(m_pixel_data.data(), initial_pixel_data, pixel_bytes);
    }

    generate_default_quad();

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created TextureBuffer: {}x{} ({} pixel bytes, {} vertex bytes)",
        m_width, m_height, m_pixel_data.size(), m_vertex_bytes.size());
}

void TextureBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<TextureBuffer>(shared_from_this());

    m_texture_processor = std::make_shared<TextureProcessor>();
    m_texture_processor->set_processing_token(token);

    set_default_processor(m_texture_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "TextureBuffer setup_processors: TextureProcessor will be attached on first registration");
}

void TextureBuffer::setup_rendering(const RenderConfig& config)
{
    if (!m_render_processor) {
        ShaderProcessorConfig shader_config { config.vertex_shader };
        shader_config.bindings[config.default_texture_binding] = ShaderBinding(
            0, 0, vk::DescriptorType::eCombinedImageSampler);

        m_render_processor = std::make_shared<RenderProcessor>(shader_config);
    }

    m_render_processor->set_shader(config.vertex_shader);
    m_render_processor->set_fragment_shader(config.fragment_shader);
    m_render_processor->set_target_window(config.target_window);
    m_render_processor->set_primitive_topology(config.topology);
    m_render_processor->bind_texture(config.default_texture_binding, this->get_texture());

    if (config.additional_texture) {
        m_render_processor->bind_texture(
            config.additional_texture->first,
            config.additional_texture->second);
    }

    get_processing_chain()->add_processor(m_render_processor, shared_from_this());
}

// =========================================================================
// Pixel Data Management
// =========================================================================

void TextureBuffer::set_pixel_data(const void* data, size_t size)
{
    if (!data || size == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_pixel_data called with null or empty data");
        return;
    }

    m_pixel_data.resize(size);
    std::memcpy(m_pixel_data.data(), data, size);
    m_texture_dirty = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureBuffer: pixel data updated ({} bytes, marked dirty)", size);
}

void TextureBuffer::mark_pixels_dirty()
{
    m_texture_dirty = true;
}

// =========================================================================
// Display Transform
// =========================================================================

void TextureBuffer::set_position(float x, float y)
{
    if (m_position.x != x || m_position.y != y) {
        m_position = { x, y };
        m_geometry_dirty = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: position set to ({}, {}), geometry marked dirty", x, y);
    }
}

void TextureBuffer::set_scale(float width, float height)
{
    if (m_scale.x != width || m_scale.y != height) {
        m_scale = { width, height };
        m_geometry_dirty = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: scale set to ({}, {}), geometry marked dirty", width, height);
    }
}

void TextureBuffer::set_rotation(float angle_radians)
{
    if (m_rotation != angle_radians) {
        m_rotation = angle_radians;
        m_geometry_dirty = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: rotation set to {}, geometry marked dirty", angle_radians);
    }
}

// =========================================================================
// Custom Geometry
// =========================================================================

void TextureBuffer::set_custom_vertices(const std::vector<QuadVertex>& vertices)
{
    if (vertices.size() != 4) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_custom_vertices: must provide exactly 4 vertices, got {}", vertices.size());
        return;
    }

    m_vertex_bytes.resize(vertices.size() * sizeof(QuadVertex));
    std::memcpy(m_vertex_bytes.data(), vertices.data(), m_vertex_bytes.size());
    m_uses_custom_vertices = true;
    m_geometry_dirty = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureBuffer: custom vertices set, geometry marked dirty");
}

void TextureBuffer::use_default_quad()
{
    if (m_uses_custom_vertices) {
        m_uses_custom_vertices = false;
        generate_default_quad();
        m_geometry_dirty = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: reset to default quad, geometry marked dirty");
    }
}

// =========================================================================
// Geometry Generation
// =========================================================================

size_t TextureBuffer::calculate_quad_vertex_size()
{
    return 4 * sizeof(QuadVertex);
}

void TextureBuffer::generate_default_quad()
{
    m_vertex_bytes.resize(base_quad.size() * sizeof(QuadVertex));
    std::memcpy(m_vertex_bytes.data(), base_quad.data(), m_vertex_bytes.size());

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
        "TextureBuffer: generated default fullscreen quad");
}

void TextureBuffer::generate_quad_with_transform()
{
    if (m_uses_custom_vertices) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: using custom vertices, skipping transform");
        return;
    }

    float cos_rot = std::cos(m_rotation);
    float sin_rot = std::sin(m_rotation);

    std::vector<QuadVertex> transformed(4);
    for (size_t i = 0; i < 4; ++i) {
        glm::vec3 pos = base_quad[i].position;

        pos.x *= m_scale.x;
        pos.y *= m_scale.y;

        float rotated_x = pos.x * cos_rot - pos.y * sin_rot;
        float rotated_y = pos.x * sin_rot + pos.y * cos_rot;

        pos.x = rotated_x + m_position.x;
        pos.y = rotated_y + m_position.y;

        transformed[i].position = pos;
        transformed[i].texcoord = base_quad[i].texcoord;
    }

    m_vertex_bytes.resize(transformed.size() * sizeof(QuadVertex));
    std::memcpy(m_vertex_bytes.data(), transformed.data(), m_vertex_bytes.size());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureBuffer: regenerated quad with transform (pos={},{}, scale={},{}, rot={})",
        m_position.x, m_position.y, m_scale.x, m_scale.y, m_rotation);
}

} // namespace MayaFlux::Buffers
