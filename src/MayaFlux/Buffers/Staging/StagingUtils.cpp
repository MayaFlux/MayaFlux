#include "StagingUtils.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

void upload_host_visible(const std::shared_ptr<VKBuffer>& target, const Kakshya::DataVariant& data)
{
    Kakshya::DataAccess accessor(
        const_cast<Kakshya::DataVariant&>(data),
        {},
        target->get_modality());

    auto [ptr, bytes, format_hint] = accessor.gpu_buffer();

    if (bytes > target->get_size_bytes()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Upload data size {} exceeds buffer capacity {}",
            bytes, target->get_size_bytes());
    }

    auto& target_resources = target->get_buffer_resources();
    void* mapped = target_resources.mapped_ptr;
    if (!mapped) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Host-visible buffer has no mapped pointer");
    }

    std::memcpy(mapped, ptr, bytes);

    target->mark_dirty_range(0, bytes);

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "upload_host_visible requires a valid buffer service");
    }

    auto dirty_ranges = target->get_and_clear_dirty_ranges();
    for (auto& [offset, size] : dirty_ranges) {
        buffer_service->flush_range(
            target_resources.memory,
            offset,
            size);
    }
}

void upload_device_local(const std::shared_ptr<VKBuffer>& target, const std::shared_ptr<VKBuffer>& staging_buffer, const Kakshya::DataVariant& data)
{
    Kakshya::DataAccess accessor(
        const_cast<Kakshya::DataVariant&>(data),
        {},
        target->get_modality());

    auto [ptr, bytes, format_hint] = accessor.gpu_buffer();

    if (bytes > target->get_size_bytes()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Upload data size {} exceeds buffer capacity {}",
            bytes, target->get_size_bytes());
    }

    auto& staging_resources = staging_buffer->get_buffer_resources();

    void* staging_mapped = staging_resources.mapped_ptr;
    if (!staging_mapped) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Staging buffer has no mapped pointer");
    }

    std::memcpy(staging_mapped, ptr, bytes);
    staging_buffer->mark_dirty_range(0, bytes);

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "upload_host_visible requires a valid buffer service");
    }

    auto dirty_ranges = staging_buffer->get_and_clear_dirty_ranges();
    for (auto& [offset, size] : dirty_ranges) {
        buffer_service->flush_range(
            staging_resources.memory,
            offset,
            size);
    }

    buffer_service->execute_immediate([&](void* ptr) {
        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = bytes;

        vk::CommandBuffer cmd(static_cast<VkCommandBuffer>(ptr));

        cmd.copyBuffer(
            staging_buffer->get_buffer(),
            target->get_buffer(),
            1, &copy_region);
    });
}

void download_host_visible(const std::shared_ptr<VKBuffer>& source, const std::shared_ptr<VKBuffer>& target)
{
    auto& source_resources = source->get_buffer_resources();
    void* mapped = source_resources.mapped_ptr;
    if (!mapped) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Host-visible buffer has no mapped pointer");
    }

    source->mark_invalid_range(0, source->get_size_bytes());

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "upload_host_visible requires a valid buffer service");
    }

    auto invalid_ranges = source->get_and_clear_invalid_ranges();
    for (auto& [offset, size] : invalid_ranges) {
        buffer_service->invalidate_range(
            source_resources.memory,
            offset,
            size);
    }

    std::vector<uint8_t> raw_bytes(source->get_size_bytes());
    std::memcpy(raw_bytes.data(), mapped, source->get_size_bytes());

    std::dynamic_pointer_cast<VKBuffer>(target)->set_data({ raw_bytes });
}

void download_device_local(const std::shared_ptr<VKBuffer>& source, const std::shared_ptr<VKBuffer>& target, const std::shared_ptr<VKBuffer>& staging_buffer)
{
    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "upload_host_visible requires a valid buffer service");
    }

    buffer_service->execute_immediate([&](void* ptr) {
        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = source->get_size_bytes();

        auto cmd = static_cast<vk::CommandBuffer*>(ptr);

        cmd->copyBuffer(
            source->get_buffer(),
            staging_buffer->get_buffer(),
            1, &copy_region);
    });

    staging_buffer->mark_invalid_range(0, source->get_size_bytes());

    auto& staging_resources = staging_buffer->get_buffer_resources();
    auto invalid_ranges = staging_buffer->get_and_clear_invalid_ranges();
    for (auto& [offset, size] : invalid_ranges) {
        buffer_service->invalidate_range(
            staging_resources.memory,
            offset,
            size);
    }

    void* staging_mapped = staging_resources.mapped_ptr;
    if (!staging_mapped) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Staging buffer has no mapped pointer");
    }

    std::vector<uint8_t> raw_bytes(source->get_size_bytes());
    std::memcpy(raw_bytes.data(), staging_mapped, source->get_size_bytes());

    std::dynamic_pointer_cast<VKBuffer>(target)->set_data({ raw_bytes });
}

