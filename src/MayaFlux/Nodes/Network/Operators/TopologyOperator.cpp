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

void TopologyOperator::initialize(const std::vector<LineVertex>& vertices)
{
    if (vertices.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot initialize topology with zero vertices");
        return;
    }

    m_topologies.clear();
    add_topology(vertices, m_default_mode);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TopologyOperator initialized with {} points in 1 topology",
        vertices.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Topologies)
//-----------------------------------------------------------------------------

void TopologyOperator::initialize_topologies(
    const std::vector<std::vector<LineVertex>>& topologies,
    Kinesis::ProximityMode mode)
{
    for (const auto& topo : topologies) {
        add_topology(topo, mode);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TopologyOperator initialized with {} topologies",
        topologies.size());
}

void TopologyOperator::add_topology(
    const std::vector<LineVertex>& vertices,
    Kinesis::ProximityMode mode)
{
    if (vertices.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot add topology with zero vertices");
        return;
    }

    auto topology = std::make_shared<GpuSync::TopologyGeneratorNode>(mode, 1024);

    topology->set_points(vertices);
    topology->compute_frame();

    m_topologies.push_back(std::move(topology));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added topology #{} with {} points, {} connections",
        m_topologies.size(), vertices.size(),
        m_topologies.back()->get_connection_count());
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
        topology->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Data Extraction)
//-----------------------------------------------------------------------------

std::vector<LineVertex> TopologyOperator::extract_vertices() const
{
    std::vector<LineVertex> positions;

    for (const auto& topology : m_topologies) {
        auto points = topology->get_points();
        for (const auto& pt : points) {
            positions.push_back(pt);
        }
    }

    return positions;
}

std::span<const uint8_t> TopologyOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (m_topologies.empty() || idx >= m_topologies.size()) {
        return {};
    }
    return m_topologies[idx]->get_vertex_data();
}

std::span<const uint8_t> TopologyOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& group : m_topologies) {
        auto span = group->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

Kakshya::VertexLayout TopologyOperator::get_vertex_layout() const
{
    if (m_topologies.empty()) {
        return {};
    }

    auto layout_opt = m_topologies[0]->get_vertex_layout();
    if (!layout_opt) {
        return {};
    }

    return *layout_opt;
}

size_t TopologyOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& topology : m_topologies) {
        total += topology->get_vertex_count();
    }
    return total;
}

bool TopologyOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_topologies,
        [](const auto& group) { return group->needs_gpu_update(); });
}

void TopologyOperator::mark_vertex_data_clean()
{
    for (auto& topology : m_topologies) {
        topology->mark_vertex_data_dirty(false);
    }
}

size_t TopologyOperator::get_point_count() const
{
    size_t total = 0;
    for (const auto& topology : m_topologies) {
        total += topology->get_point_count();
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
            topology->set_connection_radius(static_cast<float>(value));
        }
    } else if (param == "k_neighbors") {
        for (auto& topology : m_topologies) {
            topology->set_k_neighbors(static_cast<size_t>(value));
        }
    } else if (param == "line_thickness") {
        m_default_thickness = static_cast<float>(value);
        for (auto& topology : m_topologies) {
            topology->set_line_thickness(m_default_thickness);
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
            total_connections += topology->get_connection_count();
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
        topology->set_connection_radius(radius);
    }
}

void TopologyOperator::set_global_line_thickness(float thickness)
{
    m_default_thickness = thickness;
    for (auto& topology : m_topologies) {
        topology->set_line_thickness(thickness);
    }
}

void TopologyOperator::set_global_line_color(const glm::vec3& color)
{
    for (auto& topology : m_topologies) {
        topology->set_line_color(color);
    }
}

void* TopologyOperator::get_data_at(size_t global_index)
{
    size_t offset = 0;
    for (auto& group : m_topologies) {
        if (global_index < offset + group->get_point_count()) {
            size_t local_index = global_index - offset;
            return &group->get_points()[local_index];
        }
        offset += group->get_point_count();
    }
    return nullptr;
}

} // namespace MayaFlux::Nodes::Network
