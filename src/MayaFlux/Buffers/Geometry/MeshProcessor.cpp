#include "MeshProcessor.hpp"
#include "MeshBuffer.hpp"

#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

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

    m_mesh_buffer = mesh_buf;
    m_layout_count.reset();

    ensure_initialized(mesh_buf);

    upload_pending(mesh_buf);

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
    m_frame.reset();
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

    if (auto node = m_mesh_buffer->get_node()) {
        if (!node->get_mesh_vertices().empty()) {
            const auto& verts = node->get_mesh_vertices();
            const auto& indices = node->get_mesh_indices();
            m_mesh_buffer->m_mesh_data.vertex_variant = std::vector<uint8_t>(
                reinterpret_cast<const uint8_t*>(verts.data()),
                reinterpret_cast<const uint8_t*>(verts.data() + verts.size()));
            m_mesh_buffer->m_mesh_data.index_variant = indices;
            m_mesh_buffer->m_mesh_data.faces = node->get_mesh_faces();
            m_mesh_buffer->m_mesh_data.submeshes = node->get_mesh_submeshes();
            m_mesh_buffer->m_mesh_data.layout.vertex_count = static_cast<uint32_t>(verts.size());
            m_mesh_buffer->m_vertices_dirty.store(true, std::memory_order_release);
            m_mesh_buffer->m_indices_dirty.store(true, std::memory_order_release);
            m_mesh_buffer->m_rest_dirty.store(true, std::memory_order_release);
        }
    }

    if (auto frame = m_mesh_buffer->current_frame(); frame != m_frame) {
        m_frame = std::move(frame);
        adopt_frame(m_frame);
    }

    if (m_mesh_buffer->m_rest_dirty.exchange(false, std::memory_order_acq_rel)) {
        m_mesh_buffer->update_skin_rest();
    }

    if (m_mesh_buffer->restores_vertices()) {
        m_mesh_buffer->m_vertices_dirty.store(true, std::memory_order_release);
    }

    upload_pending(m_mesh_buffer);
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshProcessor::adopt_frame(const std::shared_ptr<const Kakshya::MeshData>& frame)
{
    auto& data = m_mesh_buffer->m_mesh_data;

    if (!frame) {
        data = Kakshya::MeshData::empty();
        m_mesh_buffer->m_vertices_dirty.store(true, std::memory_order_release);
        m_mesh_buffer->m_indices_dirty.store(true, std::memory_order_release);
        return;
    }

    const auto* incoming = std::get_if<std::vector<uint32_t>>(&frame->index_variant);
    const auto* current = std::get_if<std::vector<uint32_t>>(&data.index_variant);
    const bool same_indices = incoming && current && *incoming == *current;

    data.vertex_variant = frame->vertex_variant;
    data.layout = frame->layout;
    data.submeshes = frame->submeshes;
    data.faces = frame->faces;
    m_mesh_buffer->m_vertices_dirty.store(true, std::memory_order_release);
    m_mesh_buffer->m_rest_dirty.store(true, std::memory_order_release);

    if (!same_indices) {
        data.index_variant = frame->index_variant;
        m_mesh_buffer->m_indices_dirty.store(true, std::memory_order_release);
    }
}

void MeshProcessor::upload_pending(const std::shared_ptr<MeshBuffer>& buf)
{
    if (buf->m_mesh_data.is_valid() && !m_gpu_index_buffer) {
        allocate_gpu_buffers(buf);
        buf->m_vertices_dirty.store(true, std::memory_order_release);
        buf->m_indices_dirty.store(true, std::memory_order_release);
    }

    if (buf->vertices_dirty()) {
        upload_vertices(buf);
        buf->clear_vertices_dirty();
    }

    if (buf->indices_dirty()) {
        upload_indices(buf);
        buf->clear_indices_dirty();
    }

    link_index_resources();
    sync_layout(buf);
}

void MeshProcessor::sync_layout(const std::shared_ptr<MeshBuffer>& buf)
{
    const uint32_t drawn = buf->m_mesh_data.is_valid() ? buf->m_mesh_data.vertex_count() : 0U;

    if (m_layout_count == drawn) {
        return;
    }

    auto layout = buf->m_mesh_data.layout;
    layout.vertex_count = drawn;
    buf->set_vertex_layout(layout);
    m_layout_count = drawn;
}

void MeshProcessor::allocate_gpu_buffers(const std::shared_ptr<MeshBuffer>& buf)
{
    const auto* vb = std::get_if<std::vector<uint8_t>>(
        &buf->m_mesh_data.vertex_variant);
    const auto* ib = std::get_if<std::vector<uint32_t>>(
        &buf->m_mesh_data.index_variant);

    if (!vb || vb->empty() || !ib || ib->empty()) {
        return;
    }

    if (!m_vertex_staging && !buf->is_host_visible()) {
        m_vertex_staging = create_staging_buffer(vb->size());
    }

    const size_t idx_bytes = ib->size() * sizeof(uint32_t);
    m_gpu_index_buffer = std::make_shared<VKBuffer>(
        idx_bytes,
        VKBuffer::Usage::INDEX,
        Kakshya::DataModality::UNKNOWN);

    ensure_initialized(m_gpu_index_buffer);
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
        static_cast<size_t>(m_mesh_buffer->get_index_count()) * sizeof(uint32_t));
}

} // namespace MayaFlux::Buffers
