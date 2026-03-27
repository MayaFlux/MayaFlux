#include "MeshWriterNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

MeshWriterNode::MeshWriterNode(size_t initial_vertex_capacity)
    : GeometryWriterNode(static_cast<uint32_t>(initial_vertex_capacity))
{
    m_vertices.reserve(initial_vertex_capacity);
    set_vertex_stride(sizeof(MeshVertex));

    auto layout = Kakshya::VertexLayout::for_meshes(sizeof(MeshVertex));
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created MeshWriterNode with capacity for {} vertices", initial_vertex_capacity);
}

void MeshWriterNode::set_mesh(
    std::span<const MeshVertex> vertices,
    std::span<const uint32_t> indices)
{
    m_vertices.assign(vertices.begin(), vertices.end());
    m_indices.assign(indices.begin(), indices.end());
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void MeshWriterNode::set_mesh_vertices(std::span<const MeshVertex> vertices)
{
    m_vertices.assign(vertices.begin(), vertices.end());
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void MeshWriterNode::set_mesh_indices(std::span<const uint32_t> indices)
{
    m_indices.assign(indices.begin(), indices.end());
    m_vertex_data_dirty = true;
}

void MeshWriterNode::clear_mesh()
{
    m_vertices.clear();
    m_indices.clear();
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

Portal::Graphics::PrimitiveTopology MeshWriterNode::get_primitive_topology() const
{
    return Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
}

void MeshWriterNode::compute_frame()
{
    if (m_vertices.empty()) {
        resize_vertex_buffer(0, false);
        return;
    }

    set_vertices<MeshVertex>(std::span { m_vertices.data(), m_vertices.size() });
    set_indices(std::span { m_indices.data(), m_indices.size() });

    auto layout = Kakshya::VertexLayout::for_meshes(sizeof(MeshVertex));
    layout.vertex_count = static_cast<uint32_t>(m_vertices.size());
    set_vertex_layout(layout);

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "MeshWriterNode: uploaded {} vertices, {} indices ({} faces)",
        m_vertices.size(), m_indices.size(), m_indices.size() / 3);
}

} // namespace MayaFlux::Nodes::GpuSync
