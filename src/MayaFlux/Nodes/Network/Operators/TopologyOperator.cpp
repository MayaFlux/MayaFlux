#include "TopologyOperator.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

TopologyOperator::TopologyOperator(Kinesis::ProximityMode mode)
    : m_default_mode(mode)
{
    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TopologyOperator created with mode {}", static_cast<int>(mode));
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Simple Initialization)
//-----------------------------------------------------------------------------

void TopologyOperator::initialize(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors)
{
    if (positions.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot initialize TopologyOperator with zero positions");
        return;
    }

    glm::vec3 line_color = colors.empty() ? glm::vec3(1.0F) : colors[0];
    add_topology(positions, m_default_mode, line_color, m_default_thickness);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TopologyOperator initialized with {} points in 1 topology",
        positions.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Topologies)
//-----------------------------------------------------------------------------

void TopologyOperator::initialize_topologies(
    const std::vector<std::vector<glm::vec3>>& topologies,
    Kinesis::ProximityMode mode,
    const std::vector<glm::vec3>& line_colors,
    const std::vector<float>& line_thicknesses)
{
    for (size_t i = 0; i < topologies.size(); ++i) {
        glm::vec3 line_color = (i < line_colors.size()) ? line_colors[i] : glm::vec3(1.0F);
        float line_thickness = (i < line_thicknesses.size()) ? line_thicknesses[i] : m_default_thickness;
        add_topology(topologies[i], mode, line_color, line_thickness);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TopologyOperator initialized with {} topologies",
        topologies.size());
}

void TopologyOperator::add_topology(
    const std::vector<glm::vec3>& positions,
    Kinesis::ProximityMode mode,
    glm::vec3 line_color,
    float line_thickness)
{
    if (positions.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot add topology with zero positions");
        return;
    }

    TopologyCollection topology;
    topology.generator = std::make_shared<GpuSync::TopologyGeneratorNode>(
        mode,
        1024);
    topology.input_positions = positions;
    topology.mode = mode;
    topology.line_color = line_color;
    topology.line_thickness = line_thickness;

    std::vector<GpuSync::LineVertex> gen_points;
    gen_points.reserve(positions.size());

    for (const auto& pos : positions) {
        gen_points.push_back(GpuSync::LineVertex {
            .position = pos,
            .color = line_color });
    }

    topology.generator->set_points(gen_points);
    topology.generator->set_line_color(line_color);
    topology.generator->set_line_thickness(line_thickness);
    topology.generator->compute_frame();

    m_topologies.push_back(std::move(topology));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added topology #{} with {} points, {} connections",
        m_topologies.size(), positions.size(),
        m_topologies.back().generator->get_connection_count());
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

void TopologyOperator::process(float /*dt*/)
{
    if (m_topologies.empty()) {
        return;
    }

    for (auto& topology : m_topologies) {
        topology.generator->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Data Extraction)
//-----------------------------------------------------------------------------

std::vector<glm::vec3> TopologyOperator::extract_positions() const
{
    std::vector<glm::vec3> positions;

    for (const auto& topology : m_topologies) {
        positions.insert(positions.end(),
            topology.input_positions.begin(),
            topology.input_positions.end());
    }

    return positions;
}

std::vector<glm::vec3> TopologyOperator::extract_colors() const
{
    std::vector<glm::vec3> colors;

    for (const auto& topology : m_topologies) {
        colors.insert(colors.end(),
            topology.input_positions.size(),
            topology.line_color);
    }

    return colors;
}

std::span<const uint8_t> TopologyOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (m_topologies.empty() || idx >= m_topologies.size()) {
        return {};
    }
    return m_topologies[idx].generator->get_vertex_data();
}

std::span<const uint8_t> TopologyOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& group : m_topologies) {
        auto span = group.generator->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

const Kakshya::VertexLayout& TopologyOperator::get_vertex_layout() const
{
    if (m_topologies.empty()) {
        static Kakshya::VertexLayout empty_layout;
        return empty_layout;
    }
    return *m_topologies[0].generator->get_vertex_layout();
}

size_t TopologyOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& topology : m_topologies) {
        total += topology.generator->get_vertex_count();
    }
    return total;
}

bool TopologyOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_topologies,
        [](const auto& group) { return group.generator->needs_gpu_update(); });
}

void TopologyOperator::mark_vertex_data_clean()
{
    for (auto& topology : m_topologies) {
        topology.generator->mark_vertex_data_dirty(false);
    }
}

size_t TopologyOperator::get_point_count() const
{
    size_t total = 0;
    for (const auto& topology : m_topologies) {
        total += topology.input_positions.size();
    }
    return total;
}

//-----------------------------------------------------------------------------
// Parameter Control
//-----------------------------------------------------------------------------

void TopologyOperator::set_parameter(std::string_view param, double value)
{
    if (param == "connection_radius") {
        for (auto& topology : m_topologies) {
            topology.generator->set_connection_radius(static_cast<float>(value));
        }
    } else if (param == "k_neighbors") {
        for (auto& topology : m_topologies) {
            topology.generator->set_k_neighbors(static_cast<size_t>(value));
        }
    } else if (param == "line_thickness") {
        m_default_thickness = static_cast<float>(value);
        for (auto& topology : m_topologies) {
            topology.line_thickness = m_default_thickness;
            topology.generator->set_line_thickness(m_default_thickness);
        }
    }
}

std::optional<double> TopologyOperator::query_state(std::string_view query) const
{
    if (query == "point_count") {
        return static_cast<double>(get_point_count());
    }

    if (query == "connection_count") {
        size_t total_connections = 0;
        for (const auto& topology : m_topologies) {
            total_connections += topology.generator->get_connection_count();
        }
        return static_cast<double>(total_connections);
    }

    if (query == "topology_count") {
        return static_cast<double>(m_topologies.size());
    }
    return std::nullopt;
}

//-----------------------------------------------------------------------------
// Topology Configuration
//-----------------------------------------------------------------------------

void TopologyOperator::set_connection_radius(float radius)
{
    for (auto& topology : m_topologies) {
        topology.generator->set_connection_radius(radius);
    }
}

void TopologyOperator::set_global_line_thickness(float thickness)
{
    m_default_thickness = thickness;
    for (auto& topology : m_topologies) {
        topology.line_thickness = thickness;
        topology.generator->set_line_thickness(thickness);
    }
}

void TopologyOperator::set_global_line_color(const glm::vec3& color)
{
    for (auto& topology : m_topologies) {
        topology.line_color = color;
        topology.generator->set_line_color(color);
    }
}

void* TopologyOperator::get_data_at(size_t global_index)
{
    size_t offset = 0;
    for (auto& group : m_topologies) {
        if (global_index < offset + group.generator->get_point_count()) {
            size_t local_index = global_index - offset;
            return &group.generator->get_points()[local_index];
        }
        offset += group.generator->get_point_count();
    }
    return nullptr;
}

} // namespace MayaFlux::Nodes::Network
