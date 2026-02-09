#include "PointNode.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

PointNode::PointNode()
    : PointNode(PointVertex { .position = glm::vec3(0.0F), .color = glm::vec3(1.0F), .size = 10.0F })
{
}

PointNode::PointNode(const glm::vec3& position, const glm::vec3& color, float size)
    : PointNode(PointVertex { .position = position, .color = color, .size = size })
{
}

PointNode::PointNode(const PointVertex& point)
    : GeometryWriterNode(0)
    , m_point_vertex(point)
{
    const auto& stride = sizeof(PointVertex);
    set_vertex_stride(stride);
    const auto& layout = Kakshya::VertexLayout::for_points(stride);

    set_vertex_layout(layout);

    resize_vertex_buffer(1, false);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PointNode at position ({}, {}, {}), color ({}, {}, {}), size {}",
        m_point_vertex.position.x, m_point_vertex.position.y, m_point_vertex.position.z,
        m_point_vertex.color.x, m_point_vertex.color.y, m_point_vertex.color.z, m_point_vertex.size);
}

void PointNode::set_position(const glm::vec3& position)
{
    m_point_vertex.position = position;
    m_vertex_data_dirty = true;
}

void PointNode::set_color(const glm::vec3& color)
{
    m_point_vertex.color = color;
    m_vertex_data_dirty = true;
}

void PointNode::set_size(float size)
{
    m_point_vertex.size = size;
    m_vertex_data_dirty = true;
}

void PointNode::compute_frame()
{
    set_vertices<PointVertex>(std::span { &m_point_vertex, 1 });

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PointNode: position ({}, {}, {}), color ({}, {}, {}), size {}",
        m_point_vertex.position.x, m_point_vertex.position.y, m_point_vertex.position.z,
        m_point_vertex.color.x, m_point_vertex.color.y, m_point_vertex.color.z, m_point_vertex.size);
}

} // namespace MayaFlux::Nodes
