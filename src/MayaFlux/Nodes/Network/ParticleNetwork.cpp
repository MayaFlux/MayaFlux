#include "ParticleNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <glm/gtc/constants.hpp>

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

ParticleNetwork::ParticleNetwork(
    size_t num_particles,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    InitializationMode init_mode)
    : m_num_points(num_particles)
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_init_mode(init_mode)
{
    set_topology(Topology::INDEPENDENT);
    set_output_mode(OutputMode::GRAPHICS_BIND);

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created ParticleNetwork with {} points, bounds [{:.2f}, {:.2f}, {:.2f}] to [{:.2f}, {:.2f}, {:.2f}]",
        num_particles,
        bounds_min.x, bounds_min.y, bounds_min.z,
        bounds_max.x, bounds_max.y, bounds_max.z);
}

//-----------------------------------------------------------------------------
// NodeNetwork Interface
//-----------------------------------------------------------------------------

void ParticleNetwork::initialize()
{
    if (m_initialized) {
        return;
    }

    auto positions = generate_initial_vertices();

    if (!m_operator) {
        auto physics = std::make_unique<PhysicsOperator>();
        physics->set_bounds(m_bounds_min, m_bounds_max);
        physics->initialize(positions);
        m_operator = std::move(physics);
    }

    m_initialized = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Initialized ParticleNetwork: {} points, operator={}",
        m_num_points,
        m_operator ? m_operator->get_type_name() : "none");
}

void ParticleNetwork::reset()
{
    auto vertices = generate_initial_vertices();

    if (auto* physics = dynamic_cast<PhysicsOperator*>(m_operator.get())) {
        physics->initialize(vertices);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Reset ParticleNetwork: {} points reinitialized", m_num_points);
}

void ParticleNetwork::process_batch(unsigned int num_samples)
{
    ensure_initialized();

    if (!is_enabled() || !m_operator) {
        return;
    }

    update_mapped_parameters();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        m_operator->process(m_timestep);
    }

    MF_RT_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "ParticleNetwork processed {} frames with {} operator",
        num_samples, m_operator->get_type_name());
}

void ParticleNetwork::set_topology(Topology topology)
{
    NodeNetwork::set_topology(topology);

    if (auto* physics = dynamic_cast<PhysicsOperator*>(m_operator.get())) {
        bool should_interact = (topology == Topology::SPATIAL || topology == Topology::GRID_2D || topology == Topology::GRID_3D);
        physics->enable_spatial_interactions(should_interact);
    }
}

size_t ParticleNetwork::get_node_count() const
{
    if (!m_operator) {
        return m_num_points;
    }

    if (auto* graphics_op = dynamic_cast<const GraphicsOperator*>(m_operator.get())) {
        return graphics_op->get_point_count();
    }

    return m_num_points;
}

std::optional<double> ParticleNetwork::get_node_output(size_t index) const
{
    if (!m_operator) {
        return std::nullopt;
    }

    if (auto* physics = dynamic_cast<const PhysicsOperator*>(m_operator.get())) {
        return physics->get_particle_velocity(index);
    }

    return std::nullopt;
}

std::unordered_map<std::string, std::string> ParticleNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    metadata["point_count"] = std::to_string(get_node_count());
    metadata["operator"] = std::string(m_operator ? m_operator->get_type_name() : "none");
    metadata["timestep"] = std::to_string(m_timestep);
    metadata["bounds_min"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_bounds_min.x, m_bounds_min.y, m_bounds_min.z);
    metadata["bounds_max"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_bounds_max.x, m_bounds_max.y, m_bounds_max.z);

    if (m_operator) {
        if (auto* physics = dynamic_cast<PhysicsOperator*>(m_operator.get())) {
            metadata["gravity"] = std::format("({:.2f}, {:.2f}, {:.2f})",
                physics->get_gravity().x,
                physics->get_gravity().y,
                physics->get_gravity().z);
            metadata["drag"] = std::to_string(physics->get_drag());

            auto avg_vel = physics->query_state("avg_velocity");
            if (avg_vel) {
                metadata["avg_velocity"] = std::to_string(*avg_vel);
            }
        }
    }

    return metadata;
}

//-----------------------------------------------------------------------------
// Operator Management
//-----------------------------------------------------------------------------

void ParticleNetwork::set_operator(std::unique_ptr<NetworkOperator> op)
{
    if (!op) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set null operator");
        return;
    }

    const char* old_name = m_operator ? m_operator->get_type_name().data() : "none";
    const char* new_name = op->get_type_name().data();

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Switching operator: '{}' → '{}'",
        old_name, new_name);

    std::vector<PointVertex> vertices;

    if (auto* old_graphics = dynamic_cast<PhysicsOperator*>(m_operator.get())) {
        vertices = old_graphics->extract_vertices();

        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Extracted {} vertices from old operator",
            vertices.size());
    } else if (!m_operator) {
        vertices = generate_initial_vertices();
    }

    if (auto* new_graphics = dynamic_cast<PhysicsOperator*>(op.get())) {
        new_graphics->initialize(vertices);

        if (auto* physics = dynamic_cast<PhysicsOperator*>(op.get())) {
            physics->set_bounds(m_bounds_min, m_bounds_max);
        }

        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Initialized new graphics operator with {} points",
            vertices.size());
    }

    m_operator = std::move(op);

    if (auto* physics = dynamic_cast<PhysicsOperator*>(m_operator.get())) {
        bool should_interact = (get_topology() == Topology::SPATIAL);
        physics->enable_spatial_interactions(should_interact);
    }

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Operator switched successfully to '{}'", new_name);
}

