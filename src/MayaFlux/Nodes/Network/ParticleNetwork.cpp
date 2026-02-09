#include "ParticleNetwork.hpp"

#include "Operators/PhysicsOperator.hpp"

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

    auto positions = generate_initial_positions();

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
    auto positions = generate_initial_positions();

    if (auto* graphics_op = dynamic_cast<GraphicsOperator*>(m_operator.get())) {
        graphics_op->initialize(positions);
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

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;

    if (auto* old_graphics = dynamic_cast<GraphicsOperator*>(m_operator.get())) {
        positions = old_graphics->extract_positions();
        colors = old_graphics->extract_colors();

        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Extracted {} positions, {} colors from old operator",
            positions.size(), colors.size());
    } else if (!m_operator) {
        positions = generate_initial_positions();
    }

    if (auto* new_graphics = dynamic_cast<GraphicsOperator*>(op.get())) {
        new_graphics->initialize(positions, colors);

        if (auto* physics = dynamic_cast<PhysicsOperator*>(op.get())) {
            physics->set_bounds(m_bounds_min, m_bounds_max);
        }

        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Initialized new graphics operator with {} points",
            positions.size());
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

std::vector<glm::vec3> ParticleNetwork::generate_initial_positions()
{
    std::vector<glm::vec3> positions;
    positions.reserve(m_num_points);

    for (size_t i = 0; i < m_num_points; ++i) {
        positions.push_back(generate_single_position(m_init_mode, i, m_num_points));
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Generated {} initial positions with mode {}",
        m_num_points, static_cast<int>(m_init_mode));

    return positions;
}

glm::vec3 ParticleNetwork::generate_single_position(
    InitializationMode mode,
    size_t index,
    size_t total)
{
    switch (mode) {
    case InitializationMode::RANDOM_VOLUME:
        return {
            m_random_gen(m_bounds_min.x, m_bounds_max.x),
            m_random_gen(m_bounds_min.y, m_bounds_max.y),
            m_random_gen(m_bounds_min.z, m_bounds_max.z)
        };

    case InitializationMode::RANDOM_SURFACE: {
        int face = static_cast<int>(m_random_gen(0, 6));

        switch (face) {
        case 0:
            return { m_bounds_min.x,
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_random_gen(m_bounds_min.z, m_bounds_max.z) };
        case 1:
            return { m_bounds_max.x,
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_random_gen(m_bounds_min.z, m_bounds_max.z) };
        case 2:
            return {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_bounds_min.y,
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
        case 3:
            return {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_bounds_max.y,
                m_random_gen(m_bounds_min.z, m_bounds_max.z)
            };
        case 4:
            return {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_bounds_min.z
            };
        default:
            return {
                m_random_gen(m_bounds_min.x, m_bounds_max.x),
                m_random_gen(m_bounds_min.y, m_bounds_max.y),
                m_bounds_max.z
            };
        }
    }

    case InitializationMode::GRID: {
        size_t grid_size = static_cast<size_t>(std::cbrt(static_cast<double>(total))) + 1;
        glm::vec3 spacing = (m_bounds_max - m_bounds_min) / static_cast<float>(grid_size);

        size_t x = index % grid_size;
        size_t y = (index / grid_size) % grid_size;
        size_t z = index / (grid_size * grid_size);

        return m_bounds_min + glm::vec3(x * spacing.x, y * spacing.y, z * spacing.z);
    }

    case InitializationMode::SPHERE_VOLUME: {
        glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
        float max_radius = glm::length(m_bounds_max - center);

        float radius = max_radius * std::cbrt(static_cast<float>(m_random_gen(0.0, 1.0)));
        auto theta = static_cast<float>(m_random_gen(0.0, 2.0 * glm::pi<double>()));
        float phi = std::acos(static_cast<float>(m_random_gen(-1.0, 1.0)));

        return center + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));
    }

    case InitializationMode::SPHERE_SURFACE: {
        glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
        float radius = glm::length(m_bounds_max - center);

        auto theta = static_cast<float>(m_random_gen(0.0, 2.0 * glm::pi<double>()));
        float phi = std::acos(static_cast<float>(m_random_gen(-1.0, 1.0)));

        return center + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));
    }

    case InitializationMode::CUSTOM:
    default:
        return glm::vec3(0.0F);
    }
}

} // namespace MayaFlux::Nodes::Network
