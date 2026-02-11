#include "PointCloudNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <glm/gtc/constants.hpp>

namespace MayaFlux::Nodes::Network {

PointCloudNetwork::PointCloudNetwork()
    : m_init_mode(InitializationMode::EMPTY)
{
    set_topology(Topology::INDEPENDENT);
    set_output_mode(OutputMode::GRAPHICS_BIND);

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created empty PointCloudNetwork");
}

PointCloudNetwork::PointCloudNetwork(
    size_t num_points,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    InitializationMode init_mode)
    : m_num_points(num_points)
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_init_mode(init_mode)
{
    set_topology(Topology::INDEPENDENT);
    set_output_mode(OutputMode::GRAPHICS_BIND);

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created PointCloudNetwork with {} points, bounds [{:.2f}, {:.2f}, {:.2f}] to [{:.2f}, {:.2f}, {:.2f}]",
        num_points,
        bounds_min.x, bounds_min.y, bounds_min.z,
        bounds_max.x, bounds_max.y, bounds_max.z);
}

void PointCloudNetwork::initialize()
{
    if (m_initialized) {
        return;
    }

    if (m_init_mode != InitializationMode::EMPTY && m_num_points > 0) {
        m_cached_vertices = generate_initial_positions();

        if (!m_operator) {
            auto topology = std::make_unique<TopologyOperator>();
            topology->initialize(m_cached_vertices);
            m_operator = std::move(topology);
        }
    }

    m_initialized = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Initialized PointCloudNetwork: {} points, operator={}",
        m_cached_vertices.size(),
        m_operator ? m_operator->get_type_name() : "none");
}

void PointCloudNetwork::set_operator(std::unique_ptr<NetworkOperator> op)
{
    if (!op) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set null operator");
        return;
    }

    std::vector<LineVertex> vertices;

    if (auto* old_path = dynamic_cast<PathOperator*>(m_operator.get())) {
        vertices = old_path->extract_vertices();
    } else if (auto* old_topo = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        vertices = old_topo->extract_vertices();
    } else if (!m_operator) {
        vertices = !m_cached_vertices.empty()
            ? m_cached_vertices
            : generate_initial_positions();
    }

    if (auto* new_path = dynamic_cast<PathOperator*>(op.get())) {
        new_path->initialize(vertices);
    } else if (auto* new_topo = dynamic_cast<TopologyOperator*>(op.get())) {
        new_topo->initialize(vertices);
    } else {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "PointCloudNetwork only supports LineVertex operators (PathOperator, TopologyOperator)");
        return;
    }

    m_operator = std::move(op);
}

void PointCloudNetwork::reset()
{
    if (m_init_mode != InitializationMode::EMPTY && m_num_points > 0) {
        m_cached_vertices = generate_initial_positions();

        if (m_operator) {
            if (auto* topo_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
                topo_op->initialize(m_cached_vertices);
            } else if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
                path_op->initialize(m_cached_vertices);
            }
        }
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Reset PointCloudNetwork: {} points reinitialized", m_cached_vertices.size());
}

void PointCloudNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || !m_operator) {
        return;
    }

    update_mapped_parameters();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        m_operator->process(0.0F);
    }

    MF_RT_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PointCloudNetwork processed {} frames with {} operator",
        num_samples, m_operator->get_type_name());
}

void PointCloudNetwork::set_topology(Topology topology)
{
    NodeNetwork::set_topology(topology);
}

size_t PointCloudNetwork::get_node_count() const
{
    if (!m_operator) {
        return m_cached_vertices.size();
    }

    if (auto* graphics_op = dynamic_cast<const GraphicsOperator*>(m_operator.get())) {
        return graphics_op->get_point_count();
    }

    return m_cached_vertices.size();
}

std::optional<double> PointCloudNetwork::get_node_output(size_t index) const
{
    if (index >= get_node_count()) {
        return std::nullopt;
    }

    return static_cast<double>(index);
}

