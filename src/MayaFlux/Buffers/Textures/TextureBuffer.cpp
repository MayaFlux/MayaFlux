#include "TextureBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "TextureProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TextureBuffer::TextureBuffer(
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    const void* initial_pixel_data)
    : VKBuffer(
          calculate_quad_vertex_size(),
          Usage::VERTEX,
          Kakshya::DataModality::IMAGE_COLOR)
    , m_width(width)
    , m_height(height)
    , m_format(format)
{
    RenderConfig default_config;
    default_config.vertex_shader = "texture.vert.spv";
    default_config.fragment_shader = "texture.frag.spv";
    if (format == Portal::Graphics::ImageFormat::R8) {
        default_config.fragment_shader = "texture_gray.frag.spv";
    } else if (format == Portal::Graphics::ImageFormat::RG8) {
        default_config.fragment_shader = "texture_gray_alpha.frag.spv";
    }
    default_config.default_texture_binding = "texSampler";
    default_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP;
    set_default_render_config(default_config);

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

void TextureBuffer::setup_rendering(const RenderConfig& user_config)
{
    if (!user_config.vertex_shader.empty()) {
        m_render_config.vertex_shader = user_config.vertex_shader;
    }
    if (!user_config.fragment_shader.empty()) {
        m_render_config.fragment_shader = user_config.fragment_shader;
    }
    if (!user_config.default_texture_binding.empty()) {
        m_render_config.default_texture_binding = user_config.default_texture_binding;
    }
    if (user_config.topology != Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer only supports TRIANGLE_STRIP topology. Ignoring provided topology.");
    } else {
        if (user_config.topology != m_render_config.topology) {
            m_render_config.topology = user_config.topology;
        }
    }
    m_render_config.target_window = user_config.target_window;
    m_render_config.default_sampler = user_config.default_sampler;

    if (!user_config.additional_textures.empty()) {
        for (const auto& entry : user_config.additional_textures) {
            m_render_config.additional_textures.push_back(entry);
        }
    }

    ShaderConfig shader_config { m_render_config.vertex_shader };
    shader_config.bindings[m_render_config.default_texture_binding] = ShaderBinding(
        0, 1, vk::DescriptorType::eCombinedImageSampler);

    uint32_t binding_index = 2;
    for (const auto& entry : m_render_config.additional_textures) {
        shader_config.bindings[entry.name] = ShaderBinding(
            0, binding_index++, vk::DescriptorType::eCombinedImageSampler);
    }

    apply_render_config(m_render_config, shader_config);

    if (m_render_config.default_sampler) {
        m_render_processor->set_binding_sampler(
            m_render_config.default_texture_binding, *m_render_config.default_sampler);
    }
    m_render_processor->bind_texture(m_render_config.default_texture_binding, get_texture());

    for (const auto& entry : m_render_config.additional_textures) {
        m_render_processor->bind_texture(entry);
    }

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());
}

void TextureBuffer::set_sampler(const Portal::Graphics::SamplerConfig& config)
{
    set_sampler(m_render_config.default_texture_binding, config);
}

void TextureBuffer::set_sampler(const std::string& name, const Portal::Graphics::SamplerConfig& config)
{
    const auto processor = get_render_processor();
    if (!processor) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer::set_sampler('{}') called before setup_rendering; the sampler is not set", name);
        return;
    }

    if (name == m_render_config.default_texture_binding) {
        m_render_config.default_sampler = config;
    } else {
        const auto entry = std::ranges::find_if(m_render_config.additional_textures,
            [&name](const auto& texture) { return texture.name == name; });
        if (entry == m_render_config.additional_textures.end()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "TextureBuffer::set_sampler: no texture named '{}' in the render config", name);
            return;
        }
        entry->sampler = config;
    }

    processor->set_binding_sampler(name, config);
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

void TextureBuffer::set_gpu_texture(std::shared_ptr<Core::VKImage> image)
{
    m_gpu_texture = std::move(image);

    if (m_render_processor && m_gpu_texture) {
        m_render_processor->bind_texture(
            m_render_config.default_texture_binding,
            m_gpu_texture);
    }
}

void TextureBuffer::resize_texture(uint32_t new_width, uint32_t new_height)
{
    if (new_width == m_width && new_height == m_height) {
        return;
    }

    m_width = new_width;
    m_height = new_height;
    m_gpu_texture.reset();
    m_texture_dirty = true;
    m_geometry_dirty = true;
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

void TextureBuffer::set_custom_vertices(const std::vector<Kakshya::TextureQuadVertex>& vertices)
{
    if (vertices.size() != 4) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_custom_vertices: must provide exactly 4 vertices, got {}", vertices.size());
        return;
    }

    m_vertex_bytes.resize(vertices.size() * sizeof(Kakshya::TextureQuadVertex));
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
    return 4 * sizeof(Kakshya::TextureQuadVertex);
}

void TextureBuffer::generate_default_quad()
{
    auto geo = Kinesis::generate_quad(m_position, m_scale);
    m_vertex_bytes.resize(geo.vertices.size() * sizeof(Kakshya::TextureQuadVertex));
    std::memcpy(m_vertex_bytes.data(), geo.vertices.data(), m_vertex_bytes.size());
    set_vertex_layout(geo.layout);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureBuffer: generated default quad");
}

void TextureBuffer::generate_quad_with_transform()
{
    if (m_uses_custom_vertices) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBuffer: using custom vertices, skipping transform");
        return;
    }

    auto geo = Kinesis::generate_quad(m_position, m_scale, m_rotation);
    m_vertex_bytes.resize(geo.vertices.size() * sizeof(Kakshya::TextureQuadVertex));
    std::memcpy(m_vertex_bytes.data(), geo.vertices.data(), m_vertex_bytes.size());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureBuffer: regenerated quad with transform (pos={},{}, scale={},{}, rot={})",
        m_position.x, m_position.y, m_scale.x, m_scale.y, m_rotation);
}

} // namespace MayaFlux::Buffers
