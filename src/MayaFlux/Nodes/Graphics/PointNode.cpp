#include "PointNode.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

struct PointVertex {
    glm::vec3 position;
    glm::vec3 color;
    float size;
};

PointNode::PointNode()
    : PointNode(glm::vec3(0.0F), glm::vec3(1.0F), 10.0F)
{
}

PointNode::PointNode(const glm::vec3& position, const glm::vec3& color, float size)
    : GeometryWriterNode(0)
    , m_position(position)
    , m_color(color)
    , m_size(size)
{
    set_vertex_stride(sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(float));

    Kakshya::VertexLayout layout;
    layout.vertex_count = 1;
    layout.stride_bytes = sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(float);

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
        m_position.x, m_position.y, m_position.z,
        m_color.x, m_color.y, m_color.z, m_size);
}

void PointNode::set_position(const glm::vec3& position)
{
    m_position = position;
    m_vertex_data_dirty = true;
}

void PointNode::set_color(const glm::vec3& color)
{
    m_color = color;
    m_vertex_data_dirty = true;
}

void PointNode::set_size(float size)
{
    m_size = size;
    m_vertex_data_dirty = true;
}

void PointNode::compute_frame()
{
    const PointVertex vertex { .position = m_position, .color = m_color, .size = m_size };
    set_vertices<PointVertex>(std::span { &vertex, 1 });

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PointNode: position ({}, {}, {}), color ({}, {}, {}), size {}",
        m_position.x, m_position.y, m_position.z,
        m_color.x, m_color.y, m_color.z, m_size);
}

} // namespace MayaFlux::Nodes