std::unordered_map<std::string, std::string> PointCloudNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    metadata["point_count"] = std::to_string(get_node_count());
    metadata["operator"] = std::string(m_operator ? m_operator->get_type_name() : "none");
    metadata["bounds_min"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_bounds_min.x, m_bounds_min.y, m_bounds_min.z);
    metadata["bounds_max"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_bounds_max.x, m_bounds_max.y, m_bounds_max.z);

    if (auto* topology_op = dynamic_cast<const TopologyOperator*>(m_operator.get())) {
        if (auto connections = topology_op->query_state("connection_count")) {
            metadata["connection_count"] = std::to_string(static_cast<size_t>(*connections));
        }
        if (auto topology_count = topology_op->query_state("topology_count")) {
            metadata["topology_count"] = std::to_string(static_cast<size_t>(*topology_count));
        }
    }

    if (auto* path_op = dynamic_cast<const PathOperator*>(m_operator.get())) {
        if (auto vertex_count = path_op->query_state("vertex_count")) {
            metadata["vertex_count"] = std::to_string(static_cast<size_t>(*vertex_count));
        }
        if (auto path_count = path_op->query_state("path_count")) {
            metadata["path_count"] = std::to_string(static_cast<size_t>(*path_count));
        }
    }

    return metadata;
}

void PointCloudNetwork::set_vertices(const std::vector<LineVertex>& vertices)
{
    m_cached_vertices = vertices;
    m_num_points = vertices.size();

    if (m_operator) {
        if (auto* graphics_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
            graphics_op->initialize(m_cached_vertices);
        } else if (auto* graphics_op = dynamic_cast<PathOperator*>(m_operator.get())) {
            graphics_op->initialize(m_cached_vertices);
        }
    } else {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "No operator to set vertices on; vertices cached but not applied");
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Updated PointCloudNetwork vertices: {} points", vertices.size());
}

void PointCloudNetwork::apply_color_gradient(const glm::vec3& start_color, const glm::vec3& end_color)
{
    const size_t count = m_cached_vertices.size();

    for (size_t i = 0; i < count; ++i) {
        const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0F;
        m_cached_vertices[i].color = glm::mix(start_color, end_color, t);
    }

    set_vertices(m_cached_vertices);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Applied linear color gradient to {} points", count);
}

void PointCloudNetwork::apply_radial_gradient(
    const glm::vec3& center_color,
    const glm::vec3& edge_color,
    const glm::vec3& center)
{
    const size_t count = m_cached_vertices.size();

    float max_distance = 0.0F;
    for (const auto& v : m_cached_vertices) {
        const float dist = glm::length(v.position - center);
        max_distance = std::max(max_distance, dist);
    }

    for (size_t i = 0; i < count; ++i) {
        const float dist = glm::length(m_cached_vertices[i].position - center);
        const float t = max_distance > 0.0F ? dist / max_distance : 0.0F;
        m_cached_vertices[i].color = glm::mix(center_color, edge_color, t);
    }

    set_vertices(m_cached_vertices);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Applied radial color gradient to {} points", count);
}

std::vector<LineVertex> PointCloudNetwork::get_vertices() const
{
    if (m_operator) {
        if (auto* topo_op = dynamic_cast<const TopologyOperator*>(m_operator.get())) {
            return topo_op->extract_vertices();
        }

        if (auto* path_op = dynamic_cast<const PathOperator*>(m_operator.get())) {
            return path_op->extract_vertices();
        }
    }

    return m_cached_vertices;
}

void PointCloudNetwork::update_vertex(size_t index, const LineVertex& vertex)
{
    if (index >= m_cached_vertices.size()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Vertex index {} out of range (count: {})", index, m_cached_vertices.size());
        return;
    }

    m_cached_vertices[index] = vertex;

    if (m_operator) {
        if (auto* topo_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
            topo_op->initialize(m_cached_vertices);
        } else if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
            path_op->initialize(m_cached_vertices);
        }
    }
}

void PointCloudNetwork::set_connection_radius(float radius)
{
    if (auto* topology_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        topology_op->set_connection_radius(radius);
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "set_connection_radius requires TopologyOperator");
    }
}

void PointCloudNetwork::set_k_neighbors(size_t k)
{
    if (auto* topology_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        topology_op->set_parameter("k_neighbors", static_cast<double>(k));
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "set_k_neighbors requires TopologyOperator");
    }
}

void PointCloudNetwork::set_line_thickness(float thickness)
{
    if (auto* topology_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        topology_op->set_global_line_thickness(thickness);
    } else if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
        path_op->set_global_thickness(thickness);
    }
}

