#include "ImageExport.hpp"

#include "ImageWriter.hpp"

#include "MayaFlux/Buffers/Textures/TextBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

// ============================================================================
// download_image
// ============================================================================

std::optional<ImageData> download_image(
    const std::shared_ptr<Core::VKImage>& image)
{
    if (!image) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: null image");
        return std::nullopt;
    }

    return Portal::Graphics::TextureLoom::instance().download_image(image);
}

// ============================================================================
// download_texture_buffer
// ============================================================================

std::optional<ImageData> download_texture_buffer(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_texture_buffer: null buffer");
        return std::nullopt;
    }

    auto image = buffer->get_gpu_texture();
    if (!image) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_texture_buffer: buffer has no GPU texture yet "
            "(has the buffer been processed at least once?)");
        return std::nullopt;
    }

    return download_image(image);
}

// ============================================================================
// save_image / save_texture_buffer / save_text_buffer
// ============================================================================

bool save_image(
    const std::shared_ptr<Core::VKImage>& image,
    const std::string& filepath,
    const ImageWriteOptions& options)
{
    auto data = download_image(image);
    if (!data) {
        return false;
    }

    auto writer = ImageWriterRegistry::instance().create_writer(filepath);
    if (!writer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_image: no writer registered for extension of '{}'", filepath);
        return false;
    }

    const bool ok = writer->write(filepath, *data, options);
    if (!ok) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_image: writer failed: {}", writer->get_last_error());
    }
    return ok;
}

bool save_texture_buffer(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer,
    const std::string& filepath,
    const ImageWriteOptions& options)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_texture_buffer: null buffer");
        return false;
    }
    auto image = buffer->get_gpu_texture();
    if (!image) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_texture_buffer: buffer has no GPU texture");
        return false;
    }
    return save_image(image, filepath, options);
}

bool save_text_buffer(
    const std::shared_ptr<Buffers::TextBuffer>& buffer,
    const std::string& filepath,
    const ImageWriteOptions& options)
{
    return save_texture_buffer(buffer, filepath, options);
}

} // namespace MayaFlux::IO
