#include "StagingUtils.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

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

    buffer_service->copy_buffer(
        static_cast<void*>(staging_buffer->get_buffer()),
        static_cast<void*>(target->get_buffer()),
        bytes, 0, 0);
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
            "download_device_local requires a valid buffer service");
    }

    buffer_service->copy_buffer(
        static_cast<void*>(source->get_buffer()),
        static_cast<void*>(staging_buffer->get_buffer()),
        source->get_size_bytes(), 0, 0);

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

void ensure_gpu_capacity(
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging,
    size_t required,
    float growth_factor)
{
    if (required <= target->get_size_bytes()) {
        return;
    }

    const auto new_size = static_cast<size_t>(static_cast<float>(required) * growth_factor);

    target->resize(new_size, false);

    if (staging) {
        staging->resize(new_size, false);
    }
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

void upload_resizing(
    const void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& target,
    const std::shared_ptr<VKBuffer>& staging,
    float growth_factor)
{
    ensure_gpu_capacity(target, staging, size, growth_factor);
    upload_to_gpu(data, size, target, staging);
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

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu requires a valid buffer service");
    }

    buffer_service->initialize_buffer(temp_target);

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

void download_from_gpu_async(
    const std::shared_ptr<VKBuffer>& source,
    void* data,
    size_t size,
    std::shared_ptr<VKBuffer>& staging)
{
    if (!source || !data || size == 0)
        return;

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service
        || !buffer_service->execute_fenced
        || !buffer_service->wait_fenced
        || !buffer_service->release_fenced
        || !buffer_service->invalidate_range) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu_async: BufferService unavailable");
    }

    if (!staging || staging->get_size_bytes() < size)
        staging = create_staging_buffer(size);

    auto handle = buffer_service->copy_buffer_fenced(
        static_cast<void*>(source->get_buffer()),
        static_cast<void*>(staging->get_buffer()),
        size, 0, 0);

    if (!handle) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu_async: execute_fenced returned null");
    }

    buffer_service->wait_fenced(handle);

    auto& resources = staging->get_buffer_resources();
    buffer_service->invalidate_range(resources.memory, 0, size);

    void* ptr = staging->get_mapped_ptr();
    if (!ptr) {
        buffer_service->release_fenced(handle);
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "download_from_gpu_async: staging buffer has no mapped pointer");
    }

    std::memcpy(data, ptr, size);
    buffer_service->release_fenced(handle);
}

std::span<const float> download_and_normalise(
    const std::shared_ptr<Core::VKImage>& image,
    std::vector<uint8_t>& raw_staging,
    std::vector<float>& work)
{
    if (!image || !image->is_initialized())
        return {};

    auto& loom = Portal::Graphics::TextureLoom::instance();

    const auto fmt_opt = loom.from_vulkan_format(image->get_format());
    if (!fmt_opt)
        return {};

    const size_t byte_size = static_cast<size_t>(image->get_width())
        * image->get_height()
        * loom.get_bytes_per_pixel(*fmt_opt);

    if (byte_size == 0)
        return {};

    raw_staging.resize(byte_size);
    loom.download_data(
        image, raw_staging.data(), byte_size, nullptr);

    Kakshya::DataVariant variant { raw_staging };

    return Kakshya::as_normalised_float(variant, work);
}

void upload_audio_to_gpu(
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    const std::shared_ptr<VKBuffer>& staging)
{
    const auto modality = gpu_buffer->get_modality();
    if (modality != Kakshya::DataModality::AUDIO_1D
        && modality != Kakshya::DataModality::AUDIO_MULTICHANNEL
        && modality != Kakshya::DataModality::UNKNOWN) {

        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GPU buffer modality is {} but audio requires AUDIO_1D or AUDIO_MULTICHANNEL. "
            "Create VKBuffer with DataModality::AUDIO_1D or AUDIO_MULTICHANNEL.",
            Kakshya::modality_to_string(modality));
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