void PointCloudNetwork::set_line_color(const glm::vec3& color)
{
    if (auto* topology_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        topology_op->set_global_line_color(color);
    } else if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
        path_op->set_global_color(color);
    }
}

void PointCloudNetwork::set_samples_per_segment(size_t samples)
{
    if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
        path_op->set_samples_per_segment(static_cast<Eigen::Index>(samples));
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "set_samples_per_segment requires PathOperator");
    }
}

void PointCloudNetwork::set_tension(double tension)
{
    if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
        path_op->set_tension(tension);
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "set_tension requires PathOperator");
    }
}

void PointCloudNetwork::update_mapped_parameters()
{
    if (!m_operator) {
        return;
    }

    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode == MappingMode::BROADCAST && mapping.broadcast_source) {
            double value = mapping.broadcast_source->get_last_output();
            m_operator->set_parameter(mapping.param_name, value);

        } else if (mapping.mode == MappingMode::ONE_TO_ONE && mapping.network_source) {
            m_operator->apply_one_to_one(mapping.param_name, mapping.network_source);
        }
    }
}

std::vector<LineVertex> PointCloudNetwork::generate_initial_positions()
{
    std::vector<LineVertex> vertices;
    vertices.reserve(m_num_points);

    const glm::vec3 range = m_bounds_max - m_bounds_min;

    switch (m_init_mode) {
    case InitializationMode::UNIFORM_GRID: {
        const auto points_per_axis = static_cast<size_t>(std::cbrt(static_cast<double>(m_num_points)));
        const glm::vec3 step = range / static_cast<float>(points_per_axis - 1);

        for (size_t x = 0; x < points_per_axis; ++x) {
            for (size_t y = 0; y < points_per_axis; ++y) {
                for (size_t z = 0; z < points_per_axis; ++z) {
                    if (vertices.size() >= m_num_points)
                        break;
                    glm::vec3 pos = m_bounds_min + glm::vec3(static_cast<float>(x) * step.x, static_cast<float>(y) * step.y, static_cast<float>(z) * step.z);
                    glm::vec3 color = glm::vec3(
                        static_cast<float>(x) / static_cast<float>(points_per_axis - 1),
                        static_cast<float>(y) / static_cast<float>(points_per_axis - 1),
                        static_cast<float>(z) / static_cast<float>(points_per_axis - 1));
                    glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
                    float thickness = 1.0F + glm::length(pos - center) / glm::length(range) * 2.0F;
                    vertices.emplace_back(LineVertex {
                        .position = pos,
                        .color = color,
                        .thickness = thickness });
                }
            }
        }
        break;
    }
    case InitializationMode::RANDOM_SPHERE: {
        glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
        float max_radius = glm::length(range) * 0.5F;
        for (size_t i = 0; i < m_num_points; ++i) {
            auto theta = static_cast<float>(m_random_gen(0.0F, M_PI * 2.0F));
            auto phi = static_cast<float>(std::acos(m_random_gen(-1.0F, 1.0F)));
            auto radius = static_cast<float>(std::cbrt(m_random_gen(0.0F, 1.0F)));

            glm::vec3 pos = center + max_radius * radius * glm::vec3(std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi));
            glm::vec3 color = glm::vec3(
                radius,
                theta / glm::two_pi<float>(),
                phi / glm::pi<float>());
            float thickness = 1.0F + radius * 2.0F;
            vertices.emplace_back(LineVertex {
                .position = pos,
                .color = color,
                .thickness = thickness });
        }
        break;
    }
    case InitializationMode::RANDOM_CUBE:
    default: {
        for (size_t i = 0; i < m_num_points; ++i) {
            glm::vec3 pos = glm::vec3(
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_random_gen(m_bounds_min.z, m_bounds_max.z));
            glm::vec3 color = (pos - m_bounds_min) / range;
            float thickness = m_random_gen(1.0F, 3.0F);
            vertices.emplace_back(LineVertex {
                .position = pos,
                .color = color,
                .thickness = thickness });
        }
        break;
    }
    case InitializationMode::PROCEDURAL:
    case InitializationMode::EMPTY:
        break;
    }

    return vertices;
}

} // namespace MayaFlux::Nodes::Network
