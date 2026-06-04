#include "GeometryReadbackNode.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

GeometryReadbackNode::GeometryReadbackNode(
    std::shared_ptr<Buffers::VKBuffer> buffer,
    size_t vertex_count)
    : GeometryWriterNode(0)
    , m_gpu_buffer(std::move(buffer))
{
    set_vertex_stride(sizeof(Kakshya::MeshVertex));
    set_vertex_layout(Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));

    if (vertex_count > 0)
        m_vertices.resize(vertex_count);
}

/* void GeometryReadbackNode::compute_frame()
{
    if (!m_gpu_buffer)
        return;

    const size_t buf_bytes = m_gpu_buffer->get_size_bytes();
    if (buf_bytes == 0)
        return;

    const size_t count = buf_bytes / sizeof(Kakshya::MeshVertex);
    m_vertices.resize(count);

    Buffers::download_from_gpu(
        m_gpu_buffer,
        m_vertices.data(),
        buf_bytes);

    set_vertices<Kakshya::MeshVertex>(
        std::span { m_vertices.data(), m_vertices.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_vertices.size());
    set_vertex_layout(*layout);
} */

void GeometryReadbackNode::compute_frame()
{
    const size_t buf_bytes = m_gpu_buffer->get_size_bytes();
    if (!m_gpu_buffer || buf_bytes == 0)
        return;

    const size_t count = buf_bytes / sizeof(Kakshya::MeshVertex);
    m_vertices.resize(count);

    Buffers::download_from_gpu_async(m_gpu_buffer, m_vertices.data(), buf_bytes, m_staging_buffer);

    set_vertices<Kakshya::MeshVertex>(std::span { m_vertices.data(), m_vertices.size() });
    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(count);
    set_vertex_layout(*layout);
}

glm::vec3 GeometryReadbackNode::get_position(size_t index) const
{
    if (index >= m_vertices.size())
        return glm::vec3(0.F);
    return m_vertices[index].position;
}

} // namespace MayaFlux::Nodes::GpuSync
