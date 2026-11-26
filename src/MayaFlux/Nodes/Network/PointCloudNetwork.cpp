#include "PointCloudNetwork.hpp"

#include "Operators/FieldOperator.hpp"

#include "MayaFlux/Kakshya/NDData/SpatialAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexPlacement.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

PointCloudNetwork::PointCloudNetwork()
    : m_init_mode(Kinesis::SpatialDistribution::EMPTY)
    , m_controls(declare_controls<PointCloudControls>())
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
    Kinesis::SpatialDistribution init_mode)
    : m_num_points(num_points)
    , m_bounds(bounds_min, bounds_max)
    , m_init_mode(init_mode)
    , m_controls(declare_controls<PointCloudControls>())
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

    if (m_init_mode != Kinesis::SpatialDistribution::EMPTY && m_num_points > 0) {
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

void PointCloudNetwork::reinitialize(
    size_t count,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    Kinesis::SpatialDistribution mode)
{
    m_bounds = { .min = bounds_min, .max = bounds_max };
    m_num_points = count;
    m_init_mode = mode;

    reset();

    if (!m_operator) {
        auto topology = std::make_unique<TopologyOperator>();
        topology->initialize(m_cached_vertices);
        m_operator = std::move(topology);
    }
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
    } else if (auto* old_field = dynamic_cast<FieldOperator*>(m_operator.get())) {
        vertices = old_field->extract_line_vertices();
    } else if (!m_operator) {
        vertices = !m_cached_vertices.empty()
            ? m_cached_vertices
            : generate_initial_positions();
    }

    if (auto* new_path = dynamic_cast<PathOperator*>(op.get())) {
        new_path->initialize(vertices);
    } else if (auto* new_topo = dynamic_cast<TopologyOperator*>(op.get())) {
        new_topo->initialize(vertices);
    } else if (auto* new_field = dynamic_cast<FieldOperator*>(op.get())) {
        new_field->initialize(vertices);
    } else {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "PointCloudNetwork: unsupported operator type '{}'", op->get_type_name());
        return;
    }

    m_operator = std::move(op);
}

void PointCloudNetwork::reset()
{
    if (m_init_mode != Kinesis::SpatialDistribution::EMPTY && m_num_points > 0) {
        m_cached_vertices = generate_initial_positions();

        if (m_operator) {
            if (auto* topo_op = dynamic_cast<TopologyOperator*>(m_operator.get())) {
                topo_op->initialize(m_cached_vertices);
            } else if (auto* path_op = dynamic_cast<PathOperator*>(m_operator.get())) {
                path_op->initialize(m_cached_vertices);
            } else if (auto* field_op = dynamic_cast<FieldOperator*>(m_operator.get())) {
                field_op->initialize(m_cached_vertices);
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

    refresh_controls();
    apply_controls();
    update_mapped_parameters();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        m_operator->process(0.0F);
    }

    if (m_operator_chain && !m_operator_chain->empty()) {
        for (unsigned int frame = 0; frame < num_samples; ++frame) {
            m_operator_chain->process(0.0F, m_operator.get());
        }
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
    if (auto* gfx = dynamic_cast<const GraphicsOperator*>(m_operator.get()))
        return gfx->get_point_count();
    return 0;
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
        m_bounds.min.x, m_bounds.min.y, m_bounds.min.z);
    metadata["bounds_max"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_bounds.max.x, m_bounds.max.y, m_bounds.max.z);

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

    if (auto* topo = dynamic_cast<TopologyOperator*>(m_operator.get())) {
        topo->set_vertices(vertices);
    } else if (auto* path = dynamic_cast<PathOperator*>(m_operator.get())) {
        path->set_vertices(vertices);
    } else if (auto* field = dynamic_cast<FieldOperator*>(m_operator.get())) {
        field->initialize(vertices);
    } else {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "set_vertices: no operator to apply to; seed cached only");
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PointCloudNetwork reseeded: {} points", vertices.size());
}

void PointCloudNetwork::apply_color_gradient(const glm::vec4& start_color, const glm::vec4& end_color)
{
    auto verts = get_vertices();
    const size_t count = verts.size();
    for (size_t i = 0; i < count; ++i) {
        const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0F;
        verts[i].color = glm::mix(start_color, end_color, t);
    }

    set_vertices(verts);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Applied linear color gradient to {} points", count);
}

void PointCloudNetwork::apply_radial_gradient(
    const glm::vec4& center_color,
    const glm::vec4& edge_color,
    const glm::vec3& center)
{
    auto verts = get_vertices();
    const size_t count = verts.size();

    float max_dist = 0.0F;
    for (const auto& v : verts)
        max_dist = std::max(max_dist, glm::length(v.position - center));

    for (size_t i = 0; i < count; ++i) {
        const float t = max_dist > 0.0F ? glm::length(verts[i].position - center) / max_dist : 0.0F;
        verts[i].color = glm::mix(center_color, edge_color, t);
    }

    set_vertices(verts);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Applied radial color gradient to {} points", count);
}

std::vector<LineVertex> PointCloudNetwork::get_vertices() const
{
    if (auto* topo = dynamic_cast<const TopologyOperator*>(m_operator.get()))
        return topo->extract_vertices();

    if (auto* path = dynamic_cast<const PathOperator*>(m_operator.get()))
        return path->extract_vertices();

    if (auto* field = dynamic_cast<const FieldOperator*>(m_operator.get()))
        return field->extract_line_vertices();

    return {};
}

void PointCloudNetwork::update_vertex([[maybe_unused]] size_t index, [[maybe_unused]] const LineVertex& vertex)
{
    MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "update_vertex is not supported; use get_vertices/set_vertices");
}

void PointCloudNetwork::set_connection_radius(float radius)
{
    set({ .connection_radius = radius });
}

void PointCloudNetwork::set_k_neighbors(size_t k)
{
    set({ .k_neighbors = static_cast<double>(k) });
}

void PointCloudNetwork::set_line_thickness(float thickness)
{
    set({ .line_thickness = thickness });
}

void PointCloudNetwork::set_line_color(const glm::vec4& color)
{
    set({ .line_color = color });
}

void PointCloudNetwork::set_samples_per_segment(size_t samples)
{
    set({ .samples_per_segment = static_cast<double>(samples) });
}

void PointCloudNetwork::set_tension(double tension)
{
    set({ .tension = tension });
}

void PointCloudNetwork::apply_controls()
{
    auto* topology = dynamic_cast<TopologyOperator*>(m_operator.get());
    auto* path = dynamic_cast<PathOperator*>(m_operator.get());

    const auto unsupported = [](std::string_view control, std::string_view needs) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "PointCloudNetwork control '{}' requires {}", control, needs);
    };

    if (const auto color = m_controls.line_color.pending<glm::vec4>()) {
        if (topology) {
            topology->set({ .line_color = *color });
        } else if (path) {
            path->set({ .color = *color });
        }
    }
    if (const auto radius = m_controls.connection_radius.pending()) {
        if (topology) {
            topology->set({ .connection_radius = *radius });
        } else {
            unsupported("connection_radius", "TopologyOperator");
        }
    }
    if (const auto neighbors = m_controls.k_neighbors.pending()) {
        if (topology) {
            topology->set({ .k_neighbors = *neighbors });
        } else {
            unsupported("k_neighbors", "TopologyOperator");
        }
    }
    if (const auto thickness = m_controls.line_thickness.pending()) {
        if (topology) {
            topology->set({ .line_thickness = *thickness });
        } else if (path) {
            path->set({ .thickness = *thickness });
        }
    }
    if (const auto samples = m_controls.samples_per_segment.pending()) {
        if (path) {
            path->set({ .samples_per_segment = *samples });
        } else if (topology) {
            topology->set({ .samples_per_segment = *samples });
        } else {
            unsupported("samples_per_segment", "PathOperator or TopologyOperator");
        }
    }
    if (const auto tension = m_controls.tension.pending()) {
        if (path) {
            path->set({ .tension = *tension });
        } else {
            unsupported("tension", "PathOperator");
        }
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
            m_operator->set_mapped_parameter(
                { mapping.param_name, mapping.param_hash }, value);

        } else if (mapping.mode == MappingMode::ONE_TO_ONE && mapping.network_source) {
            m_operator->apply_mapped_one_to_one(
                { mapping.param_name, mapping.param_hash }, mapping.network_source);
        }
    }
}

