#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"

namespace MayaFlux::Buffers {

class VKBuffer;
class AudioBuffer;

/**
 * @brief Upload data to a host-visible buffer
 * @param target Target VKBuffer to upload data into
 * @param data DataVariant containing the data to upload
 *
 * This function handles uploading data from a Kakshya::DataVariant into a
 * host-visible VKBuffer. It maps the buffer memory, copies the data, and
 * marks the buffer as dirty for synchronization.
 */
MAYAFLUX_API void upload_host_visible(const std::shared_ptr<VKBuffer>& target, const Kakshya::DataVariant& data);

/**
 * @brief Upload data to a device-local buffer using a staging buffer
 * @param target Target VKBuffer to upload data into
 * @param staging_buffer Host-visible staging VKBuffer used for the upload
 * @param data DataVariant containing the data to upload
 *
 * This function handles uploading data from a Kakshya::DataVariant into a
 * device-local VKBuffer by utilizing a staging buffer. It copies the data
 * into the staging buffer, flushes it, and then issues a command to copy
 * from the staging buffer to the target device-local buffer.
 */
MAYAFLUX_API void upload_device_local(const std::shared_ptr<VKBuffer>& target, const std::shared_ptr<VKBuffer>& staging_buffer, const Kakshya::DataVariant& data);

/**
 * @brief Download data from a host-visible buffer
 * @param source Source VKBuffer to download data from
 * @param target Target VKBuffer to store the downloaded data
 *
 * This function handles downloading data from a host-visible VKBuffer.
 * It maps the buffer memory, copies the data into a CPU-accessible format,
 * and updates the associated target buffer.
 */
MAYAFLUX_API void download_host_visible(const std::shared_ptr<VKBuffer>& source, const std::shared_ptr<VKBuffer>& target);

/**
 * @brief Download data from a device-local buffer using a staging buffer
 * @param source Source VKBuffer to download data from
 * @param target Target VKBuffer to store the downloaded data
 * @param staging_buffer Host-visible staging VKBuffer used for the download
 *
 * This function handles downloading data from a device-local VKBuffer by
 * utilizing a staging buffer. It issues a command to copy the data from
 * the device-local buffer to the staging buffer, invalidates the staging
 * buffer memory, and then copies the data into a CPU-accessible format
 * to update the target buffer.
 */
MAYAFLUX_API void download_device_local(const std::shared_ptr<VKBuffer>& source, const std::shared_ptr<VKBuffer>& target, const std::shared_ptr<VKBuffer>& staging_buffer);

/**
 * @brief Upload raw data to GPU buffer (auto-detects host-visible vs device-local)
 * @param data Source data pointer
 * @param size Size in bytes
 * @param target Target GPU buffer
 * @param staging Optional staging buffer (created if needed for device-local)
 *
 * Convenience wrapper over StagingUtils that:
 * - Converts raw pointer → DataVariant
 * - Auto-detects if buffer is host-visible or device-local
 * - Handles staging buffer creation if needed
 */
MAYAFLUX_API void upload_to_gpu(
    const void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging = nullptr);

/**
 * @brief Upload typed data to GPU buffer
 * @tparam T Data type (float, double, int, etc.)
 * @param data Source data span
 * @param target Target GPU buffer
 * @param staging Optional staging buffer
 */
template <typename T>
void upload_to_gpu(
    std::span<const T> data,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    upload_to_gpu(data.data(), data.size_bytes(), target, staging);
}

/**
 * @brief Upload vector to GPU buffer
 * @tparam T Data type
 * @param data Source data vector
 * @param target Target GPU buffer
 * @param staging Optional staging buffer
 */
template <typename T>
void upload_to_gpu(
    const std::vector<T>& data,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    upload_to_gpu(std::span<const T>(data), target, staging);
}

/**
 * @brief Download from GPU buffer to raw data (auto-detects host-visible vs device-local)
 * @param source Source GPU buffer
 * @param data Destination data pointer
 * @param size Size in bytes
 * @param staging Optional staging buffer (created if needed for device-local)
 *
 * Convenience wrapper over StagingUtils that:
 * - Auto-detects if buffer is host-visible or device-local
 * - Handles staging buffer creation if needed
 * - Copies data to destination pointer
 */
MAYAFLUX_API void download_from_gpu(
    const std::shared_ptr<VKBuffer>& source,
    void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& staging = nullptr);

/**
 * @brief Download from GPU buffer to typed span
 * @tparam T Data type
 * @param source Source GPU buffer
 * @param data Destination data span
 * @param staging Optional staging buffer
 */
template <typename T>
void download_from_gpu(
    const std::shared_ptr<VKBuffer>& source,
    std::span<T> data,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    download_from_gpu(source, data.data(), data.size_bytes(), staging);
}

/**
 * @brief Download from GPU buffer to vector
 * @tparam T Data type
 * @param source Source GPU buffer
 * @param data Destination data vector (resized if needed)
 * @param staging Optional staging buffer
 */
template <typename T>
void download_from_gpu(
    const std::shared_ptr<VKBuffer>& source,
    std::vector<T>& data,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    size_t element_count = source->get_size_bytes() / sizeof(T);
    data.resize(element_count);
    download_from_gpu(source, std::span<T>(data), staging);
}

/**
 * @brief Create staging buffer for transfers
 * @param size Size in bytes
 * @return Host-visible staging buffer ready for transfers
 */
MAYAFLUX_API std::shared_ptr<VKBuffer> create_staging_buffer(size_t size);

/**
 * @brief Check if buffer is device-local (staging needed)
 * @param buffer Buffer to check
 * @return True if buffer is device-local
 */
MAYAFLUX_API bool is_device_local(const std::shared_ptr<VKBuffer>& buffer);

/**
 * @brief Upload data from DataAccess view to GPU buffer (precision-preserving)
 * @tparam T View type (double, glm::dvec2, glm::dvec3, glm::vec3, float, etc.)
 * @param accessor DataAccess instance providing the view
 * @param target Target GPU buffer
 * @param staging Optional staging buffer (auto-created if needed)
 *
 * Zero-copy when types match, automatic conversion cache when they don't.
 * For AUDIO modalities, defaults to DOUBLE precision to preserve accuracy.
 */
template <typename T>
void upload_from_view(
    const Kakshya::DataAccess& accessor,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    auto view = accessor.view<T>();

    const void* data_ptr = view.data();
    size_t data_bytes = view.size() * sizeof(T);

    if constexpr (std::is_same_v<T, double>) {
        if (target->get_format() != vk::Format::eR64Sfloat) {
            MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Uploading double precision to buffer with format {}. Consider using R64Sfloat for audio.",
                vk::to_string(target->get_format()));
        }
    }

