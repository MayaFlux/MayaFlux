#include "PointCollectionNode.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

PointCollectionNode::PointCollectionNode(size_t initial_capacity)
    : GeometryWriterNode(static_cast<uint32_t>(initial_capacity))
{
    m_points.reserve(initial_capacity);
    const auto& stride = sizeof(PointVertex);
    set_vertex_stride(stride);
    auto layout = Kakshya::VertexLayout::for_points(stride);

    layout.vertex_count = 0;
    set_vertex_layout(layout);

    resize_vertex_buffer(static_cast<uint32_t>(initial_capacity), false);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PointCollectionNode with capacity for {} points", initial_capacity);
}

PointCollectionNode::PointCollectionNode(std::vector<PointVertex> points)
    : GeometryWriterNode(static_cast<uint32_t>(points.size()))
    , m_points(std::move(points))
{
    const auto& stride = sizeof(PointVertex);
    set_vertex_stride(stride);
    auto layout = Kakshya::VertexLayout::for_points(stride);
    layout.vertex_count = static_cast<uint32_t>(m_points.size());
    set_vertex_layout(layout);

    resize_vertex_buffer(static_cast<uint32_t>(m_points.size()), false);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PointCollectionNode with {} points", m_points.size());
}

void PointCollectionNode::add_point(const PointVertex& point)
{
    m_points.push_back(point);
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void PointCollectionNode::add_points(const std::vector<PointVertex>& points)
{
    m_points.insert(m_points.end(), points.begin(), points.end());
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void PointCollectionNode::set_points(const std::vector<PointVertex>& points)
{
    m_points.clear();
    m_points = points;
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void PointCollectionNode::update_point(size_t index, const PointVertex& point)
{
    if (index < m_points.size()) {
        m_points[index] = point;
        m_vertex_data_dirty = true;
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range (count: {})", index, m_points.size());
    }
}

PointVertex PointCollectionNode::get_point(size_t index) const
{
    if (index < m_points.size()) {
        return m_points[index];
    }
    MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Point index {} out of range, returning zero", index);
    return {};
}

void PointCollectionNode::clear_points()
{
    m_points.clear();
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
}

void PointCollectionNode::compute_frame()
{
    if (m_points.empty()) {
        resize_vertex_buffer(0, false);
        return;
    }

    if (get_vertex_count() != m_points.size()) {
        resize_vertex_buffer(static_cast<uint32_t>(m_points.size()), false);
    }

    set_vertices<PointVertex>(std::span { m_points.data(), m_points.size() });

    if (auto layout = get_vertex_layout()) {
        layout->vertex_count = static_cast<uint32_t>(m_points.size());
        set_vertex_layout(*layout);
    }

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PointSourceNode: Uploaded {} points to vertex buffer", m_points.size());
}

} // namespace MayaFlux::Nodes
