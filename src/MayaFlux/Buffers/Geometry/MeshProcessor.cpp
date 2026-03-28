#include "MeshProcessor.hpp"
#include "MeshBuffer.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

MeshProcessor::MeshProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

// =============================================================================
// on_attach
// =============================================================================

void MeshProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_buffer_service) {
        m_buffer_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::BufferService>();
    }

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "MeshProcessor requires a valid BufferService");
    }

    auto mesh_buf = std::dynamic_pointer_cast<MeshBuffer>(buffer);
    if (!mesh_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshProcessor: attached to non-MeshBuffer — detaching");
        return;
    }

    if (!mesh_buf->m_mesh_data.is_valid()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshProcessor: MeshData is not valid on attach — no GPU resources allocated");
        return;
    }

    m_mesh_buffer = mesh_buf;

    if (!mesh_buf->is_initialized()) {
        m_buffer_service->initialize_buffer(mesh_buf);
    }

    allocate_gpu_buffers(mesh_buf);
    upload_vertices(mesh_buf);
    upload_indices(mesh_buf);
    link_index_resources();

    mesh_buf->clear_vertices_dirty();
    mesh_buf->clear_indices_dirty();

    mesh_buf->set_vertex_layout(mesh_buf->m_mesh_data.layout);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "MeshProcessor: initialised — {} vertices, {} indices ({} faces), stride {}",
        mesh_buf->get_vertex_count(),
        mesh_buf->get_index_count(),
        mesh_buf->get_face_count(),
        mesh_buf->m_mesh_data.layout.stride_bytes);
}

void MeshProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_mesh_buffer.reset();
    m_gpu_index_buffer.reset();
    m_vertex_staging.reset();
    m_index_staging.reset();
}

// =============================================================================
// processing_function — per-cycle dirty check
// =============================================================================

void MeshProcessor::processing_function(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (!m_mesh_buffer) {
        return;
    }

    if (m_mesh_buffer->vertices_dirty()) {
        upload_vertices(m_mesh_buffer);
        m_mesh_buffer->clear_vertices_dirty();

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshProcessor: re-uploaded vertex data ({} bytes)",
            m_mesh_buffer->get_size_bytes());
    }

    if (m_mesh_buffer->indices_dirty()) {
        upload_indices(m_mesh_buffer);
        link_index_resources();
        m_mesh_buffer->clear_indices_dirty();

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshProcessor: re-uploaded index data ({} indices)",
            m_mesh_buffer->get_index_count());
    }
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshProcessor::allocate_gpu_buffers(const std::shared_ptr<MeshBuffer>& buf)
{
    const auto* vb = std::get_if<std::vector<uint8_t>>(
        &buf->m_mesh_data.vertex_variant);
    const auto* ib = std::get_if<std::vector<uint32_t>>(
        &buf->m_mesh_data.index_variant);

    if (!vb || vb->empty() || !ib || ib->empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshProcessor::allocate_gpu_buffers: empty vertex or index data");
        return;
    }

    if (!buf->is_host_visible()) {
        m_vertex_staging = create_staging_buffer(vb->size());
    }

    const size_t idx_bytes = ib->size() * sizeof(uint32_t);
    m_gpu_index_buffer = std::make_shared<VKBuffer>(
        idx_bytes,
        VKBuffer::Usage::INDEX,
        Kakshya::DataModality::UNKNOWN);
    m_buffer_service->initialize_buffer(m_gpu_index_buffer);
    m_index_staging = create_staging_buffer(idx_bytes);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "MeshProcessor: allocated vertex buffer ({} bytes) + index buffer ({} bytes)",
        vb->size(), idx_bytes);
}

void MeshProcessor::upload_vertices(const std::shared_ptr<MeshBuffer>& buf)
{
    const auto* vb = std::get_if<std::vector<uint8_t>>(
        &buf->m_mesh_data.vertex_variant);
    if (!vb || vb->empty()) {
        return;
    }

    upload_resizing(
        vb->data(),
        vb->size(),
        std::dynamic_pointer_cast<VKBuffer>(buf),
        buf->is_host_visible() ? nullptr : m_vertex_staging);
}

void MeshProcessor::upload_indices(const std::shared_ptr<MeshBuffer>& buf)
{
    const auto* ib = std::get_if<std::vector<uint32_t>>(
        &buf->m_mesh_data.index_variant);
    if (!ib || ib->empty() || !m_gpu_index_buffer) {
        return;
    }

    upload_resizing(
        ib->data(),
        ib->size() * sizeof(uint32_t),
        m_gpu_index_buffer,
        m_index_staging);
}

void MeshProcessor::link_index_resources()
{
    if (!m_mesh_buffer || !m_gpu_index_buffer) {
        return;
    }

    m_mesh_buffer->set_index_resources(
        m_gpu_index_buffer->get_buffer(),
        m_gpu_index_buffer->get_buffer_resources().memory,
        m_gpu_index_buffer->get_size_bytes());
}

} // namespace MayaFlux::Buffers
