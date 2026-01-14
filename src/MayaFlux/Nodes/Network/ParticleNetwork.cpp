#include "ParticleNetwork.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "glm/gtc/constants.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

ParticleNetwork::ParticleNetwork(
    size_t num_particles,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    InitializationMode init_mode)
    : m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_init_mode(init_mode)
{
    set_topology(Topology::INDEPENDENT);
    set_output_mode(OutputMode::GRAPHICS_BIND);

    m_particles.reserve(num_particles);

    for (size_t i = 0; i < num_particles; ++i) {
        ParticleNode particle;
        particle.index = i;
        particle.point = std::make_shared<GpuSync::PointNode>();
        particle.point->set_in_network(true);
        particle.mass = 1.0F;
        particle.velocity = glm::vec3(0.0F);
        particle.acceleration = glm::vec3(0.0F);
        particle.force = glm::vec3(0.0F);

        m_particles.push_back(std::move(particle));
    }

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created ParticleNetwork with {} particles, bounds [{}, {}] to [{}, {}], mode={}",
        num_particles,
        bounds_min.x, bounds_min.y, bounds_min.z,
        bounds_max.x, bounds_max.y, bounds_max.z,
        static_cast<int>(init_mode));
}

//-----------------------------------------------------------------------------
// NodeNetwork Interface Implementation
//-----------------------------------------------------------------------------

void ParticleNetwork::initialize()
{
    initialize_particle_positions(m_init_mode);

    if (m_topology == Topology::SPATIAL) {
        rebuild_spatial_neighbors();
    } else if (m_topology == Topology::GRID_2D) {
        auto grid_size = static_cast<size_t>(std::sqrt(m_particles.size()));
        m_neighbor_map = build_grid_2d_neighbors(grid_size, grid_size);
    } else if (m_topology == Topology::RING) {
        m_neighbor_map = build_ring_neighbors(m_particles.size());
    } else if (m_topology == Topology::CHAIN) {
        m_neighbor_map = build_chain_neighbors(m_particles.size());
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Initialized ParticleNetwork: {} particles, topology={}",
        m_particles.size(), static_cast<int>(m_topology));
}

void ParticleNetwork::reset()
{
    for (auto& particle : m_particles) {
        particle.velocity = glm::vec3(0.0F);
        particle.acceleration = glm::vec3(0.0F);
        particle.force = glm::vec3(0.0F);
        particle.mass = 1.0F;
    }

    initialize_particle_positions(m_init_mode);
    update_point_nodes();
}

void ParticleNetwork::process_batch(unsigned int num_samples)
{
    ensure_initialized();

    if (!is_enabled()) {
        return;
    }

    update_mapped_parameters();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        clear_forces();
        apply_gravity();
        apply_drag();

        if (m_has_attraction_point) {
            apply_attraction_force();
        }

        if (m_topology != Topology::INDEPENDENT) {
            apply_interaction_forces();
        }

        integrate(m_timestep);
        handle_bounds();
    }

    update_point_nodes();

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "ParticleNetwork processed {} frames, {} particles",
        num_samples, m_particles.size());
}

std::unordered_map<std::string, std::string>
ParticleNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    metadata["particle_count"] = std::to_string(m_particles.size());
    metadata["gravity"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        m_gravity.x, m_gravity.y, m_gravity.z);
    metadata["drag"] = std::to_string(m_drag);
    metadata["timestep"] = std::to_string(m_timestep);
    metadata["bounds_mode"] = [this]() {
        switch (m_bounds_mode) {
        case BoundsMode::NONE:
            return "NONE";
        case BoundsMode::BOUNCE:
            return "BOUNCE";
        case BoundsMode::WRAP:
            return "WRAP";
        case BoundsMode::CLAMP:
            return "CLAMP";
        case BoundsMode::DESTROY:
            return "DESTROY";
        default:
            return "UNKNOWN";
        }
    }();

    glm::vec3 avg_velocity(0.0F);
    for (const auto& p : m_particles) {
        avg_velocity += p.velocity;
    }
    avg_velocity /= static_cast<float>(m_particles.size());
    metadata["avg_velocity"] = std::format("({:.2f}, {:.2f}, {:.2f})",
        avg_velocity.x, avg_velocity.y, avg_velocity.z);

    if (m_topology == Topology::SPATIAL) {
        metadata["interaction_radius"] = std::to_string(m_interaction_radius);
        metadata["neighbor_map_size"] = std::to_string(m_neighbor_map.size());
    }

    return metadata;
}

