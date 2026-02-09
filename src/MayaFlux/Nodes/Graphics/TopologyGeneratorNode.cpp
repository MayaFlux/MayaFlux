#include "TopologyGeneratorNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

namespace {
    float distance_squared(const glm::vec3& a, const glm::vec3& b)
    {
        glm::vec3 diff = b - a;
        return glm::dot(diff, diff);
    }
}

TopologyGeneratorNode::TopologyGeneratorNode(
    Kinesis::ProximityMode mode,
    bool auto_connect,
    size_t max_points)
    : GeometryWriterNode(static_cast<uint32_t>(max_points * max_points))
    , m_mode(mode)
    , m_points(max_points)
    , m_auto_connect(auto_connect)
{
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_vertices.reserve(max_points * max_points);
    m_connections.reserve(max_points * max_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created TopologyGeneratorNode with mode {}, auto_connect={}, capacity={}",
        static_cast<int>(mode), auto_connect, max_points);
}

TopologyGeneratorNode::TopologyGeneratorNode(
    CustomConnectionFunction custom_func,
    bool auto_connect,
    size_t max_points)
    : GeometryWriterNode(static_cast<uint32_t>(max_points * max_points))
    , m_mode(Kinesis::ProximityMode::CUSTOM)
    , m_custom_func(std::move(custom_func))
    , m_points(max_points)
    , m_auto_connect(auto_connect)
{
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_vertices.reserve(max_points * max_points);
    m_connections.reserve(max_points * max_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created TopologyGeneratorNode with custom function");
}

void TopologyGeneratorNode::add_point(const LineVertex& point)
{
    m_points.push(point);

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::add_point(const glm::vec3& position)
{
    add_point(LineVertex { .position = position });
}

void TopologyGeneratorNode::remove_point(size_t index)
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        return;
    }

    auto view = m_points.linearized_view();
    std::vector<LineVertex> temp_points(view.begin(), view.end());
    temp_points.erase(temp_points.begin() + static_cast<uint32_t>(index));

    m_points.reset();
    for (const auto& pt : temp_points) {
        m_points.push(pt);
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::update_point(size_t index, const glm::vec3& position)
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        return;
    }

    LineVertex pt = m_points[index];
    pt.position = position;

    m_points.update(index, pt);

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::update_point(size_t index, const LineVertex& point)
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        return;
    }

    m_points.update(index, point);

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_points(const std::vector<LineVertex>& points)
{
    m_points.reset();
    for (const auto& pt : points) {
        m_points.push(pt);
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_points(const std::vector<glm::vec3>& positions)
{
    m_points.reset();
    for (const auto& pos : positions) {
        m_points.push(LineVertex { .position = pos });
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::clear()
{
    m_points.reset();
    m_connections.clear();
    m_vertices.clear();
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
    regenerate_topology();
}

void TopologyGeneratorNode::regenerate_topology()
{
    m_connections.clear();

    if (m_points.empty()) {
        m_vertex_data_dirty = true;
        return;
    }

    Eigen::MatrixXd positions = points_to_eigen();

    Kinesis::ProximityConfig config;
    config.mode = m_mode;
    config.k_neighbors = m_k_neighbors;
    config.radius = m_connection_radius;
    config.custom_function = m_custom_func;

    m_connections = Kinesis::generate_proximity_graph(positions, config);
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_connection_mode(Kinesis::ProximityMode mode)
{
    m_mode = mode;
    regenerate_topology();
}

void TopologyGeneratorNode::set_auto_connect(bool enable)
{
    m_auto_connect = enable;
}

void TopologyGeneratorNode::set_k_neighbors(size_t k)
{
    m_k_neighbors = k;
    if (m_mode == Kinesis::ProximityMode::K_NEAREST && m_auto_connect) {
        regenerate_topology();
    }
}

void TopologyGeneratorNode::set_connection_radius(float radius)
{
    m_connection_radius = radius;
    if (m_mode == Kinesis::ProximityMode::RADIUS_THRESHOLD && m_auto_connect) {
        regenerate_topology();
    }
}

void TopologyGeneratorNode::set_line_color(const glm::vec3& color)
{
    m_line_color = color;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_line_thickness(float thickness)
{
    m_line_thickness = thickness;
    m_vertex_data_dirty = true;
}

const LineVertex& TopologyGeneratorNode::get_point(size_t index) const
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        static LineVertex default_point {};
        return default_point;
    }

    return m_points[index];
}

std::vector<LineVertex> TopologyGeneratorNode::get_points() const
{
    auto view = m_points.linearized_view();
    return { view.begin(), view.end() };
}

void TopologyGeneratorNode::set_path_interpolation_mode(Kinesis::InterpolationMode mode)
{
    m_path_interpolation_mode = mode;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_samples_per_segment(size_t samples)
{
    if (samples >= 2) {
        m_samples_per_segment = samples;
        m_vertex_data_dirty = true;
    }
}

void TopologyGeneratorNode::set_arc_length_reparameterization(bool enable)
{
    m_use_arc_length_reparameterization = enable;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::build_vertex_buffer()
{
    m_vertices.clear();

    auto points = get_points();
    size_t num_points = points.size();

    if (m_mode == Kinesis::ProximityMode::SEQUENTIAL
        && num_points >= 2
        && m_path_interpolation_mode != Kinesis::InterpolationMode::LINEAR) {
        build_interpolated_path(points, num_points);
    } else {
        build_direct_connections(points, num_points);
    }

    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::compute_frame()
{
    if (!m_vertex_data_dirty) {
        return;
    }

    build_vertex_buffer();

    set_vertices<LineVertex>(std::span { m_vertices.data(), m_vertices.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_vertices.size());
    set_vertex_layout(*layout);
}

void TopologyGeneratorNode::build_interpolated_path(
    std::span<LineVertex> points,
    size_t num_points)
{
    Eigen::MatrixXd control_points(3, num_points);
    for (Eigen::Index i = 0; i < num_points; ++i) {
        control_points.col(i) << points[i].position.x,
            points[i].position.y,
            points[i].position.z;
    }

    size_t num_segments = num_points - 1;
    Eigen::Index total_samples = 1 + num_segments * (m_samples_per_segment - 1);

    Eigen::MatrixXd dense_points = Kinesis::generate_interpolated_points(
        control_points,
        total_samples,
        m_path_interpolation_mode,
        0.5);

    if (m_use_arc_length_reparameterization) {
        dense_points = Kinesis::reparameterize_by_arc_length(
            dense_points, total_samples);
    }

    m_vertices.clear();
    m_vertices.reserve((dense_points.cols() - 1) * 2);

    for (Eigen::Index i = 0; i < dense_points.cols() - 1; ++i) {
        m_vertices.emplace_back(LineVertex {
            .position = glm::vec3(dense_points(0, i), dense_points(1, i), dense_points(2, i)),
            .color = m_line_color,
            .thickness = m_line_thickness });

        m_vertices.emplace_back(LineVertex {
            .position = glm::vec3(dense_points(0, i + 1), dense_points(1, i + 1), dense_points(2, i + 1)),
            .color = m_line_color,
            .thickness = m_line_thickness });
    }
}

void TopologyGeneratorNode::build_direct_connections(
    std::span<LineVertex> points,
    size_t num_points)
{
    size_t valid_connections = std::ranges::count_if(m_connections,
        [num_points](const auto& conn) {
            return conn.first < num_points && conn.second < num_points;
        });

    m_vertices.clear();
    m_vertices.reserve(valid_connections * 2);

    for (const auto& [a, b] : m_connections) {
        if (a >= num_points || b >= num_points) {
            continue;
        }

        m_vertices.emplace_back(LineVertex {
            .position = points[a].position,
            .color = m_line_color,
            .thickness = m_line_thickness });
        m_vertices.emplace_back(LineVertex {
            .position = points[b].position,
            .color = m_line_color,
            .thickness = m_line_thickness });
    }
}

Eigen::MatrixXd TopologyGeneratorNode::points_to_eigen() const
{
    auto view = m_points.linearized_view();
    Eigen::MatrixXd matrix(3, view.size());

    Eigen::Index idx = 0;
    for (const auto& point : view) {
        matrix.col(idx++) << point.position.x,
            point.position.y,
            point.position.z;
    }
    return matrix;
}

} // namespace MayaFlux::Nodes::GpuSync
