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
    m_pending_data = std::move(variant);
    m_data_dirty.test_and_set(std::memory_order_release);
}

void GeometryWriteProcessor::set_vertices(
    const void* data, size_t byte_count,
    const Kakshya::VertexLayout& layout)
{
    VertexSnapshot snap;
    snap.bytes.resize(byte_count);
    std::memcpy(snap.bytes.data(), data, byte_count);
    snap.layout = layout;

    m_pending_vertices = std::move(snap);
    m_vertices_dirty.test_and_set(std::memory_order_release);
}

bool GeometryWriteProcessor::has_pending() const noexcept
{
    return m_data_dirty.test(std::memory_order_acquire);
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
    m_active_data.reset();
    m_active_vertices.reset();
    m_pending_data.reset();
    m_pending_vertices.reset();
}

void GeometryWriteProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GeometryWriteProcessor attached to non-VKBuffer");
        return;
    }

    if (m_vertices_dirty.test(std::memory_order_acquire)) {
        m_vertices_dirty.clear(std::memory_order_release);
        std::swap(m_active_vertices, m_pending_vertices);

        if (m_active_vertices.has_value()) {
            const size_t required = m_active_vertices->bytes.size();
            const size_t available = vk->get_size_bytes();

            if (required > available) {
                vk->resize(static_cast<size_t>((float)required * 1.5F), false);
                if (m_staging) {
                    m_staging = create_staging_buffer(vk->get_size_bytes());
                }
            }

            upload_to_gpu(m_active_vertices->bytes.data(),
                std::min<size_t>(required, vk->get_size_bytes()),
                vk, m_staging);

            vk->set_vertex_layout(m_active_vertices->layout);
            return;
        }
    }

    if (!m_data_dirty.test(std::memory_order_acquire)) {
        return;
    }

    m_data_dirty.clear(std::memory_order_release);
    std::swap(m_active_data, m_pending_data);

    if (!m_active_data.has_value()) {
        return;
    }

    std::optional<Kakshya::VertexAccess> access;
    switch (m_mode) {
    case GeometryWriteMode::LINE:
        access = Kakshya::as_line_vertex_access(*m_active_data, m_config);
        break;
    case GeometryWriteMode::MESH:
        access = Kakshya::as_mesh_vertex_access(*m_active_data, m_config);
        break;
    case GeometryWriteMode::POINT:
        access = Kakshya::as_point_vertex_access(*m_active_data, m_config);
        break;
    }

    if (!access.has_value()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GeometryWriteProcessor: as_vertex_access returned nullopt — unsupported variant type");
        return;
    }

    upload_resizing(access->data_ptr, access->byte_count, vk, m_staging);

    vk->set_vertex_layout(access->layout);
}

} // namespace MayaFlux::Buffers
