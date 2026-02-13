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
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
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
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_vertices.reserve(samples_per_segment * max_control_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PathGeneratorNode with custom function");
}

void PathGeneratorNode::add_control_point(const LineVertex& vertex)
{
    m_control_points.push(vertex);
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::generate_curve_segment(
    const std::vector<LineVertex>& curve_verts,
    size_t start_idx,
    std::vector<LineVertex>& output)
{
    if (start_idx + 3 >= curve_verts.size()) {
        return;
    }

    Eigen::MatrixXd segment_controls(3, 4);
    for (Eigen::Index i = 0; i < 4; ++i) {
        const auto& pt = curve_verts[start_idx + i].position;
        segment_controls.col(i) << pt.x, pt.y, pt.z;
    }

    Eigen::MatrixXd interpolated = Kinesis::generate_interpolated_points(
        segment_controls,
        m_samples_per_segment,
        m_mode,
        m_tension);

    if (m_arc_length_parameterization) {
        interpolated = Kinesis::reparameterize_by_arc_length(
            interpolated,
            m_samples_per_segment);
    }

    for (Eigen::Index i = 0; i < interpolated.cols() - 1; ++i) {
        float t0 = static_cast<float>(i) / static_cast<float>(interpolated.cols() - 1);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(interpolated.cols() - 1);

        size_t ctrl_idx0 = start_idx + std::min(static_cast<size_t>(t0 * 3), size_t(3));
        size_t ctrl_idx1 = start_idx + std::min(static_cast<size_t>(t1 * 3), size_t(3));

        glm::vec3 color0 = m_force_uniform_color ? m_current_color : curve_verts[ctrl_idx0].color;
        glm::vec3 color1 = m_force_uniform_color ? m_current_color : curve_verts[ctrl_idx1].color;

        float thick0 = m_force_uniform_thickness ? m_current_thickness : curve_verts[ctrl_idx0].thickness;
        float thick1 = m_force_uniform_thickness ? m_current_thickness : curve_verts[ctrl_idx1].thickness;

        glm::vec3 p0(interpolated(0, i), interpolated(1, i), interpolated(2, i));
        glm::vec3 p1(interpolated(0, i + 1), interpolated(1, i + 1), interpolated(2, i + 1));

        output.push_back({ p0, color0, thick0 });
        output.push_back({ p1, color1, thick1 });
    }
}

void PathGeneratorNode::append_line_segment(
    const LineVertex& v0,
    const LineVertex& v1,
    std::vector<LineVertex>& output)
{
    glm::vec3 color0 = m_force_uniform_color ? m_current_color : v0.color;
    glm::vec3 color1 = m_force_uniform_color ? m_current_color : v1.color;

    float thick0 = m_force_uniform_thickness ? m_current_thickness : v0.thickness;
    float thick1 = m_force_uniform_thickness ? m_current_thickness : v1.thickness;

    output.push_back({ v0.position, color0, thick0 });
    output.push_back({ v1.position, color1, thick1 });
}

void PathGeneratorNode::draw_to(const LineVertex& vertex)
{
    m_draw_window.push_back(vertex);

    if (m_draw_window.size() == 1) {
        m_draw_vertices.push_back({ vertex });
    } else {
        append_line_segment(
            m_draw_window[m_draw_window.size() - 2],
            vertex,
            m_draw_vertices);
    }

    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_control_points(const std::vector<LineVertex>& vertices)
{
    m_control_points.reset();

    for (const auto& v : vertices) {
        m_control_points.push(v);
    }

    m_vertex_data_dirty = true;
    m_geometry_dirty = true;
}

void PathGeneratorNode::update_control_point(size_t index, const LineVertex& vertex)
{
    if (index >= m_control_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Control point index {} out of range (count: {})",
            index, m_control_points.size());
        return;
    }

    m_control_points.update(index, vertex);

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

LineVertex PathGeneratorNode::get_control_point(size_t index) const
{
    if (index >= m_control_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Control point index {} out of range (count: {})",
            index, m_control_points.size());
        return {};
    }

    return m_control_points[index];
}

std::vector<LineVertex> PathGeneratorNode::get_control_points() const
{
    auto view = m_control_points.linearized_view();
    std::vector<LineVertex> positions;
    positions.reserve(view.size());
    for (const auto& v : view) {
        positions.push_back(v);
    }
    return positions;
}

void PathGeneratorNode::clear_path()
{
    m_control_points.reset();
    m_vertices.clear();
    m_draw_window.clear();
    m_draw_vertices.clear();
    m_completed_draws.clear();
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
    m_geometry_dirty = true;
    m_dirty_segment_start = INVALID_SEGMENT;
    m_dirty_segment_end = INVALID_SEGMENT;
}

void PathGeneratorNode::set_path_color(const glm::vec3& color, bool force_uniform)
{
    m_current_color = color;
    m_force_uniform_color = force_uniform;
    m_vertex_data_dirty = true;
    m_geometry_dirty = true;

    if (m_force_uniform_color) {
        for (auto& v : m_completed_draws)
            v.color = color;

        for (auto& v : m_draw_vertices)
            v.color = color;
    }
}

void PathGeneratorNode::force_uniform_color(bool should_force)
{
    set_path_color(m_current_color, should_force);
}

void PathGeneratorNode::set_path_thickness(float thickness, bool force_uniform)
{
    m_current_thickness = thickness;
    m_force_uniform_thickness = force_uniform;
    m_vertex_data_dirty = true;
    m_geometry_dirty = true;

    if (m_force_uniform_thickness) {
        for (auto& v : m_completed_draws)
            v.thickness = thickness;

        for (auto& v : m_draw_vertices)
            v.thickness = thickness;
    }
}

void PathGeneratorNode::force_uniform_thickness(bool should_force)
{
    set_path_thickness(m_current_thickness, should_force);
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

    for (const auto& v : view) {
        glm::vec3 color = m_force_uniform_color ? m_current_color : v.color;
        float thickness = m_force_uniform_thickness ? m_current_thickness : v.thickness;

        m_vertices.emplace_back(LineVertex {
            .position = v.position,
            .color = color,
            .thickness = thickness });
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

        auto ctrl_idx = std::min<size_t>(static_cast<size_t>(t * float(num_points - 1)), num_points - 1);
        glm::vec3 color = m_force_uniform_color ? m_current_color : view[ctrl_idx].color;
        float thickness = m_force_uniform_thickness ? m_current_thickness : view[ctrl_idx].thickness;

        m_vertices[i] = LineVertex {
            .position = m_custom_func(view, t),
            .color = color,
            .thickness = thickness
        };
    }
}

void PathGeneratorNode::generate_interpolated_path()
{
    auto control_view = m_control_points.linearized_view();
    std::vector<LineVertex> control_vec(control_view.begin(), control_view.end());

    for (size_t i = 0; i + 3 < control_vec.size(); ++i) {
        generate_curve_segment(control_vec, i, m_vertices);
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

    std::vector<LineVertex> segment_verts;
    for (size_t i = start_ctrl_idx; i <= end_ctrl_idx; ++i) {
        segment_verts.push_back(view[i]);
    }

    size_t start_vertex_idx = start_ctrl_idx * m_samples_per_segment * 2;

    std::vector<LineVertex> new_segment;

    for (size_t i = 0; i + 3 < segment_verts.size(); ++i) {
        generate_curve_segment(segment_verts, i, new_segment);
    }

    if (start_vertex_idx + new_segment.size() > m_vertices.size()) {
        m_vertices.resize(start_vertex_idx + new_segment.size());
    }

    std::ranges::copy(new_segment, m_vertices.begin() + (long)start_vertex_idx);
}

void PathGeneratorNode::compute_frame()
{
    if (m_control_points.empty() && m_draw_vertices.empty() && m_completed_draws.empty()) {
        resize_vertex_buffer(0);
        return;
    }

    if (m_geometry_dirty) {
        regenerate_geometry();
        m_geometry_dirty = false;
    }

    if (!m_vertex_data_dirty) {
        return;
    }

    m_combined_cache.clear();
    m_combined_cache.reserve(m_vertices.size() + m_completed_draws.size() + m_draw_vertices.size());
    m_combined_cache.insert(m_combined_cache.end(), m_vertices.begin(), m_vertices.end());
    m_combined_cache.insert(m_combined_cache.end(), m_completed_draws.begin(), m_completed_draws.end());
    m_combined_cache.insert(m_combined_cache.end(), m_draw_vertices.begin(), m_draw_vertices.end());

    if (m_combined_cache.empty()) {
        resize_vertex_buffer(0);
        return;
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    std::vector<LineVertex> expanded = expand_lines_to_triangles(m_combined_cache);
    set_vertices<LineVertex>(std::span { expanded.data(), expanded.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(expanded.size());
    set_vertex_layout(*layout);
#else
    set_vertices<LineVertex>(std::span { m_combined_cache.data(), m_combined_cache.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_combined_cache.size());
    set_vertex_layout(*layout);
#endif
}

void PathGeneratorNode::complete()
{
    if (m_draw_window.size() < 4) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Not enough points in draw window to generate curve segment ({} points)",
            m_draw_window.size());
        m_completed_draws.insert(
            m_completed_draws.end(),
            m_draw_vertices.begin(),
            m_draw_vertices.end());
        m_draw_vertices.clear();
        m_draw_window.clear();
        m_vertex_data_dirty = true;
        return;
    }

    std::vector<LineVertex> smoothed;

    size_t start_idx = 0;
    while (start_idx + 3 < m_draw_window.size()) {
        generate_curve_segment(m_draw_window, start_idx, smoothed);
        start_idx++;
    }

    for (size_t i = start_idx + 1; i < m_draw_window.size(); ++i) {
        append_line_segment(m_draw_window[i - 1], m_draw_window[i], smoothed);
    }

    m_completed_draws.insert(
        m_completed_draws.end(),
        smoothed.begin(),
        smoothed.end());

    m_draw_vertices.clear();
    m_draw_window.clear();
    m_vertex_data_dirty = true;
}

} // namespace MayaFlux::Nodes::GpuSync
