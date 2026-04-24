#include "ImageExport.hpp"

#include "MayaFlux/Buffers/Textures/TextBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/IO/ImageWriter.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::IO {

namespace {

    using Portal::Graphics::ImageFormat;
    using Portal::Graphics::TextureLoom;

    /**
     * @brief Emplace the correct variant in ImageData based on format byte-width.
     *
     * Sizes the vector in element counts (not bytes): one element per channel
     * per pixel. The emplaced variant is left default-initialized to zero.
     */
    bool emplace_variant_for_format(ImageData& data, ImageFormat format,
        size_t pixel_count, uint32_t channels)
    {
        const size_t element_count = pixel_count * channels;
        const size_t bpp = TextureLoom::get_bytes_per_pixel(format);
        const size_t bpe = channels > 0 ? (bpp / channels) : 0;

        switch (bpe) {
        case 1:
            data.pixels.emplace<std::vector<uint8_t>>(element_count);
            return true;
        case 2:
            data.pixels.emplace<std::vector<uint16_t>>(element_count);
            return true;
        case 4:
            data.pixels.emplace<std::vector<float>>(element_count);
            return true;
        default:
            return false;
        }
    }

} // namespace

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

    const vk::Format vk_format = image->get_format();
    const auto format_opt = Portal::Graphics::TextureLoom::from_vulkan_format(vk_format);
    if (!format_opt) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: vk::Format {} has no ImageFormat mapping",
            vk::to_string(vk_format));
        return std::nullopt;
    }

    const ImageFormat format = *format_opt;
    const uint32_t channels = Portal::Graphics::TextureLoom::get_channel_count(format);
    if (channels == 0) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: zero channels for format {}",
            static_cast<int>(format));
        return std::nullopt;
    }

    const uint32_t width = image->get_width();
    const uint32_t height = image->get_height();
    if (width == 0 || height == 0) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: zero dimensions {}x{}", width, height);
        return std::nullopt;
    }

    const size_t pixel_count = static_cast<size_t>(width) * height;
    const size_t mip0_bytes = pixel_count * TextureLoom::get_bytes_per_pixel(format);

    ImageData result;
    result.width = width;
    result.height = height;
    result.channels = channels;
    result.format = format;

    if (!emplace_variant_for_format(result, format, pixel_count, channels)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: unsupported bytes-per-element for format {}",
            static_cast<int>(format));
        return std::nullopt;
    }

    TextureLoom::instance().download_data_async(
        image,
        const_cast<void*>(result.data()),
        mip0_bytes);

    if (!result.is_consistent()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_image: resulting ImageData failed is_consistent()");
        return std::nullopt;
    }

    MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
        "download_image: {}x{}, {} channels, {} bytes",
        width, height, channels, mip0_bytes);

    return result;
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