//-----------------------------------------------------------------------------
// Parameter Mapping (Delegates to Operator)
//-----------------------------------------------------------------------------

void ParticleNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    NodeNetwork::map_parameter(param_name, source, mode);
}

void ParticleNetwork::unmap_parameter(const std::string& param_name)
{
    NodeNetwork::unmap_parameter(param_name);
}

void ParticleNetwork::update_mapped_parameters()
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

//-----------------------------------------------------------------------------
// Internal Helpers
//-----------------------------------------------------------------------------

void ParticleNetwork::ensure_initialized()
{
    if (!m_initialized) {
        initialize();
    }
}

std::vector<PointVertex> ParticleNetwork::generate_initial_vertices()
{
    std::vector<PointVertex> vertices;
    vertices.reserve(m_num_points);

    for (size_t i = 0; i < m_num_points; ++i) {
        vertices.push_back(generate_single_vertex(m_init_mode, i, m_num_points));
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Generated {} initial vertices with mode {}",
        m_num_points, static_cast<int>(m_init_mode));

    return vertices;
}

PointVertex ParticleNetwork::generate_single_vertex(
    InitializationMode mode,
    size_t index,
    size_t total)
{
    PointVertex vertex;
    glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
    glm::vec3 extent = m_bounds_max - m_bounds_min;

    switch (mode) {
    case InitializationMode::RANDOM_VOLUME: {
        vertex.position = {
            m_random_gen(m_bounds_min.x, m_bounds_max.x),
            m_random_gen(m_bounds_min.y, m_bounds_max.y),
            m_random_gen(m_bounds_min.z, m_bounds_max.z)
        };

        vertex.color = (vertex.position - m_bounds_min) / extent;

        vertex.size = static_cast<float>(m_random_gen(8.0F, 12.0F));
        break;
    }

    case InitializationMode::RANDOM_SURFACE: {
        int face = static_cast<int>(m_random_gen(0, 6));

        switch (face) {
        case 0:
            vertex.position = {
                m_bounds_min.x,
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
            vertex.color = glm::vec3(0.8F, 0.3F, 0.3F);
            break;
        case 1:
            vertex.position = {
                m_bounds_max.x,
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
            vertex.color = glm::vec3(1.0F, 0.4F, 0.4F);
            break;
        case 2:
            vertex.position = {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_bounds_min.y,
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
            vertex.color = glm::vec3(0.3F, 0.8F, 0.3F);
            break;
        case 3:
            vertex.position = {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_bounds_max.y,
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
            vertex.color = glm::vec3(0.4F, 1.0F, 0.4F);
            break;
        case 4:
            vertex.position = {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_bounds_min.z
            };
            vertex.color = glm::vec3(0.3F, 0.3F, 0.8F);
            break;
        default:
            vertex.position = {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_bounds_max.z
            };
            vertex.color = glm::vec3(0.4F, 0.4F, 1.0F);
            break;
        }

        vertex.size = static_cast<float>(m_random_gen(10.0F, 14.0F));
        break;
    }

    case InitializationMode::GRID: {
        const size_t grid_size = static_cast<size_t>(std::cbrt(static_cast<double>(total))) + 1;
        const glm::vec3 spacing = extent / static_cast<float>(grid_size);

        const size_t x = index % grid_size;
        const size_t y = (index / grid_size) % grid_size;
        const size_t z = index / (grid_size * grid_size);

        vertex.position = m_bounds_min + glm::vec3(static_cast<float>(x) * spacing.x, static_cast<float>(y) * spacing.y, static_cast<float>(z) * spacing.z);

        vertex.color = glm::vec3(
            static_cast<float>(x) / static_cast<float>(grid_size),
            static_cast<float>(y) / static_cast<float>(grid_size),
            static_cast<float>(z) / static_cast<float>(grid_size));

        vertex.size = 10.0F;
        break;
    }

    case InitializationMode::SPHERE_VOLUME: {
        const float max_radius = glm::length(extent) * 0.5F;
        const float radius = max_radius * std::cbrt(static_cast<float>(m_random_gen(0.0, 1.0)));
        const auto theta = static_cast<float>(m_random_gen(0.0, 2.0 * glm::pi<double>()));
        const float phi = std::acos(static_cast<float>(m_random_gen(-1.0, 1.0)));

        vertex.position = center + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));

        const float normalized_radius = radius / max_radius;
        vertex.color = glm::mix(
            glm::vec3(1.0F, 0.8F, 0.2F),
            glm::vec3(0.2F, 0.4F, 1.0F),
            normalized_radius);

        vertex.size = glm::mix(15.0F, 6.0F, normalized_radius);
        break;
    }

    case InitializationMode::SPHERE_SURFACE: {
        const float radius = glm::length(extent) * 0.5F;
        const auto theta = static_cast<float>(m_random_gen(0.0, 2.0 * glm::pi<double>()));
        const float phi = std::acos(static_cast<float>(m_random_gen(-1.0, 1.0)));

        vertex.position = center + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));

        vertex.color = glm::vec3(
            (std::sin(theta) + 1.0F) * 0.5F,
            (phi / glm::pi<float>()),
            (std::cos(theta) + 1.0F) * 0.5F);

        const float lat_factor = std::sin(phi);
        vertex.size = glm::mix(8.0F, 12.0F, lat_factor);
        break;
    }

    case InitializationMode::CUSTOM:
    default:
        vertex.position = glm::vec3(0.0F);
        vertex.color = glm::vec3(0.5F);
        vertex.size = 10.0F;
        break;
    }

    return vertex;
}

} // namespace MayaFlux::Nodes::Network