std::optional<double> ParticleNetwork::get_node_output(size_t index) const
{
    if (index < m_particles.size()) {
        return static_cast<double>(glm::length(m_particles[index].velocity));
    }
    return std::nullopt;
}

//-----------------------------------------------------------------------------
// Parameter Mapping
//-----------------------------------------------------------------------------

void ParticleNetwork::update_mapped_parameters()
{
    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode == MappingMode::BROADCAST && mapping.broadcast_source) {
            double value = mapping.broadcast_source->get_last_output();
            apply_broadcast_parameter(mapping.param_name, value);
        } else if (mapping.mode == MappingMode::ONE_TO_ONE && mapping.network_source) {
            apply_one_to_one_parameter(mapping.param_name, mapping.network_source);
        }
    }
}

void ParticleNetwork::apply_broadcast_parameter(const std::string& param, double value)
{
    if (param == "gravity") {
        m_gravity.y = static_cast<float>(value);
    } else if (param == "drag") {
        m_drag = glm::clamp(static_cast<float>(value), 0.0F, 1.0F);
    } else if (param == "turbulence") {
        for (auto& particle : m_particles) {
            glm::vec3 random_force(
                (static_cast<float>(rand()) / RAND_MAX - 0.5F) * 2.0F,
                (static_cast<float>(rand()) / RAND_MAX - 0.5F) * 2.0F,
                (static_cast<float>(rand()) / RAND_MAX - 0.5F) * 2.0F);
            particle.force += random_force * static_cast<float>(value);
        }
    } else if (param == "attraction") {
        m_attraction_strength = static_cast<float>(value);
    } else if (param == "spring_stiffness") {
        m_spring_stiffness = glm::max(0.0F, static_cast<float>(value));
    } else if (param == "repulsion_strength") {
        m_repulsion_strength = glm::max(0.0F, static_cast<float>(value));
    } else if (param == "force_x") {
        for (auto& particle : m_particles) {
            particle.force.x += static_cast<float>(value);
        }
    } else if (param == "force_y") {
        for (auto& particle : m_particles) {
            particle.force.y += static_cast<float>(value);
        }
    } else if (param == "force_z") {
        for (auto& particle : m_particles) {
            particle.force.z += static_cast<float>(value);
        }
    }
}

void ParticleNetwork::apply_one_to_one_parameter(
    const std::string& param,
    const std::shared_ptr<NodeNetwork>& source)
{
    if (source->get_node_count() != m_particles.size()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Parameter mapping size mismatch: {} particles vs {} source nodes",
            m_particles.size(), source->get_node_count());
        return;
    }

    if (param == "force_x") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                m_particles[i].force.x += static_cast<float>(*val);
            }
        }
    } else if (param == "force_y") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                m_particles[i].force.y += static_cast<float>(*val);
            }
        }
    } else if (param == "force_z") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                m_particles[i].force.z += static_cast<float>(*val);
            }
        }
    } else if (param == "color") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                float normalized = glm::clamp(static_cast<float>(*val), 0.0F, 1.0F);
                glm::vec3 color(normalized, 0.5F, 1.0F - normalized);
                m_particles[i].point->set_color(color);
            }
        }
    } else if (param == "size") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                float size = glm::clamp(static_cast<float>(*val) * 10.0F, 1.0F, 50.0F);
                m_particles[i].point->set_size(size);
            }
        }
    } else if (param == "mass") {
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                m_particles[i].mass = std::max(0.1F, static_cast<float>(*val));
            }
        }
    }
}

void ParticleNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = mode;
    mapping.broadcast_source = source;
    mapping.network_source = nullptr;

    m_parameter_mappings.push_back(std::move(mapping));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Mapped parameter '{}' in BROADCAST mode", param_name);
}

void ParticleNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<NodeNetwork>& source_network)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = MappingMode::ONE_TO_ONE;
    mapping.broadcast_source = nullptr;
    mapping.network_source = source_network;

    m_parameter_mappings.push_back(std::move(mapping));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Mapped parameter '{}' in ONE_TO_ONE mode ({} → {} particles)",
        param_name, source_network->get_node_count(), m_particles.size());
}

void ParticleNetwork::unmap_parameter(const std::string& param_name)
{
    m_parameter_mappings.erase(
        std::remove_if(m_parameter_mappings.begin(), m_parameter_mappings.end(),
            [&](const auto& m) { return m.param_name == param_name; }),
        m_parameter_mappings.end());
}

//-----------------------------------------------------------------------------
// Physics Simulation
//-----------------------------------------------------------------------------

void ParticleNetwork::clear_forces()
{
    for (auto& particle : m_particles) {
        particle.force = glm::vec3(0.0F);
    }
}

void ParticleNetwork::apply_gravity()
{
    for (auto& particle : m_particles) {
        particle.force += m_gravity * particle.mass;
    }
}

void ParticleNetwork::apply_drag()
{
    for (auto& particle : m_particles) {
        // F_drag = -k * v
        particle.force -= particle.velocity * m_drag;
    }
}

void ParticleNetwork::apply_attraction_force()
{
    for (auto& particle : m_particles) {
        glm::vec3 to_attractor = m_attraction_point - particle.point->get_position();
        float distance = glm::length(to_attractor);

        if (distance > 0.001F) {
            glm::vec3 direction = to_attractor / distance;
            float force_magnitude = m_attraction_strength / std::max(distance * distance, 0.1F);
            particle.force += direction * force_magnitude * particle.mass;
        }
    }
}

void ParticleNetwork::apply_interaction_forces()
{
    if (m_topology == Topology::SPATIAL) {
        if (m_neighbor_map_dirty) {
            rebuild_spatial_neighbors();
        }

        for (const auto& [idx, neighbors] : m_neighbor_map) {
            auto& particle_a = m_particles[idx];
            glm::vec3 pos_a = particle_a.point->get_position();

            for (size_t neighbor_idx : neighbors) {
                auto& particle_b = m_particles[neighbor_idx];
                glm::vec3 pos_b = particle_b.point->get_position();

                glm::vec3 delta = pos_b - pos_a;
                float distance = glm::length(delta);

                if (distance > 0.001F) {
                    glm::vec3 direction = delta / distance;

                    float spring_force = m_spring_stiffness * (distance - m_interaction_radius);

                    float repulsion_force = 0.0F;
                    if (distance < m_interaction_radius * 0.5F) {
                        repulsion_force = m_repulsion_strength / (distance * distance);
                    }

                    glm::vec3 force = direction * (spring_force - repulsion_force);
                    particle_a.force += force;
                }
            }
        }
    } else if (m_topology == Topology::GRID_2D || m_topology == Topology::GRID_3D || m_topology == Topology::RING || m_topology == Topology::CHAIN) {
        for (const auto& [idx, neighbors] : m_neighbor_map) {
            auto& particle_a = m_particles[idx];
            glm::vec3 pos_a = particle_a.point->get_position();

            for (size_t neighbor_idx : neighbors) {
                auto& particle_b = m_particles[neighbor_idx];
                glm::vec3 pos_b = particle_b.point->get_position();

                glm::vec3 delta = pos_b - pos_a;
                float distance = glm::length(delta);

                if (distance > 0.001F) {
                    glm::vec3 direction = delta / distance;
                    float spring_force = m_spring_stiffness * (distance - 1.0F); // Rest length = 1.0
                    particle_a.force += direction * spring_force;
                }
            }
        }
    }
}

void ParticleNetwork::integrate(float dt)
{
    for (auto& particle : m_particles) {
        // F = ma → a = F/m
        particle.acceleration = particle.force / particle.mass;

        // Semi-implicit Euler integration
        particle.velocity += particle.acceleration * dt;

        glm::vec3 new_position = particle.point->get_position() + particle.velocity * dt;
        particle.point->set_position(new_position);
    }
}