    upload_to_gpu(data_ptr, data_bytes, target, staging);
}

/**
 * @brief Upload structured data with GLM double-precision types
 * @tparam T GLM type (glm::dvec2, glm::dvec3, glm::dvec4 for double precision)
 * @param accessor DataAccess with structured dimensions
 * @param target Target GPU buffer
 * @param staging Optional staging buffer
 *
 * Use this for high-precision structured data like audio samples interpreted
 * as multi-dimensional vectors. Supports both single and double precision GLM types.
 */
template <typename T>
    requires GlmType<T>
void upload_structured_view(
    const Kakshya::DataAccess& accessor,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    if (!accessor.is_structured()) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot upload structured view from non-structured data");
    }

    auto structured_view = accessor.view<T>();
    upload_to_gpu(structured_view.data(), structured_view.size_bytes(), target, staging);
}

/**
 * @brief Download GPU buffer to DataAccess-compatible format (precision-preserving)
 */
template <typename T>
Kakshya::DataAccess download_to_view(
    const std::shared_ptr<VKBuffer>& source,
    Kakshya::DataVariant& target_variant,
    const std::vector<Kakshya::DataDimension>& dimensions,
    Kakshya::DataModality modality,
    const std::shared_ptr<VKBuffer>& staging = nullptr)
{
    size_t element_count = source->get_size_bytes() / sizeof(T);

    std::vector<T> temp_buffer(element_count);
    download_from_gpu(source, temp_buffer, staging);

    target_variant = std::move(temp_buffer);

    return Kakshya::DataAccess(target_variant, dimensions, modality);
}

/**
 * @brief Upload AudioBuffer to GPU (always double precision)
 * @param audio_buffer Source audio buffer (double[])
 * @param gpu_buffer Target GPU buffer (must support R64Sfloat format)
 * @param staging Optional staging buffer (auto-created if needed)
 *
 * AudioBuffer is always double precision. This function ensures the GPU buffer
 * is configured for double precision and performs a direct upload with no conversion.
 *
 * @throws std::runtime_error if gpu_buffer doesn't support double precision
 */
void upload_audio_to_gpu(
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    const std::shared_ptr<VKBuffer>& staging = nullptr);

/**
 * @brief Download GPU buffer to AudioBuffer (expects double precision)
 * @param gpu_buffer Source GPU buffer (should contain double precision data)
 * @param audio_buffer Target audio buffer (always double[])
 * @param staging Optional staging buffer (auto-created if needed)
 *
 * Downloads GPU data and copies to AudioBuffer. If the GPU buffer contains
 * float data instead of double, DataAccess will handle the upconversion
 * (though this is not recommended for audio precision).
 */
void download_audio_from_gpu(
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& staging = nullptr);

} // namespace MayaFlux::Buffers
