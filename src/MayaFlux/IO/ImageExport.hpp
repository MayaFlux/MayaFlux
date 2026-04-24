#pragma once

#include "MayaFlux/IO/ImageWriter.hpp"

namespace MayaFlux::Core {
class VKImage;
}
namespace MayaFlux::Buffers {
class TextureBuffer;
class TextBuffer;
}

namespace MayaFlux::IO {

/**
 * @brief Download pixel data from a GPU-resident VKImage into host ImageData.
 *
 * Performs a blocking GPU->host transfer via TextureLoom::download_data,
 * allocating a host buffer of the appropriate variant for the image format.
 * The source image is restored to eShaderReadOnlyOptimal / eFragmentShader
 * after the copy; pass a different restore layout/stage via the overload if
 * the image was, for example, in a compute write state.
 *
 * Multi-layer and multi-mip images are downloaded at layer 0, mip 0 only.
 * Array and mipmap export is a separate concern.
 *
 * @param image  Fully-initialized VKImage to read from.
 * @return       Populated ImageData, or std::nullopt on failure.
 */
[[nodiscard]] std::optional<ImageData> download_image(
    const std::shared_ptr<Core::VKImage>& image);

/**
 * @brief Download a TextureBuffer's GPU texture into host ImageData.
 *
 * Convenience wrapper over download_image() that pulls the VKImage from
 * TextureBuffer::get_gpu_texture(). Returns nullopt if the texture is not
 * yet allocated (buffer has not been processed at least once).
 */
[[nodiscard]] std::optional<ImageData> download_texture_buffer(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer);

/**
 * @brief Save a VKImage directly to disk via the ImageWriter registry.
 *
 * Combines download_image() with ImageWriterRegistry::create_writer(). The
 * file extension selects the writer. Format-variant compatibility is the
 * writer's responsibility: saving a float VKImage as PNG will fail at the
 * writer (uint8 mismatch), saving a uint8 VKImage as EXR will fail too.
 *
 * @param image    VKImage to save.
 * @param filepath Destination path with extension.
 * @param options  Format-specific writer options.
 * @return         True on success.
 */
bool save_image(
    const std::shared_ptr<Core::VKImage>& image,
    const std::string& filepath,
    const ImageWriteOptions& options = {});

/**
 * @brief Save a TextureBuffer's current GPU state to disk.
 *
 * Equivalent to download_texture_buffer() + save_image(). Most common entry
 * point for writing rendered output to a file.
 */
bool save_texture_buffer(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer,
    const std::string& filepath,
    const ImageWriteOptions& options = {});

/**
 * @brief Save a TextBuffer's rendered glyph texture to disk.
 *
 * TextBuffer inherits from TextureBuffer, so this is a convenience alias
 * with type-specific documentation. The RGBA8 coverage texture is written
 * to disk with transparency preserved when the target format supports it
 * (PNG, TGA, EXR). JPG discards alpha.
 */
bool save_text_buffer(
    const std::shared_ptr<Buffers::TextBuffer>& buffer,
    const std::string& filepath,
    const ImageWriteOptions& options = {});

} // namespace MayaFlux::IO