void ParticleNetwork::handle_bounds()
{
    if (m_bounds_mode == BoundsMode::NONE) {
        return;
    }

    for (auto& particle : m_particles) {
        glm::vec3 pos = particle.point->get_position();
        bool out_of_bounds = false;

        for (int axis = 0; axis < 3; ++axis) {
            if (pos[axis] < m_bounds_min[axis] || pos[axis] > m_bounds_max[axis]) {
                out_of_bounds = true;

                switch (m_bounds_mode) {
                case BoundsMode::BOUNCE:
                    if (pos[axis] < m_bounds_min[axis]) {
                        pos[axis] = m_bounds_min[axis];
                        particle.velocity[axis] = -particle.velocity[axis] * 0.8F; // Energy loss
                    } else if (pos[axis] > m_bounds_max[axis]) {
                        pos[axis] = m_bounds_max[axis];
                        particle.velocity[axis] = -particle.velocity[axis] * 0.8F;
                    }
                    break;

                case BoundsMode::WRAP:
                    if (pos[axis] < m_bounds_min[axis]) {
                        pos[axis] = m_bounds_max[axis];
                    } else if (pos[axis] > m_bounds_max[axis]) {
                        pos[axis] = m_bounds_min[axis];
                    }
                    break;

                case BoundsMode::CLAMP:
                    pos[axis] = glm::clamp(pos[axis], m_bounds_min[axis], m_bounds_max[axis]);
                    particle.velocity[axis] = 0.0F;
                    break;

                case BoundsMode::DESTROY:
                    pos = random_position_volume();
                    particle.velocity = glm::vec3(0.0F);
                    break;

                case BoundsMode::NONE:
                    break;
                }
            }
        }

        if (out_of_bounds) {
            particle.point->set_position(pos);
        }
    }
}

