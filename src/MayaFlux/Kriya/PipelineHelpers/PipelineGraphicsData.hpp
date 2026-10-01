#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class DataReadProcessor;
}

namespace MayaFlux::Kriya::detail {

/**
 * @brief Arm the read that suits what the buffer is.
 *
 * Image buffers read their GPU image, mesh and geometry sources read their mesh,
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

}
