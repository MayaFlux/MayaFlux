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
    set_vertex_stride(sizeof(PointVertex));

    Kakshya::VertexLayout layout;
    layout.vertex_count = 1;
    layout.stride_bytes = sizeof(PointVertex);

    // Location 0: position
    layout.attributes.push_back(Kakshya::VertexAttributeLayout {
        .component_modality = Kakshya::DataModality::VERTEX_POSITIONS_3D,
        .offset_in_vertex = 0,
        .name = "position" });

    // Location 1: color
    layout.attributes.push_back(Kakshya::VertexAttributeLayout {
        .component_modality = Kakshya::DataModality::VERTEX_COLORS_RGB,
        .offset_in_vertex = sizeof(glm::vec3),
        .name = "color" });

    // Location 2: size
    layout.attributes.push_back(Kakshya::VertexAttributeLayout {
        .component_modality = Kakshya::DataModality::UNKNOWN,
        .offset_in_vertex = sizeof(glm::vec3) + sizeof(glm::vec3),
        .name = "size" });

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
