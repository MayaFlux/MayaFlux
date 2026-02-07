#include "PathGeneratorNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

namespace {
    struct SegmentRange {
        size_t start_control_idx;
        size_t end_control_idx;

        size_t start_vertex_idx;
        size_t end_vertex_idx;
    };

    Kakshya::VertexLayout create_line_vertex_layout()
    {
        Kakshya::VertexLayout layout;
        layout.stride_bytes = sizeof(LineVertex);

        layout.attributes.push_back(Kakshya::VertexAttributeLayout {
            .component_modality = Kakshya::DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });
        layout.attributes.push_back(Kakshya::VertexAttributeLayout {
            .component_modality = Kakshya::DataModality::VERTEX_COLORS_RGB,
            .offset_in_vertex = sizeof(glm::vec3),
            .name = "color" });
        layout.attributes.push_back(Kakshya::VertexAttributeLayout {
            .component_modality = Kakshya::DataModality::UNKNOWN,
            .offset_in_vertex = sizeof(glm::vec3) + sizeof(glm::vec3),
            .name = "thickness" });

        return layout;
    }

    SegmentRange calculate_affected_segment_range(
        size_t control_idx,
        size_t total_controls,
        Kinesis::InterpolationMode mode,
        Eigen::Index samples_per_segment)
    {
        SegmentRange range {};

        switch (mode) {
        case Kinesis::InterpolationMode::CATMULL_ROM:
        case Kinesis::InterpolationMode::BSPLINE:
            range.start_control_idx = (control_idx > 0) ? control_idx - 1 : 0;
            range.end_control_idx = std::min(control_idx + 2, total_controls - 1);
            break;

        case Kinesis::InterpolationMode::CUBIC_BEZIER:
        case Kinesis::InterpolationMode::CUBIC_HERMITE:
            range.start_control_idx = (control_idx / 4) * 4;
            range.end_control_idx = std::min(range.start_control_idx + 3, total_controls - 1);
            break;

        case Kinesis::InterpolationMode::QUADRATIC_BEZIER:
            range.start_control_idx = (control_idx / 3) * 3;
            range.end_control_idx = std::min(range.start_control_idx + 2, total_controls - 1);
            break;

        default:
            range.start_control_idx = control_idx;
            range.end_control_idx = control_idx;
            break;
        }

        range.start_vertex_idx = range.start_control_idx * samples_per_segment;
        range.end_vertex_idx = (range.end_control_idx + 1) * samples_per_segment;

        return range;
    }
}

PathGeneratorNode::PathGeneratorNode(
    Kinesis::InterpolationMode mode,
    Eigen::Index samples_per_segment,
    size_t max_control_points,
    double tension)
    : GeometryWriterNode(static_cast<uint32_t>(samples_per_segment * 10))
    , m_mode(mode)
    , m_control_points(max_control_points)
    , m_samples_per_segment(samples_per_segment)
    , m_tension(tension)
{
    set_vertex_stride(sizeof(LineVertex));

    auto layout = create_line_vertex_layout();
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_vertices.reserve(samples_per_segment * max_control_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PathGeneratorNode with mode {}, {} samples per segment, capacity {}",
        static_cast<int>(mode), samples_per_segment, max_control_points);
}

