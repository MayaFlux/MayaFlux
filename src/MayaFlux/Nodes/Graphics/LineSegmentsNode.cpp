#include "LineSegmentsNode.hpp"

#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

LineSegmentsNode::LineSegmentsNode(size_t initial_capacity)
    : PathGeneratorNode(Kinesis::InterpolationMode::CATMULL_ROM, 0, 0, 0.5)
{
    auto layout = Kakshya::VertexLayout::for_lines(sizeof(LineVertex));
    layout.vertex_count = initial_capacity;
    set_vertex_layout(layout);

    m_segments.reserve(initial_capacity);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created LineSegmentsNode with capacity {}", initial_capacity);
}

//-----------------------------------------------------------------------------
// Core segment API
//-----------------------------------------------------------------------------

void LineSegmentsNode::add_line(const LineVertex& a, const LineVertex& b)
{
    m_segments.push_back(a);
    m_segments.push_back(b);
    m_vertex_data_dirty = true;
}

void LineSegmentsNode::add_axis(const LineVertex& origin, const glm::vec3& direction, float length)
{
    LineVertex tip = origin;
    tip.position = origin.position + direction * length;
    m_segments.push_back(origin);
    m_segments.push_back(tip);
    m_vertex_data_dirty = true;
}

void LineSegmentsNode::clear_segments()
{
    m_segments.clear();
    m_vertex_data_dirty = true;
}

//-----------------------------------------------------------------------------
// Differential geometry annotation
//-----------------------------------------------------------------------------

void LineSegmentsNode::add_normal(
    const std::vector<LineVertex>& path_vertices,
    float length,
    size_t stride)
{
    append_pairs(Kinesis::compute_path_normals(path_vertices, length, stride));
}

void LineSegmentsNode::add_tangent(
    const std::vector<LineVertex>& path_vertices,
    float length,
    size_t stride)
{
    append_pairs(Kinesis::compute_path_tangents(path_vertices, length, stride));
}

void LineSegmentsNode::add_curvature(
    const std::vector<LineVertex>& path_vertices,
    float scale,
    size_t stride)
{
    append_pairs(Kinesis::compute_path_curvature(path_vertices, scale, stride));
}

//-----------------------------------------------------------------------------
// compute_frame
//-----------------------------------------------------------------------------

void LineSegmentsNode::compute_frame()
{
    if (m_segments.empty()) {
        resize_vertex_buffer(0);
        return;
    }

    if (!m_vertex_data_dirty) {
        return;
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    std::vector<LineVertex> expanded = expand_lines_to_triangles(m_segments);
    set_vertices<LineVertex>(std::span { expanded.data(), expanded.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(expanded.size());
    set_vertex_layout(*layout);
#else
    set_vertices<LineVertex>(std::span { m_segments.data(), m_segments.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_segments.size());
    set_vertex_layout(*layout);
#endif

    m_vertex_data_dirty = false;
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

void LineSegmentsNode::append_pairs(const std::vector<LineVertex>& pairs)
{
    m_segments.insert(m_segments.end(), pairs.begin(), pairs.end());
    m_vertex_data_dirty = true;
}

} // namespace MayaFlux::Nodes::GpuSync
