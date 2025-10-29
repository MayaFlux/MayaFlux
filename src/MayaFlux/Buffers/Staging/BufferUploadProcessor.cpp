#include "BufferUploadProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"

#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "vulkan/vulkan.hpp"

namespace MayaFlux::Buffers {

BufferUploadProcessor::BufferUploadProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void BufferUploadProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "BufferUploadProcessor requires VKBuffer");
        return;
    }

    if (!vk_buffer->is_initialized()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VKBuffer not initialized - register with BufferManager first");
        return;
    }

    auto source_it = m_source_map.find(buffer);
    if (source_it == m_source_map.end() || !source_it->second) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "BufferUploadProcessor has no source configured for this buffer");
        return;
    }

    auto source = source_it->second;
    auto source_data = std::dynamic_pointer_cast<VKBuffer>(source)->get_data();
    if (source_data.empty()) {
        MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Source buffer has no data to upload");
        return;
    }

    if (vk_buffer->is_host_visible()) {
        upload_host_visible(vk_buffer, source_data[0]);
    } else {
        upload_device_local(vk_buffer, source_data[0]);
    }
}

void BufferUploadProcessor::upload_host_visible(const std::shared_ptr<VKBuffer>& target, const Kakshya::DataVariant& data)
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

    auto dirty_ranges = target->get_and_clear_dirty_ranges();
    for (auto& [offset, size] : dirty_ranges) {
        m_buffer_service->flush_range(
            target_resources.memory,
            offset,
            size);
    }
}

void BufferUploadProcessor::upload_device_local(const std::shared_ptr<VKBuffer>& target, const Kakshya::DataVariant& data)
{
    ensure_staging_buffer(target);

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

    auto staging_buffer = m_staging_buffers[target];
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

    auto dirty_ranges = staging_buffer->get_and_clear_dirty_ranges();
    for (auto& [offset, size] : dirty_ranges) {
        m_buffer_service->flush_range(
            staging_resources.memory,
            offset,
            size);
    }

    m_buffer_service->execute_immediate([&](void* ptr) {
        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = bytes;

        auto cmd = static_cast<vk::CommandBuffer*>(ptr);

        cmd->copyBuffer(
            staging_buffer->get_buffer(),
            target->get_buffer(),
            1, &copy_region);
    });
}

void BufferUploadProcessor::ensure_staging_buffer(const std::shared_ptr<VKBuffer>& target)
{
    auto it = m_staging_buffers.find(target);
    if (it != m_staging_buffers.end() && it->second->get_size_bytes() >= target->get_size_bytes() && it->second->is_initialized()) {
        return;
    }

    auto staging_buffer = std::make_shared<VKBuffer>(
        target->get_size_bytes(),
        VKBuffer::Usage::STAGING,
        Kakshya::DataModality::UNKNOWN);

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "No processing context available for staging buffer initialization");
    }

    if (!staging_buffer->is_initialized()) {
        try {
            m_buffer_service->initialize_buffer(staging_buffer);
        } catch (const std::exception& e) {
            error_rethrow(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "Failed to initialize staging buffer: {}", e.what());
        }
    }

    m_staging_buffers[target] = staging_buffer;

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Created staging buffer: {} bytes", staging_buffer->get_size_bytes());
}

void BufferUploadProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    if (!is_compatible_with(buffer)) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "BufferUploadProcessor can only be attached to VKBuffer");
    }

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "BufferUploadProcessor requires a valid buffer service");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "BufferUploadProcessor attached to buffer");
}

void BufferUploadProcessor::on_detach(std::shared_ptr<Buffer> buffer)
{
    m_staging_buffers.erase(buffer);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "BufferUploadProcessor detached from buffer");
}

bool BufferUploadProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
}

void BufferUploadProcessor::configure_source(const std::shared_ptr<Buffer>& target, std::shared_ptr<Buffer> source)
{
    if (!std::dynamic_pointer_cast<VKBuffer>(target)) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Target must be a VKBuffer");
    }

    m_source_map[target] = std::move(source);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Configured upload source for target buffer");
}

void BufferUploadProcessor::remove_source(const std::shared_ptr<Buffer>& target)
{
    m_source_map.erase(target);
}

std::shared_ptr<Buffer> BufferUploadProcessor::get_source(const std::shared_ptr<Buffer>& target) const
{
    auto it = m_source_map.find(target);
    return it != m_source_map.end() ? it->second : nullptr;
}

}
