#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class DataReadProcessor;
}

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Kriya::detail {

/**
 * @brief Arm the read that suits what the buffer is.
 *
 * A texture buffer that retains host pixels, such as a video or camera buffer,
 * reads them without touching the GPU; a texture buffer without them and a node
 * texture read their GPU image, which blocks the graphics cycle for the
 * transfer. Mesh and geometry sources read their mesh,
 * network geometry reads its declared vertices, and any other buffer reads its
 * primary storage.
 */
void arm_graphics_read(Buffers::DataReadProcessor& reader, const std::shared_ptr<Buffers::VKBuffer>& buffer);

/**
 * @brief Finish the pending read as one DataVariant.
 *
 * Pixels yield their typed storage, a mesh yields its interleaved vertex bytes,
 * and any other read yields raw bytes. Returns nullopt while nothing is ready.
 */
std::optional<Kakshya::DataVariant> resolve_graphics_read(Buffers::DataReadProcessor& reader);

/**
 * @brief Whether a DataVariant can be written to the buffer's primary storage.
 *
 * False for image, mesh, geometry, network and volume buffers: their primary
 * storage is regenerated from another source or is not the data a caller means,
 * so a raw write would be lost or would corrupt it.
 */
bool accepts_raw_write(const std::shared_ptr<Buffers::VKBuffer>& buffer);

/**
 * @brief Whether the buffer holds its frames as layers that can be written one at a time.
 */
bool accepts_layer_write(const std::shared_ptr<Buffers::VKBuffer>& buffer);

/**
 * @brief Write a DataVariant into one layer of a layered buffer.
 *
 * An empty variant is skipped, as when nothing has arrived yet. The bytes must
 * match the layer's size, which the buffer reports when they do not.
 *
 * @param target Buffer that passed accepts_layer_write.
 * @param layer  Layer to replace.
 * @param data   Frame bytes.
 */
void write_layer(
    const std::shared_ptr<Buffers::VKBuffer>& target,
    uint32_t layer,
    const Kakshya::DataVariant& data);

/**
 * @brief Write a GPU image into one layer of a layered buffer, with no host read.
 *
 * @param target Buffer that passed accepts_layer_write.
 * @param layer  Layer to replace.
 * @param image  Source image; the buffer fits it to the layer.
 */
void write_layer(
    const std::shared_ptr<Buffers::VKBuffer>& target,
    uint32_t layer,
    const std::shared_ptr<Core::VKImage>& image);

/**
 * @brief The GPU image a buffer displays, or null when it has none to copy from.
 */
std::shared_ptr<Core::VKImage> source_image(const std::shared_ptr<Buffers::VKBuffer>& buffer);

/**
 * @brief Make a buffer draw to the window in a render config, unless it already does.
 *
 * A buffer that already has a render processor is left as it is; a different
 * target window than the one requested is reported. Otherwise the config is
 * applied through the buffer's own setup_rendering, chosen by buffer type: the
 * texture, geometry, mesh, network, grid and Forma families are covered. Any
 * other buffer is reported and left untouched.
 *
 * @return True if the buffer renders after the call.
 */
bool ensure_rendering(const std::shared_ptr<Buffers::VKBuffer>& buffer, const Portal::Graphics::RenderConfig& config);

}
