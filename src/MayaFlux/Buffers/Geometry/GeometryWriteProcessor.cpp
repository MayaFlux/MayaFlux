#include "GeometryWriteProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

GeometryWriteProcessor::GeometryWriteProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void GeometryWriteProcessor::set_data(Kakshya::DataVariant variant)
{
    m_pending = std::move(variant);
    m_dirty.test_and_set(std::memory_order_release);
}

bool GeometryWriteProcessor::has_pending() const noexcept
{
    return m_dirty.test(std::memory_order_acquire);
}

void GeometryWriteProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GeometryWriteProcessor requires a VKBuffer");
    }

    if (!vk->is_host_visible()) {
        m_staging = create_staging_buffer(vk->get_size_bytes());
    }
}

void GeometryWriteProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_staging.reset();
    m_active.reset();
    m_pending.reset();
}

void GeometryWriteProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GeometryWriteProcessor attached to non-VKBuffer");
        return;
    }

    if (!m_dirty.test(std::memory_order_acquire)) {
        return;
    }

    m_dirty.clear(std::memory_order_release);
    std::swap(m_active, m_pending);

    if (!m_active.has_value()) {
        return;
    }

    auto access = (m_mode == GeometryWriteMode::LINE)
        ? Kakshya::as_line_vertex_access(*m_active, m_config)
        : Kakshya::as_point_vertex_access(*m_active, m_config);

    if (!access.has_value()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GeometryWriteProcessor: as_vertex_access returned nullopt — unsupported variant type");
        return;
    }

    upload_resizing(access->data_ptr, access->byte_count, vk, m_staging);

    vk->set_vertex_layout(access->layout);
}

} // namespace MayaFlux::Buffers
