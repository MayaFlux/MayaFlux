#include "PathGeneratorNode.hpp"

#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"
#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

namespace {
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
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_control_points(const std::vector<glm::vec3>& points)
{
    m_control_points.reset();

    for (const auto& pt : points) {
        m_control_points.push(pt);
    }

    m_vertex_data_dirty = true;
}

void PathGeneratorNode::update_control_point(size_t index, const glm::vec3& position)
{
    if (index >= m_control_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Control point index {} out of range (count: {})",
            index, m_control_points.size());
        return;
    }

    // auto view = m_control_points.linearized_view();
    // view[index] = position;
    m_control_points.update(index, position);

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
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_samples_per_segment(Eigen::Index samples)
{
    m_samples_per_segment = samples;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::set_tension(double tension)
{
    m_tension = tension;
    m_vertex_data_dirty = true;
}

void PathGeneratorNode::parameterize_arc_length(bool enable)
{
    m_arc_length_parameterization = enable;
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

    auto variant = Kakshya::from_eigen_matrix(interpolated,
        Kakshya::MatrixInterpretation::VEC3);

    Kakshya::EigenAccess eigen_access(variant);

    if (auto positions = eigen_access.view_as_glm<glm::vec3>()) {
        emit_vertices_from_positions(*positions);
    } else {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Failed to create zero-copy view of interpolated positions");
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

    generate_path_vertices();

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