bool is_device_local(const std::shared_ptr<VKBuffer>& buffer)
{
    return buffer && !buffer->is_host_visible();
}

std::shared_ptr<VKBuffer> create_staging_buffer(size_t size)
{
    auto buffer = std::make_shared<VKBuffer>(
        size,
        VKBuffer::Usage::STAGING,
        Kakshya::DataModality::UNKNOWN);

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();
    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "create_staging_buffer requires a valid buffer service");
    }

    buffer_service->initialize_buffer(buffer);

    return buffer;
}

std::shared_ptr<VKBuffer> create_image_staging_buffer(size_t size)
{
    auto buf = std::make_shared<VKBuffer>(
        size,
        VKBuffer::Usage::STAGING,
        Kakshya::DataModality::IMAGE_COLOR);

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "create_image_staging_buffer requires a valid buffer service");
    }

    buffer_service->initialize_buffer(buf);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "create_image_staging_buffer: allocated {} bytes", size);

    return buf;
}

void upload_to_gpu(
    const void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging)
{
    if (!target) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "upload_to_gpu: target buffer is null");
    }

    if (size == 0) {
        return;
    }

    std::vector<uint8_t> raw_bytes(size);
    std::memcpy(raw_bytes.data(), data, size);
    Kakshya::DataVariant data_variant(raw_bytes);

    if (target->is_host_visible()) {
        upload_host_visible(target, data_variant);
    } else {
        std::shared_ptr<VKBuffer> staging_buf = staging;

        if (!staging_buf) {
            staging_buf = create_staging_buffer(size);
        }

        upload_device_local(target, staging_buf, data_variant);
    }
}

void download_from_gpu(
    const std::shared_ptr<VKBuffer>& source,
    void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& staging)
{
    if (!source) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu: source buffer is null");
    }

    if (size == 0) {
        return;
    }

    auto temp_target = std::make_shared<VKBuffer>(
        size, VKBuffer::Usage::STAGING, Kakshya::DataModality::UNKNOWN);

    if (source->is_host_visible()) {
        download_host_visible(source, temp_target);
    } else {
        std::shared_ptr<VKBuffer> staging_buf = staging;

        if (!staging_buf) {
            staging_buf = create_staging_buffer(size);
        }

        download_device_local(source, temp_target, staging_buf);
    }

    auto temp_data = temp_target->get_data();

    if (temp_data.empty()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu: failed to retrieve data from temporary buffer");
    }

    if (temp_data.size() > 1) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "download_from_gpu: unexpected multiple data variants in temporary buffer. Only the first will be used.");
    }

    Kakshya::DataAccess accessor(
        const_cast<Kakshya::DataVariant&>(temp_data[0]),
        {},
        source->get_modality());

    auto [ptr, bytes, format_hint] = accessor.gpu_buffer();

    std::memcpy(data, ptr, std::min(size, bytes));
}

void upload_audio_to_gpu(
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    const std::shared_ptr<VKBuffer>& staging)
{
    if (gpu_buffer->get_format() != vk::Format::eR64Sfloat && gpu_buffer->get_format() != vk::Format::eUndefined) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GPU buffer format is {} but audio requires R64Sfloat for double precision. "
            "Create VKBuffer with DataModality::AUDIO_1D or AUDIO_MULTICHANNEL.",
            vk::to_string(gpu_buffer->get_format()));
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GPU buffer format mismatch for audio upload");
    }

    auto& audio_data = audio_buffer->get_data();

    if (audio_data.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioBuffer contains no data to upload");
        return;
    }

    const void* data_ptr = audio_data.data();
    size_t data_bytes = audio_data.size() * sizeof(double);

    upload_to_gpu(data_ptr, data_bytes, gpu_buffer, staging);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Uploaded {} bytes of double-precision audio to GPU", data_bytes);
}

void download_audio_from_gpu(
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& staging)
{
    Kakshya::DataVariant downloaded_data;

    auto dimensions = std::vector<Kakshya::DataDimension> {
        Kakshya::DataDimension::time(
            gpu_buffer->get_size_bytes() / sizeof(double),
            "samples")
    };
    Kakshya::DataAccess accessor = download_to_view<double>(
        gpu_buffer, downloaded_data, dimensions,
        Kakshya::DataModality::AUDIO_1D, staging);

    auto double_view = accessor.view<double>();

    audio_buffer->get_data() = std::vector<double>(double_view.begin(), double_view.end());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Downloaded {} samples of double-precision audio from GPU", double_view.size());
}

} // namespace MayaFlux::Buffers
