#pragma once

#include "MayaFlux/IO/VolumeWriter.hpp"

namespace MayaFlux::Buffers {
class VolumeGridBuffer;
}

namespace MayaFlux::IO {

/**
 * @brief Download named fields from a GPU-resident volume into host VolumeData.
 *
 * Performs one blocking GPU->host transfer per field via
 * VolumeGridBuffer::read_field, reading the current read slot of each. The
 * calling thread must have command queue access, and the call costs a full
 * round trip at each field's byte size. Not for per-frame use on the
 * graphics thread.
 *
 * Fields with stride sizeof(float) land as scalars. Fields with stride
 * sizeof(glm::vec4) are read into scratch storage and gathered into
 * glm::vec3, discarding the padding component the GPU layout requires and
 * nothing reads. That gather costs a second allocation at four thirds the
 * output size, which is the price of doing the pack on the host.
 *
 * Any other stride is unrepresentable in VolumeData and is skipped with an
 * error. A named field that was never declared is likewise skipped.
 *
 * @param volume      Volume to read from.
 * @param field_names Fields to download. Empty means every declared field,
 *                    in declaration order.
 * @return Populated VolumeData, or std::nullopt if nothing was downloaded
 *         or the result failed is_consistent().
 */
[[nodiscard]] std::optional<Kakshya::VolumeData> download_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::vector<std::string>& field_names = {});

/**
 * @brief Save a volume directly to disk via the VolumeWriter registry.
 *
 * Combines download_volume() with VolumeWriterRegistry::create_writer(). The
 * file extension selects the writer. Whether a given writer can express a
 * given field is the writer's responsibility: a format with no vector grid
 * type rejects a vector field rather than dropping it.
 *
 * Inherits download_volume's thread requirements.
 *
 * @param volume      Volume to save.
 * @param filepath    Destination path with extension.
 * @param options     Format-specific writer options.
 * @param field_names Fields to save. Empty means every declared field.
 * @return True on success.
 */
bool save_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::string& filepath,
    const VolumeWriteOptions& options = {},
    const std::vector<std::string>& field_names = {});

/**
 * @brief Save already-downloaded VolumeData to disk via the registry.
 *
 * Pure CPU, no thread restrictions. For callers holding a VolumeData from
 * download_volume, from a future reader, or built procedurally.
 *
 * ImageExport has no equivalent because IOManager::save_image(ImageData)
 * covers that case asynchronously. A synchronous data-to-file path is worth
 * having here: it is what a test exercising a writer calls, and what a
 * shutdown flush calls.
 */
bool save_volume(
    const Kakshya::VolumeData& data,
    const std::string& filepath,
    const VolumeWriteOptions& options = {});

} // namespace MayaFlux::IO