PathGeneratorNode::PathGeneratorNode(
    CustomPathFunction custom_func,
    Eigen::Index samples_per_segment,
    size_t max_control_points)
    : GeometryWriterNode(static_cast<uint32_t>(samples_per_segment * 10))
    , m_mode(Kinesis::InterpolationMode::CUSTOM)
    , m_custom_func(std::move(custom_func))
    , m_control_points(max_control_points)
    , m_samples_per_segment(samples_per_segment)
    , m_tension(0.5)
{
    set_vertex_stride(sizeof(LineVertex));

    auto layout = create_line_vertex_layout();
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_vertices.reserve(samples_per_segment * max_control_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PathGeneratorNode with custom function");
}

void PathGeneratorNode::add_control_point(const glm::vec3& position)
{
    m_control_points.push(position);
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_control_points(const std::vector<glm::vec3>& points)
{
    m_control_points.reset();

    for (const auto& pt : points) {
        m_control_points.push(pt);
    }

    m_vertex_data_dirty = true;
    m_geometry_dirty = true;
}

void PathGeneratorNode::update_control_point(size_t index, const glm::vec3& position)
{
    if (index >= m_control_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Control point index {} out of range (count: {})",
            index, m_control_points.size());
        return;
    }

    m_control_points.update(index, position);

    auto range = calculate_affected_segment_range(
        index,
        m_control_points.size(),
        m_mode,
        m_samples_per_segment);

    if (m_dirty_segment_start == INVALID_SEGMENT) {
        m_dirty_segment_start = range.start_control_idx;
        m_dirty_segment_end = range.end_control_idx;
    } else {
        m_dirty_segment_start = std::min(m_dirty_segment_start, range.start_control_idx);
        m_dirty_segment_end = std::max(m_dirty_segment_end, range.end_control_idx);
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

glm::vec3 PathGeneratorNode::get_control_point(size_t index) const
{
    if (index >= m_control_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Control point index {} out of range (count: {})",
            index, m_control_points.size());
        return glm::vec3(0.0F);
    }

    return m_control_points[index];
}

std::vector<glm::vec3> PathGeneratorNode::get_control_points() const
{
    auto view = m_control_points.linearized_view();
    return { view.begin(), view.end() };
}

void PathGeneratorNode::clear_path()
{
    m_control_points.reset();
    m_vertices.clear();
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
    m_geometry_dirty = true;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
}

void PathGeneratorNode::set_path_color(const glm::vec3& color)
{
    m_current_color = color;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_path_thickness(float thickness)
{
    m_current_thickness = thickness;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_interpolation_mode(Kinesis::InterpolationMode mode)
{
    m_mode = mode;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
    m_vertex_data_dirty = true;
    m_geometry_dirty = true;
}

void PathGeneratorNode::set_samples_per_segment(Eigen::Index samples)
{
    m_samples_per_segment = samples;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_tension(double tension)
{
    m_tension = tension;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::parameterize_arc_length(bool enable)
{
    m_arc_length_parameterization = enable;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::generate_path_vertices()
{
    m_vertices.clear();

    const size_t num_points = m_control_points.size();
    if (num_points < 2) {
        return;
    }

    if (m_mode == Kinesis::InterpolationMode::LINEAR) {
        generate_direct_path();
    } else if (m_mode == Kinesis::InterpolationMode::CUSTOM && m_custom_func) {
        generate_custom_path();
    } else {
        generate_interpolated_path();
    }

    m_vertex_data_dirty = true;
}

void PathGeneratorNode::generate_direct_path()
{
    auto view = m_control_points.linearized_view();
    m_vertices.reserve(view.size());

    for (const auto& pt : view) {
        m_vertices.emplace_back(LineVertex {
            .position = pt,
            .color = m_current_color,
            .thickness = m_current_thickness });
    }
}

void PathGeneratorNode::generate_custom_path()
{
    auto view = m_control_points.linearized_view();
    const size_t num_points = view.size();
    size_t total_samples = m_samples_per_segment * (num_points - 1);

    m_vertices.resize(total_samples);

    for (Eigen::Index i = 0; i < total_samples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(total_samples - 1);
        m_vertices[i] = LineVertex {
            .position = m_custom_func(view, t),
            .color = m_current_color,
            .thickness = m_current_thickness
        };
    }
}

void PathGeneratorNode::generate_interpolated_path()
{
    auto control_view = m_control_points.linearized_view();
    const size_t num_points = control_view.size();

    Eigen::MatrixXd control_matrix(3, num_points);
    for (Eigen::Index i = 0; i < num_points; ++i) {
        control_matrix.col(i) << control_view[i].x,
            control_view[i].y,
            control_view[i].z;
    }

    Eigen::Index total_samples = m_samples_per_segment * (num_points - 1);

    Eigen::MatrixXd interpolated = Kinesis::generate_interpolated_points(
        control_matrix,
        total_samples,
        m_mode,
        m_tension);

    if (m_arc_length_parameterization) {
        interpolated = Kinesis::reparameterize_by_arc_length(interpolated, total_samples);
    }

    for (Eigen::Index i = 0; i < interpolated.cols() - 1; ++i) {
        glm::vec3 p0(interpolated(0, i), interpolated(1, i), interpolated(2, i));
        glm::vec3 p1(interpolated(0, i + 1), interpolated(1, i + 1), interpolated(2, i + 1));

        m_vertices.push_back({ p0, m_current_color, m_current_thickness });
        m_vertices.push_back({ p1, m_current_color, m_current_thickness });
    }
}

void PathGeneratorNode::regenerate_geometry()
{
    if (!m_geometry_dirty) {
        return;
    }

    if (m_dirty_segment_start != INVALID_SEGMENT && m_dirty_segment_end != INVALID_SEGMENT) {
        regenerate_segment_range(m_dirty_segment_start, m_dirty_segment_end);
        m_dirty_segment_start = INVALID_SEGMENT;
        m_dirty_segment_end = INVALID_SEGMENT;
    } else {
        generate_path_vertices();
    }

    m_geometry_dirty = false;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::regenerate_segment_range(size_t start_ctrl_idx, size_t end_ctrl_idx)
{
    auto view = m_control_points.linearized_view();
    const size_t num_points = view.size();

    if (start_ctrl_idx >= num_points || end_ctrl_idx >= num_points) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Invalid segment range [{}, {}] for {} control points",
            start_ctrl_idx, end_ctrl_idx, num_points);
        return;
    }

    size_t segment_ctrl_count = end_ctrl_idx - start_ctrl_idx + 1;

    Eigen::MatrixXd control_matrix(3, segment_ctrl_count);
    for (Eigen::Index i = 0; i < segment_ctrl_count; ++i) {
        size_t idx = start_ctrl_idx + i;
        control_matrix.col(i) << view[idx].x, view[idx].y, view[idx].z;
    }

    Eigen::Index segment_samples = m_samples_per_segment * (segment_ctrl_count - 1);

    Eigen::MatrixXd interpolated = Kinesis::generate_interpolated_points(
        control_matrix,
        segment_samples,
        m_mode,
        m_tension);

    if (m_arc_length_parameterization) {
        interpolated = Kinesis::reparameterize_by_arc_length(interpolated, segment_samples);
    }

    size_t start_vertex_idx = start_ctrl_idx * m_samples_per_segment;
    auto vertex_count = static_cast<size_t>(interpolated.cols());

    if (start_vertex_idx + vertex_count > m_vertices.size()) {
        m_vertices.resize(start_vertex_idx + vertex_count);
    }

    for (Eigen::Index i = 0; i < interpolated.cols() - 1; ++i) {
        glm::vec3 p0(interpolated(0, i), interpolated(1, i), interpolated(2, i));
        glm::vec3 p1(interpolated(0, i + 1), interpolated(1, i + 1), interpolated(2, i + 1));

        size_t v_idx = start_vertex_idx + (i * 2);
        if (v_idx + 1 < m_vertices.size()) {
            m_vertices[v_idx] = {
                .position = p0,
                .color = m_current_color,
                .thickness = m_current_thickness
            };
            m_vertices[v_idx + 1] = {
                .position = p1,
                .color = m_current_color,
                .thickness = m_current_thickness
            };
        }
    }
}

void PathGeneratorNode::emit_vertices_from_positions(
    std::span<const glm::vec3> positions)
{
    m_vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        m_vertices[i] = LineVertex {
            .position = positions[i],
            .color = m_current_color,
            .thickness = m_current_thickness
        };
    }
}

void PathGeneratorNode::compute_frame()
{
    if (m_control_points.empty()) {
        resize_vertex_buffer(0);
        return;
    }

    regenerate_geometry();

    if (!m_vertex_data_dirty) {
        return;
    }

    if (m_vertices.empty()) {
        resize_vertex_buffer(0);
        return;
    }

    set_vertices<LineVertex>(std::span { m_vertices.data(), m_vertices.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_vertices.size());
    set_vertex_layout(*layout);
}

} // namespace MayaFlux::Nodes::GpuSync
