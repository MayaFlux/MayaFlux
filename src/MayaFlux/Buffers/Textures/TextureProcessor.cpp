#include "TextureProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "TextureBuffer.hpp"

namespace MayaFlux::Buffers {

TextureProcessor::TextureProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

TextureProcessor::~TextureProcessor() = default;

void TextureProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    if (!m_buffer_service) {
        m_buffer_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::BufferService>();
    }

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "TextureProcessor requires a valid buffer service");
    }

    auto tex_buffer = std::dynamic_pointer_cast<TextureBuffer>(buffer);
    if (!tex_buffer) {
        return;
    }

    m_texture_buffer = tex_buffer;

    if (!tex_buffer->is_initialized()) {
        try {
            m_buffer_service->initialize_buffer(tex_buffer);
        } catch (const std::exception& e) {
            error_rethrow(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "Failed to initialize texture buffer: {}", e.what());
        }
    }

    initialize_gpu_resources();

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor attached to {}x{} TextureBuffer",
        tex_buffer->get_width(), tex_buffer->get_height());
}

void TextureProcessor::on_detach(std::shared_ptr<Buffer> /*buffer*/)
{
    m_texture_buffer.reset();
}

void TextureProcessor::processing_function(std::shared_ptr<Buffer> /*buffer*/)
{
    if (!m_texture_buffer) {
        return;
    }

    update_geometry_if_dirty();

    update_pixels_if_dirty();
}

// =========================================================================
// Initialization
// =========================================================================

void TextureProcessor::initialize_gpu_resources()
{
    if (!m_texture_buffer) {
        return;
    }

    m_texture_buffer->m_gpu_texture = create_gpu_texture();

    upload_initial_geometry();

    upload_initial_pixels();

    m_texture_buffer->m_texture_dirty = false;
    m_texture_buffer->m_geometry_dirty = false;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor: GPU resources initialized");
}

void TextureProcessor::upload_initial_geometry()
{
    if (!m_texture_buffer || m_texture_buffer->m_vertex_bytes.empty()) {
        return;
    }

    try {
        upload_to_gpu(
            m_texture_buffer->m_vertex_bytes.data(),
            m_texture_buffer->m_vertex_bytes.size(),
            m_texture_buffer);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureProcessor: uploaded {} bytes of geometry data",
            m_texture_buffer->m_vertex_bytes.size());
    } catch (const std::exception& e) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to upload initial geometry: {}", e.what());
    }
}

void TextureProcessor::upload_initial_pixels()
{
    if (!m_texture_buffer || !m_texture_buffer->has_texture()) {
        return;
    }

    const auto& pixel_data = m_texture_buffer->m_pixel_data;
    if (pixel_data.empty()) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureProcessor: no pixel data to upload (uninitialized texture)");
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    loom.upload_data(
        m_texture_buffer->get_texture(),
        pixel_data.data(),
        pixel_data.size());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor: uploaded {} bytes of pixel data", pixel_data.size());
}

// =========================================================================
// Per-Frame Updates
// =========================================================================

void TextureProcessor::update_geometry_if_dirty()
{
    if (!m_texture_buffer || !m_texture_buffer->m_geometry_dirty) {
        return;
    }

    m_texture_buffer->generate_quad_with_transform();

    try {
        upload_to_gpu(
            m_texture_buffer->m_vertex_bytes.data(),
            m_texture_buffer->m_vertex_bytes.size(),
            m_texture_buffer);

        m_texture_buffer->m_geometry_dirty = false;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureProcessor: geometry updated and uploaded");
    } catch (const std::exception& e) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to update geometry: {}", e.what());
    }
}

void TextureProcessor::update_pixels_if_dirty()
{
    if (!m_texture_buffer || !m_texture_buffer->m_texture_dirty) {
        return;
    }

    if (!m_texture_buffer->has_texture()) {
        m_texture_buffer->m_gpu_texture = create_gpu_texture();
    }

    const auto& pixel_data = m_texture_buffer->m_pixel_data;
    if (pixel_data.empty()) {
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    loom.upload_data(
        m_texture_buffer->get_texture(),
        pixel_data.data(),
        pixel_data.size());

    m_texture_buffer->m_texture_dirty = false;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor: pixel data updated ({} bytes)", pixel_data.size());
}

// =========================================================================
// GPU Resource Creation
// =========================================================================

std::shared_ptr<Core::VKImage> TextureProcessor::create_gpu_texture()
{
    if (!m_texture_buffer) {
        return nullptr;
    }

    auto& loom = Portal::Graphics::get_texture_manager();

    auto texture = loom.create_2d(
        m_texture_buffer->get_width(),
        m_texture_buffer->get_height(),
        m_texture_buffer->get_format(),
        nullptr,
        1);

    if (!texture) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Failed to create GPU texture via TextureLoom");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor: created GPU VKImage {}x{}",
        m_texture_buffer->get_width(), m_texture_buffer->get_height());

    return texture;
}

void TextureProcessor::generate_quad_vertices(std::vector<uint8_t>& out_bytes)
{
    // Helper for future enhancements (actual transform application)
    // For now, this is called by TextureBuffer::generate_quad_with_transform()
    // Future: apply m_position, m_scale, m_rotation here
}

} // namespace MayaFlux::Buffers
