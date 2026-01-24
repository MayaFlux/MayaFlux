#include "BufferDownloadProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "StagingUtils.hpp"

namespace MayaFlux::Buffers {

BufferDownloadProcessor::BufferDownloadProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

BufferDownloadProcessor::~BufferDownloadProcessor()
{
    m_staging_buffers.clear();
    m_target_map.clear();
}

void BufferDownloadProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "BufferDownloadProcessor requires VKBuffer");
        return;
    }

    if (!vk_buffer->is_initialized()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VKBuffer not initialized - register with BufferManager first");
        return;
    }

    auto target_it = m_target_map.find(buffer);
    if (target_it == m_target_map.end() || !target_it->second) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "BufferDownloadProcessor has no target configured for this buffer");
        return;
    }

    if (vk_buffer->is_host_visible()) {
        download_host_visible(vk_buffer);
    } else {
        download_device_local(vk_buffer);
    }
}

void BufferDownloadProcessor::download_host_visible(const std::shared_ptr<VKBuffer>& source)
{
    auto target_it = m_target_map.find(source);
    if (target_it == m_target_map.end() || !target_it->second) {
        return;
    }
    auto target = target_it->second;

    Buffers::download_host_visible(source, std::dynamic_pointer_cast<VKBuffer>(target));
}

void BufferDownloadProcessor::download_device_local(const std::shared_ptr<VKBuffer>& source)
{
    auto target_it = m_target_map.find(source);
    if (target_it == m_target_map.end() || !target_it->second) {
        return;
    }
    auto target = target_it->second;

    ensure_staging_buffer(source);

    auto staging_buffer = m_staging_buffers[source];

    Buffers::download_device_local(source, std::dynamic_pointer_cast<VKBuffer>(target), staging_buffer);
}

void BufferDownloadProcessor::ensure_staging_buffer(const std::shared_ptr<VKBuffer>& source)
{
    auto it = m_staging_buffers.find(source);
    if (it != m_staging_buffers.end() && it->second->get_size_bytes() >= source->get_size_bytes() && it->second->is_initialized()) {
        return;
    }

    auto staging_buffer = std::make_shared<VKBuffer>(
        source->get_size_bytes(),
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

    m_staging_buffers[source] = staging_buffer;

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Created staging buffer for download: {} bytes", staging_buffer->get_size_bytes());
}

void BufferDownloadProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!is_compatible_with(buffer)) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "BufferDownloadProcessor can only be attached to VKBuffer");
    }

    if (!m_buffer_service) {
        m_buffer_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::BufferService>();
    }

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "No processing context available for BufferDownloadProcessor");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "BufferDownloadProcessor attached");
}

void BufferDownloadProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    m_staging_buffers.erase(buffer);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "BufferDownloadProcessor detached");
}

bool BufferDownloadProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
}

void BufferDownloadProcessor::configure_target(const std::shared_ptr<Buffer>& source, std::shared_ptr<Buffer> target)
{
    if (!std::dynamic_pointer_cast<VKBuffer>(source)) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Source must be a VKBuffer");
    }

    m_target_map[source] = std::move(target);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Configured download target for source buffer");
}

void BufferDownloadProcessor::remove_target(const std::shared_ptr<Buffer>& source)
{
    m_target_map.erase(source);
}

std::shared_ptr<Buffer> BufferDownloadProcessor::get_target(const std::shared_ptr<Buffer>& source) const
{
    auto it = m_target_map.find(source);
    return it != m_target_map.end() ? it->second : nullptr;
}

} // namespace MayaFlux::Buffers
