#include "GeometryLeafNode.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

namespace {

Kakshya::VertexLayout layout_for_topology(Portal::Graphics::PrimitiveTopology topology)
{
    using Portal::Graphics::PrimitiveTopology;
    switch (topology) {
    case PrimitiveTopology::LINE_LIST:
    case PrimitiveTopology::LINE_STRIP:
        return Kakshya::VertexLayout::for_lines(sizeof(Vertex));
    case PrimitiveTopology::TRIANGLE_LIST:
    case PrimitiveTopology::TRIANGLE_STRIP:
    case PrimitiveTopology::TRIANGLE_FAN:
        return Kakshya::VertexLayout::for_meshes(sizeof(Vertex));
    case PrimitiveTopology::POINT_LIST:
    default:
        return Kakshya::VertexLayout::for_points(sizeof(Vertex));
    }
}

} // namespace

GeometryLeafNode::GeometryLeafNode(size_t initial_capacity)
    : GeometryWriterNode(static_cast<uint32_t>(initial_capacity))
{
    m_items.reserve(initial_capacity);
    set_vertex_stride(sizeof(Vertex));

    auto layout = layout_for_topology(get_primitive_topology());
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    resize_vertex_buffer(static_cast<uint32_t>(initial_capacity), false);
}

GeometryLeafNode::GeometryLeafNode(std::vector<Vertex> vertices)
    : GeometryWriterNode(static_cast<uint32_t>(vertices.size()))
    , m_items(std::move(vertices))
{
    set_vertex_stride(sizeof(Vertex));

    auto layout = layout_for_topology(get_primitive_topology());
    layout.vertex_count = static_cast<uint32_t>(m_items.size());
    set_vertex_layout(layout);

    resize_vertex_buffer(static_cast<uint32_t>(m_items.size()), false);
}

GeometryLeafNode::GeometryLeafNode(std::vector<glm::vec3> positions)
    : GeometryLeafNode(size_t { 0 })
{
    set_positions(std::move(positions));
}

void GeometryLeafNode::set_positions(std::vector<glm::vec3> positions)
{
    m_items.clear();
    m_items.reserve(positions.size());
    for (const auto& p : positions) {
        Vertex v;
        v.position = p;
        m_items.push_back(v);
    }
    m_geometry_dirty = true;
}

void GeometryLeafNode::set_geometry(std::vector<Vertex> vertices)
{
    m_items = std::move(vertices);
    m_geometry_dirty = true;
}

void GeometryLeafNode::update_vertex(size_t index, const Vertex& vertex)
{
    if (index >= m_items.size()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GeometryLeafNode: vertex index {} out of range (count: {})", index, m_items.size());
        return;
    }
    m_items[index] = vertex;
    m_geometry_dirty = true;
}

void GeometryLeafNode::clear_geometry()
{
    m_items.clear();
    m_geometry_dirty = true;
}

void GeometryLeafNode::compute_frame()
{
    if (!m_geometry_dirty) {
        return;
    }
    m_geometry_dirty = false;

    if (m_items.empty()) {
        resize_vertex_buffer(0, false);
        return;
    }

    if (get_vertex_count() != m_items.size()) {
        resize_vertex_buffer(static_cast<uint32_t>(m_items.size()), false);
    }

    set_vertices<Vertex>(std::span { m_items.data(), m_items.size() });

    auto layout = layout_for_topology(get_primitive_topology());
    layout.vertex_count = static_cast<uint32_t>(m_items.size());
    set_vertex_layout(layout);
}

} // namespace MayaFlux::Nodes::GpuSync