void ParticleNetwork::update_point_nodes()
{
    for (auto& particle : m_particles) {
        float speed = glm::length(particle.velocity);
        float normalized_speed = glm::clamp(speed / 10.0F, 0.0F, 1.0F);

        // Map speed to color: blue (slow) → red (fast)
        glm::vec3 color(normalized_speed, 0.3F, 1.0F - normalized_speed);
        particle.point->set_color(color);

        particle.point->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// Topology Management
//-----------------------------------------------------------------------------

void ParticleNetwork::rebuild_spatial_neighbors()
{
    m_neighbor_map.clear();

    // Simple O(n²) spatial query - could be optimized with spatial hash
    for (size_t i = 0; i < m_particles.size(); ++i) {
        glm::vec3 pos_i = m_particles[i].point->get_position();

        for (size_t j = i + 1; j < m_particles.size(); ++j) {
            glm::vec3 pos_j = m_particles[j].point->get_position();
            float distance = glm::length(pos_j - pos_i);

            if (distance < m_interaction_radius) {
                m_neighbor_map[i].push_back(j);
                m_neighbor_map[j].push_back(i);
            }
        }
    }

    m_neighbor_map_dirty = false;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Rebuilt spatial neighbor map: {} particles with neighbors",
        m_neighbor_map.size());
}

std::vector<size_t> ParticleNetwork::get_neighbors(size_t index) const
{
    auto it = m_neighbor_map.find(index);
    if (it != m_neighbor_map.end()) {
        return it->second;
    }
    return {};
}

//-----------------------------------------------------------------------------
// Initialization Helpers
//-----------------------------------------------------------------------------

void ParticleNetwork::initialize_particle_positions(InitializationMode mode)
{
    switch (mode) {
    case InitializationMode::RANDOM_VOLUME:
        for (auto& particle : m_particles) {
            particle.point->set_position(random_position_volume());
        }
        break;

    case InitializationMode::RANDOM_SURFACE:
        for (auto& particle : m_particles) {
            particle.point->set_position(random_position_surface());
        }
        break;

    case InitializationMode::GRID: {
        auto grid_size = static_cast<size_t>(std::cbrt(m_particles.size()));
        glm::vec3 spacing = (m_bounds_max - m_bounds_min) / static_cast<float>(grid_size);

        size_t idx = 0;
        for (size_t x = 0; x < grid_size && idx < m_particles.size(); ++x) {
            for (size_t y = 0; y < grid_size && idx < m_particles.size(); ++y) {
                for (size_t z = 0; z < grid_size && idx < m_particles.size(); ++z) {
                    glm::vec3 pos = m_bounds_min + glm::vec3(x, y, z) * spacing;
                    m_particles[idx++].point->set_position(pos);
                }
            }
        }
        break;
    }

    case InitializationMode::SPHERE_VOLUME: {
        glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
        float radius = glm::length(m_bounds_max - center);
        for (auto& particle : m_particles) {
            particle.point->set_position(center + random_position_sphere(radius));
        }
        break;
    }

    case InitializationMode::SPHERE_SURFACE: {
        glm::vec3 center = (m_bounds_min + m_bounds_max) * 0.5F;
        float radius = glm::length(m_bounds_max - center);
        for (auto& particle : m_particles) {
            particle.point->set_position(center + random_position_sphere_surface(radius));
        }
        break;
    }

    case InitializationMode::CUSTOM:
        break;
    }

    update_point_nodes();
}

glm::vec3 ParticleNetwork::random_position_volume() const
{
    return {
        m_bounds_min.x + (static_cast<float>(rand()) / RAND_MAX) * (m_bounds_max.x - m_bounds_min.x),
        m_bounds_min.y + (static_cast<float>(rand()) / RAND_MAX) * (m_bounds_max.y - m_bounds_min.y),
        m_bounds_min.z + (static_cast<float>(rand()) / RAND_MAX) * (m_bounds_max.z - m_bounds_min.z)
    };
}

glm::vec3 ParticleNetwork::random_position_surface() const
{
    int face = rand() % 6;
    float u = static_cast<float>(rand()) / RAND_MAX;
    float v = static_cast<float>(rand()) / RAND_MAX;

    switch (face) {
    case 0:
        return { m_bounds_min.x, glm::mix(m_bounds_min.y, m_bounds_max.y, u), glm::mix(m_bounds_min.z, m_bounds_max.z, v) };
    case 1:
        return { m_bounds_max.x, glm::mix(m_bounds_min.y, m_bounds_max.y, u), glm::mix(m_bounds_min.z, m_bounds_max.z, v) };
    case 2:
        return { glm::mix(m_bounds_min.x, m_bounds_max.x, u), m_bounds_min.y, glm::mix(m_bounds_min.z, m_bounds_max.z, v) };
    case 3:
        return { glm::mix(m_bounds_min.x, m_bounds_max.x, u), m_bounds_max.y, glm::mix(m_bounds_min.z, m_bounds_max.z, v) };
    case 4:
        return { glm::mix(m_bounds_min.x, m_bounds_max.x, u), glm::mix(m_bounds_min.y, m_bounds_max.y, v), m_bounds_min.z };
    case 5:
        return { glm::mix(m_bounds_min.x, m_bounds_max.x, u), glm::mix(m_bounds_min.y, m_bounds_max.y, v), m_bounds_max.z };
    default:
        return m_bounds_min;
    }
}

glm::vec3 ParticleNetwork::random_position_sphere(float radius) const
{
    float theta = 2.0F * glm::pi<float>() * (static_cast<float>(rand()) / RAND_MAX);
    float phi = std::acos(2.0F * (static_cast<float>(rand()) / RAND_MAX) - 1.0F);
    float r = radius * std::cbrt(static_cast<float>(rand()) / RAND_MAX);

    return {
        r * std::sin(phi) * std::cos(theta),
        r * std::sin(phi) * std::sin(theta),
        r * std::cos(phi)
    };
}

glm::vec3 ParticleNetwork::random_position_sphere_surface(float radius) const
{
    float theta = 2.0F * glm::pi<float>() * (static_cast<float>(rand()) / RAND_MAX);
    float phi = std::acos(2.0F * (static_cast<float>(rand()) / RAND_MAX) - 1.0F);

    return {
        radius * std::sin(phi) * std::cos(theta),
        radius * std::sin(phi) * std::sin(theta),
        radius * std::cos(phi)
    };
}

//-----------------------------------------------------------------------------
// Particle Control
//-----------------------------------------------------------------------------

void ParticleNetwork::apply_global_impulse(const glm::vec3& impulse)
{
    for (auto& particle : m_particles) {
        particle.velocity += impulse / particle.mass;
    }
}

void ParticleNetwork::apply_impulse(size_t index, const glm::vec3& impulse)
{
    if (index < m_particles.size()) {
        m_particles[index].velocity += impulse / m_particles[index].mass;
    }
}

void ParticleNetwork::reinitialize_positions(InitializationMode mode)
{
    initialize_particle_positions(mode);
}

void ParticleNetwork::reset_velocities()
{
    for (auto& particle : m_particles) {
        particle.velocity = glm::vec3(0.0F);
        particle.acceleration = glm::vec3(0.0F);
        particle.force = glm::vec3(0.0F);
    }
}

std::unordered_map<size_t, std::vector<size_t>>
NodeNetwork::build_grid_2d_neighbors(size_t width, size_t height)
{
    std::unordered_map<size_t, std::vector<size_t>> neighbors;

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            std::vector<size_t> node_neighbors;

            if (x > 0) {
                node_neighbors.push_back(idx - 1);
            }
            if (x < width - 1) {
                node_neighbors.push_back(idx + 1);
            }
            if (y > 0) {
                node_neighbors.push_back(idx - width);
            }
            if (y < height - 1) {
                node_neighbors.push_back(idx + width);
            }

            if (!node_neighbors.empty()) {
                neighbors[idx] = std::move(node_neighbors);
            }
        }
    }

    return neighbors;
}

