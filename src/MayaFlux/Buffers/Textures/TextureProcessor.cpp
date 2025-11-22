#include "TextureProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "TextureBuffer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TextureProcessor::TextureProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

TextureProcessor::~TextureProcessor() = default;

void TextureProcessor::bind_texture_buffer(const std::shared_ptr<TextureBuffer>& texture)
{
    m_texture_buffer = texture;

    if (!m_texture_buffer->has_texture()) {
        m_texture_buffer->m_gpu_texture = create_gpu_texture();

        upload_pixels_to_gpu();
        m_texture_buffer->clear_dirty_flag();
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "TextureProcessor bound to {}x{} texture",
        m_texture_buffer->get_width(), m_texture_buffer->get_height());
}

void TextureProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_texture_buffer) {
        return;
    }

    if (!m_texture_buffer->has_texture()) {
        m_texture_buffer->m_gpu_texture = create_gpu_texture();
    }

    if (needs_upload()) {
        upload_pixels_to_gpu();
        m_texture_buffer->clear_dirty_flag();

        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureProcessor uploaded pixels to GPU");
    }
}

void TextureProcessor::upload_pixels_to_gpu()
{
    if (!m_texture_buffer->has_texture()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot upload pixels: GPU texture not created");
        return;
    }

    const auto& pixel_data = m_texture_buffer->get_pixel_data();
    if (pixel_data.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot upload: no pixel data");
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    loom.upload_data(
        m_texture_buffer->get_texture(),
        pixel_data.data(),
        pixel_data.size());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Uploaded {} bytes to GPU texture", pixel_data.size());
}
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
            "BufferUploadProcessor requires a valid buffer service");
    }

    auto tex_buffer = std::dynamic_pointer_cast<TextureBuffer>(buffer);
    if (tex_buffer) {
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

        if (!tex_buffer->m_vertex_bytes.empty()) {
            try {
                upload_to_gpu(
                    tex_buffer->m_vertex_bytes.data(),
                    tex_buffer->m_vertex_bytes.size(),
                    tex_buffer);

                MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "TextureProcessor uploaded {} bytes of vertex data to GPU",
                    tex_buffer->m_vertex_bytes.size());
            } catch (const std::exception& e) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Failed to upload vertex data: {}", e.what());
            }
        }

        bind_texture_buffer(tex_buffer);
    }
}

void TextureProcessor::on_detach(std::shared_ptr<Buffer> /*buffer*/)
{
    m_texture_buffer.reset();
}

std::shared_ptr<Core::VKImage> TextureProcessor::create_gpu_texture()
{
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
        "Created GPU VKImage: {}x{}",
        m_texture_buffer->get_width(), m_texture_buffer->get_height());

    return texture;
}

bool TextureProcessor::needs_upload() const
{
    return m_texture_buffer && m_texture_buffer->is_texture_dirty();
}

} // namespace MayaFlux::Buffers