std::vector<LineVertex> PointCloudNetwork::generate_initial_positions()
{
    auto samples = Kinesis::generate_samples(m_init_mode, m_num_points, m_bounds, m_random_gen);
    return Kakshya::to_line_vertices(samples, { 1.0F, 2.0F });
}

std::shared_ptr<PointCloudNetwork> make_point_cloud_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    std::span<const uint64_t> rows)
{
    using Portal::Graphics::PrimitiveTopology;

    if (!assembly || rows.empty()) {
        return nullptr;
    }

    const auto world = assembly->world_transforms();
    const auto layout = Kakshya::VertexLayout::for_points();
    std::vector<Kakshya::Vertex> vertices;
    std::optional<bool> lines;

    for (const uint64_t row : rows) {
        const auto placement = assembly->placement(row);
        const auto* spatial = placement && placement->source ? assembly->spatial(*placement->source) : nullptr;
        if (!spatial) {
            return nullptr;
        }

        const bool is_line = spatial->topology != PrimitiveTopology::POINT_LIST;
        if (lines.value_or(is_line) != is_line) {
            return nullptr;
        }
        lines = is_line;

        const auto access = Kakshya::as_spatial_vertex_access(*spatial);
        if (!access) {
            return nullptr;
        }

        const auto* begin = reinterpret_cast<const uint8_t*>(access->conversion_buffer.data());
        std::vector<uint8_t> bytes(begin, begin + access->byte_count);
        if (!Kakshya::place_vertices(bytes, layout, world[row])) {
            return nullptr;
        }

        std::vector<Kakshya::Vertex> held(bytes.size() / sizeof(Kakshya::Vertex));
        std::memcpy(held.data(), bytes.data(), held.size() * sizeof(Kakshya::Vertex));

        if (spatial->topology != PrimitiveTopology::LINE_STRIP) {
            vertices.insert(vertices.end(), held.begin(), held.end());
            continue;
        }

        size_t offset = 0;
        for (const int32_t length : spatial->vertex_counts_per_curve) {
            const size_t end = std::min(held.size(), offset + static_cast<size_t>(std::max(length, 0)));
            for (size_t v = offset + 1; v < end; ++v) {
                vertices.push_back(held[v - 1]);
                vertices.push_back(held[v]);
            }
            offset = end;
        }
    }

    const auto fill = [&vertices]<typename V>(std::type_identity<V>) {
        static_assert(sizeof(V) == sizeof(Kakshya::Vertex));
        std::vector<V> out(vertices.size());
        std::memcpy(out.data(), vertices.data(), vertices.size() * sizeof(V));
        return out;
    };

    auto network = std::make_shared<PointCloudNetwork>();
    auto* held = network->create_operator<FieldOperator>();
    if (*lines) {
        held->initialize(fill(std::type_identity<LineVertex> {}));
    } else {
        held->initialize(fill(std::type_identity<PointVertex> {}));
    }
    return network;
}

} // namespace MayaFlux::Nodes::Network