std::unordered_map<size_t, std::vector<size_t>>
NodeNetwork::build_grid_3d_neighbors(size_t width, size_t height, size_t depth)
{
    std::unordered_map<size_t, std::vector<size_t>> neighbors;

    for (size_t z = 0; z < depth; ++z) {
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                size_t idx = z * (width * height) + y * width + x;
                std::vector<size_t> node_neighbors;

                if (x > 0) {
                    node_neighbors.push_back(idx - 1);
                }
                if (x < width - 1) {
                    node_neighbors.push_back(idx + 1);
                }

                if (y > 0) {
                    node_neighbors.push_back(idx - width);
                }
                if (y < height - 1) {
                    node_neighbors.push_back(idx + width);
                }

                if (z > 0) {
                    node_neighbors.push_back(idx - (width * height));
                }
                if (z < depth - 1) {
                    node_neighbors.push_back(idx + (width * height));
                }

                if (!node_neighbors.empty()) {
                    neighbors[idx] = std::move(node_neighbors);
                }
            }
        }
    }

    return neighbors;
}

std::unordered_map<size_t, std::vector<size_t>>
NodeNetwork::build_ring_neighbors(size_t count)
{
    std::unordered_map<size_t, std::vector<size_t>> neighbors;

    for (size_t i = 0; i < count; ++i) {
        std::vector<size_t> node_neighbors;

        node_neighbors.push_back((i == 0) ? count - 1 : i - 1);

        node_neighbors.push_back((i == count - 1) ? 0 : i + 1);

        neighbors[i] = std::move(node_neighbors);
    }

    return neighbors;
}

std::unordered_map<size_t, std::vector<size_t>>
NodeNetwork::build_chain_neighbors(size_t count)
{
    std::unordered_map<size_t, std::vector<size_t>> neighbors;

    for (size_t i = 0; i < count; ++i) {
        std::vector<size_t> node_neighbors;

        if (i > 0) {
            node_neighbors.push_back(i - 1);
        }

        if (i < count - 1) {
            node_neighbors.push_back(i + 1);
        }

        if (!node_neighbors.empty()) {
            neighbors[i] = std::move(node_neighbors);
        }
    }

    return neighbors;
}

} // namespace MayaFlux::Nodes
