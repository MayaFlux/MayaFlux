#include "TextureBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "TextureProcessor.hpp"

namespace MayaFlux::Buffers {

TextureBuffer::TextureBuffer(
    uint32_t width, uint32_t height,
    Portal::Graphics::ImageFormat format,
    const void* initial_pixel_data)
    : VKBuffer(
          calculate_quad_size(),
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

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created TextureBuffer: {}x{} ({} pixel bytes, {} vertex bytes)",
        m_width, m_height, m_pixel_data.size(), get_size_bytes());
}

void TextureBuffer::setup_processors(ProcessingToken token)
{
    initialize_quad_vertices();

    auto self = std::dynamic_pointer_cast<TextureBuffer>(shared_from_this());

    m_texture_processor = std::make_shared<TextureProcessor>();
    m_texture_processor->set_processing_token(token);
    m_texture_processor->bind_texture_buffer(self);

    set_default_processor(m_texture_processor);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "TextureBuffer initialized with TextureProcessor");
}

size_t TextureBuffer::calculate_quad_size()
{
    // 4 vertices * (3 floats position + 2 floats texcoord)
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texcoord;
    };
    return 4 * sizeof(QuadVertex);
}

void TextureBuffer::initialize_quad_vertices()
{
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texcoord;
    };

    const std::vector<QuadVertex> quad_vertices = {
        { { -1.0F, -1.0F, 0.0F }, { 0.0F, 1.0F } }, // Bottom-left
        { { 1.0F, -1.0F, 0.0F }, { 1.0F, 1.0F } }, // Bottom-right
        { { 1.0F, 1.0F, 0.0F }, { 1.0F, 0.0F } }, // Top-right
        { { -1.0F, 1.0F, 0.0F }, { 0.0F, 0.0F } } // Top-left
    };

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

    std::vector<uint8_t> vertex_bytes(
        reinterpret_cast<const uint8_t*>(quad_vertices.data()),
        reinterpret_cast<const uint8_t*>(quad_vertices.data()) + sizeof(QuadVertex) * 4);

    set_data({ vertex_bytes });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Initialized fullscreen quad vertices in TextureBuffer");
}

} // namespace MayaFlux::Buffers
