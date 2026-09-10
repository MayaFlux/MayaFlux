#include "TopologyGeneratorNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

TopologyGeneratorNode::TopologyGeneratorNode(
    Kinesis::ProximityMode mode,
    bool auto_connect,
    size_t max_points)
    : GeometryWriterNode(static_cast<uint32_t>(max_points * max_points))
    , m_mode(mode)
    , m_max_points(max_points)
    , m_auto_connect(auto_connect)
{
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_points.reserve(max_points);
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
    , m_max_points(max_points)
    , m_auto_connect(auto_connect)
{
    const auto& stride = sizeof(LineVertex);
    set_vertex_stride(stride);

    auto layout = Kakshya::VertexLayout::for_lines(stride);
    layout.vertex_count = 0;
    set_vertex_layout(layout);

    m_points.reserve(max_points);
    m_vertices.reserve(max_points * max_points);
    m_connections.reserve(max_points * max_points);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created TopologyGeneratorNode with custom function");
}

void TopologyGeneratorNode::refresh_positions()
{
    m_positions.resize(3, static_cast<Eigen::Index>(m_points.size()));

    Eigen::Index idx = 0;
    for (const auto& point : m_points) {
        m_positions(0, idx) = point.position.x;
        m_positions(1, idx) = point.position.y;
        m_positions(2, idx) = point.position.z;
        ++idx;
    }
}

void TopologyGeneratorNode::add_point(const LineVertex& point)
{
    m_points.insert(m_points.begin(), point);
    if (m_points.size() > m_max_points) {
        m_points.pop_back();
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::add_points(std::span<const LineVertex> points)
{
    if (points.empty()) {
        return;
    }

    for (const auto& pt : points) {
        m_points.insert(m_points.begin(), pt);
        if (m_points.size() > m_max_points) {
            m_points.pop_back();
        }
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::write_path_attributes(
    std::span<const LineVertex> points,
    size_t num_points)
{
    if (num_points < 2 || m_vertices.empty()) {
        return;
    }

    const size_t num_segments = num_points - 1;
    const size_t count = m_vertices.size() / 2 + 1;
    const auto span = static_cast<float>(count - 1);

    for (size_t i = 0; i + 1 < count; ++i) {
        const float t0 = static_cast<float>(i) / span;
        const float t1 = static_cast<float>(i + 1) / span;

        const auto s0 = std::min<size_t>(
            static_cast<size_t>(t0 * static_cast<float>(num_segments)), num_segments - 1);
        const auto s1 = std::min<size_t>(
            static_cast<size_t>(t1 * static_cast<float>(num_segments)), num_segments - 1);

        LineVertex& v0 = m_vertices[i * 2];
        LineVertex& v1 = m_vertices[i * 2 + 1];

        v0.color = m_force_uniform_color ? m_line_color : points[s0].color;
        v1.color = m_force_uniform_color ? m_line_color : points[s1].color;

        v0.thickness = m_force_uniform_thickness ? m_line_thickness : points[s0].thickness;
        v1.thickness = m_force_uniform_thickness ? m_line_thickness : points[s1].thickness;
    }
}

void TopologyGeneratorNode::refresh_attributes()
{
    if (m_vertices.empty()) {
        return;
    }

    const size_t num_points = m_points.size();

    if (m_mode == Kinesis::ProximityMode::SEQUENTIAL
        && num_points >= 2
        && m_path_interpolation_mode != Kinesis::InterpolationMode::LINEAR) {
        write_path_attributes(m_points, num_points);
        return;
    }

    build_direct_connections(m_points, num_points);
}

void TopologyGeneratorNode::remove_point(size_t index)
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        return;
    }

    m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::update_point(size_t index, const LineVertex& point)
{
    if (index >= m_points.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Point index {} out of range", index);
        return;
    }

    m_points[index] = point;

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_points(const std::vector<LineVertex>& points)
{
    m_points.assign(points.rbegin(), points.rend());
    if (m_points.size() > m_max_points) {
        m_points.erase(
            m_points.begin() + static_cast<std::ptrdiff_t>(m_max_points),
            m_points.end());
    }

    if (m_auto_connect) {
        regenerate_topology();
    }

    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::clear()
{
    m_points.clear();
    m_connections.clear();
    m_vertices.clear();
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
    m_needs_layout_update = true;
    regenerate_topology();
}

void TopologyGeneratorNode::regenerate_topology()
{
    m_connections.clear();

    if (m_points.empty()) {
        m_geometry_dirty = true;
        m_vertex_data_dirty = true;
        return;
    }

    refresh_positions();

    Kinesis::ProximityConfig config;
    config.mode = m_mode;
    config.k_neighbors = m_k_neighbors;
    config.radius = m_connection_radius;
    config.custom_function = m_custom_func;

    m_connections = Kinesis::generate_proximity_graph(m_positions, config);

    m_geometry_dirty = true;
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

void TopologyGeneratorNode::set_line_color(const glm::vec3& color, bool force_uniform)
{
    m_line_color = color;
    m_force_uniform_color = force_uniform;
    m_attributes_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::force_uniform_color(bool should_force)
{
    m_force_uniform_color = should_force;
    m_attributes_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_line_thickness(float thickness, bool force_uniform)
{
    m_line_thickness = thickness;
    m_force_uniform_thickness = force_uniform;
    m_attributes_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::force_uniform_thickness(bool should_force)
{
    m_force_uniform_thickness = should_force;
    m_attributes_dirty = true;
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
    return m_points;
}

void TopologyGeneratorNode::set_path_interpolation_mode(Kinesis::InterpolationMode mode)
{
    m_path_interpolation_mode = mode;
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::set_samples_per_segment(size_t samples)
{
    if (samples >= 2) {
        m_samples_per_segment = samples;
        m_geometry_dirty = true;
        m_vertex_data_dirty = true;
    }
}

void TopologyGeneratorNode::set_arc_length_reparameterization(bool enable)
{
    m_use_arc_length_reparameterization = enable;
    m_geometry_dirty = true;
    m_vertex_data_dirty = true;
}

void TopologyGeneratorNode::build_vertex_buffer()
{
    m_vertices.clear();

    const size_t num_points = m_points.size();

    if (m_mode == Kinesis::ProximityMode::SEQUENTIAL
        && num_points >= 2
        && m_path_interpolation_mode != Kinesis::InterpolationMode::LINEAR) {
        build_interpolated_path(m_points, num_points);
    } else {
        build_direct_connections(m_points, num_points);
    }
}

void TopologyGeneratorNode::compute_frame()
{
    if (m_geometry_dirty) {
        build_vertex_buffer();
        m_geometry_dirty = false;
        m_attributes_dirty = false;
    } else if (m_attributes_dirty) {
        refresh_attributes();
        m_attributes_dirty = false;
        m_vertex_data_dirty = true;
    }

    if (!m_vertex_data_dirty) {
        return;
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    m_expand_cache = expand_lines_to_triangles(m_vertices);
    set_vertices<LineVertex>(std::span { m_expand_cache.data(), m_expand_cache.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_expand_cache.size());
    set_vertex_layout(*layout);
#else
    set_vertices<LineVertex>(std::span { m_vertices.data(), m_vertices.size() });

    auto layout = get_vertex_layout();
    layout->vertex_count = static_cast<uint32_t>(m_vertices.size());
    set_vertex_layout(*layout);
#endif

    m_vertex_data_dirty = false;
}

void TopologyGeneratorNode::build_interpolated_path(
    std::span<LineVertex> points,
    size_t num_points)
{
    m_evaluator.configure(m_path_interpolation_mode, 0.5);

    m_control_scratch.resize(num_points * 3);
    for (size_t i = 0; i < num_points; ++i) {
        m_control_scratch[i * 3 + 0] = points[i].position.x;
        m_control_scratch[i * 3 + 1] = points[i].position.y;
        m_control_scratch[i * 3 + 2] = points[i].position.z;
    }

    const size_t num_segments = num_points - 1;
    const auto total_samples = static_cast<Eigen::Index>(
        1 + num_segments * (m_samples_per_segment - 1));

    m_evaluator.evaluate_planar(m_control_scratch, 3, total_samples, m_curve_primary);

    const std::vector<double>* curve = &m_curve_primary;

    if (m_use_arc_length_reparameterization) {
        m_evaluator.reparameterize_planar(m_curve_primary, 3,
            total_samples, total_samples, m_curve_secondary);
        curve = &m_curve_secondary;
    }

    const auto count = static_cast<size_t>(total_samples);
    if (count < 2) {
        return;
    }

    m_vertices.resize((count - 1) * 2);

    const double* x = curve->data();
    const double* y = x + count;
    const double* z = y + count;

    for (size_t i = 0; i + 1 < count; ++i) {
        m_vertices[i * 2].position = {
            static_cast<float>(x[i]), static_cast<float>(y[i]), static_cast<float>(z[i])
        };
        m_vertices[i * 2 + 1].position = {
            static_cast<float>(x[i + 1]), static_cast<float>(y[i + 1]), static_cast<float>(z[i + 1])
        };
    }

    write_path_attributes(points, num_points);
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

        glm::vec3 color_a = m_force_uniform_color ? m_line_color : points[a].color;
        glm::vec3 color_b = m_force_uniform_color ? m_line_color : points[b].color;

        float thick_a = m_force_uniform_thickness ? m_line_thickness : points[a].thickness;
        float thick_b = m_force_uniform_thickness ? m_line_thickness : points[b].thickness;

        m_vertices.emplace_back(LineVertex {
            .position = points[a].position,
            .color = color_a,
            .thickness = thick_a });

        m_vertices.emplace_back(LineVertex {
            .position = points[b].position,
            .color = color_b,
            .thickness = thick_b });
    }
}

} // namespace MayaFlux::Nodes::GpuSync
