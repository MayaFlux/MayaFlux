#include "PhysicsOperator.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

namespace MayaFlux::Nodes::Network {

PhysicsOperator::PhysicsOperator()
{
    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PhysicsOperator created");
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Basic Initialization)
//-----------------------------------------------------------------------------

void PhysicsOperator::initialize(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors)
{
    std::vector<PointVertex> vertices;
    vertices.reserve(positions.size());

    glm::vec3 fallback_color = colors.empty() ? glm::vec3(1.0F) : colors[0];
    for (size_t i = 0; i < positions.size(); ++i) {
        glm::vec3 color = (i < colors.size()) ? colors[i] : fallback_color;
        vertices.push_back(PointVertex {
            .position = positions[i],
            .color = color,
            .size = m_point_size });
    }

    add_collection(vertices);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PhysicsOperator initialized with {} points in 1 collection",
        positions.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Collections)
//-----------------------------------------------------------------------------

void PhysicsOperator::initialize_collections(
    const std::vector<std::vector<PointVertex>>& collections)
{
    for (const auto& collection : collections) {
        add_collection(collection);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PhysicsOperator initialized with {} collections",
        collections.size());
}

void PhysicsOperator::add_collection(
    const std::vector<PointVertex>& vertices,
    float mass_multiplier)
{
    if (vertices.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot add collection with zero vertices");
        return;
    }

    CollectionGroup group;
    group.collection = std::make_shared<GpuSync::PointCollectionNode>();

    group.physics_state.resize(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        group.physics_state[i] = PhysicsState {
            .velocity = glm::vec3(0.0F),
            .force = glm::vec3(0.0F),
            .mass = mass_multiplier
        };
    }

    group.collection->set_points(vertices);
    group.collection->compute_frame();

    m_collections.push_back(std::move(group));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added collection #{} with {} points (mass_mult={:.2f})",
        m_collections.size(), vertices.size(), mass_multiplier);
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

void PhysicsOperator::process(float dt)
{
    if (m_collections.empty()) {
        return;
    }

    apply_forces();
    integrate(dt);
    handle_boundary_conditions();
    sync_to_point_collection();

    for (auto& group : m_collections) {
        group.collection->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// Parameter System
//-----------------------------------------------------------------------------

std::optional<PhysicsParameter> PhysicsOperator::string_to_parameter(std::string_view param)
{
    return Reflect::string_to_enum_case_insensitive<PhysicsParameter>(param);
}

void PhysicsOperator::set_parameter(std::string_view param, double value)
{
    auto param_enum = string_to_parameter(param);

    if (!param_enum) {
        try {
            Reflect::string_to_enum_or_throw_case_insensitive<PhysicsParameter>(
                param, "PhysicsOperator parameter");
        } catch (const std::invalid_argument& e) {
            MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                "{}", e.what());
        }
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Unknown physics parameter: '{}'", param);
        return;
    }

    switch (*param_enum) {
    case PhysicsParameter::GRAVITY_X:
        m_gravity.x = static_cast<float>(value);
        break;
    case PhysicsParameter::GRAVITY_Y:
        m_gravity.y = static_cast<float>(value);
        break;
    case PhysicsParameter::GRAVITY_Z:
        m_gravity.z = static_cast<float>(value);
        break;
    case PhysicsParameter::DRAG:
        m_drag = glm::clamp(static_cast<float>(value), 0.0F, 1.0F);
        break;
    case PhysicsParameter::INTERACTION_RADIUS:
        m_interaction_radius = static_cast<float>(value);
        break;
    case PhysicsParameter::SPRING_STIFFNESS:
        m_spring_stiffness = static_cast<float>(value);
        break;
    case PhysicsParameter::REPULSION_STRENGTH:
        m_repulsion_strength = static_cast<float>(value);
        break;
    case PhysicsParameter::SPATIAL_INTERACTIONS:
        m_spatial_interactions_enabled = (value > 0.5);
        break;
    case PhysicsParameter::POINT_SIZE:
        m_point_size = static_cast<float>(value);
        for (auto& group : m_collections) {
            auto& points = group.collection->get_points();
            for (auto& pt : points) {
                pt.size = m_point_size;
            }
        }
        break;
    case PhysicsParameter::ATTRACTION_STRENGTH:
        m_attraction_strength = static_cast<float>(value);
        break;
    case PhysicsParameter::TURBULENCE:
        m_turbulence_strength = static_cast<float>(value);
        break;
    }
}

std::optional<double> PhysicsOperator::query_state(std::string_view query) const
{
    if (query == "point_count") {
        return static_cast<double>(get_point_count());
    }
    if (query == "collection_count") {
        return static_cast<double>(m_collections.size());
    }
    if (query == "avg_velocity") {
        glm::vec3 avg(0.0F);
        size_t total_points = 0;

        for (const auto& group : m_collections) {
            for (const auto& state : group.physics_state) {
                avg += state.velocity;
                ++total_points;
            }
        }

        if (total_points > 0) {
            avg /= static_cast<float>(total_points);
        }
        return static_cast<double>(glm::length(avg));
    }

    return std::nullopt;
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Data Extraction)
//-----------------------------------------------------------------------------

std::vector<glm::vec3> PhysicsOperator::extract_positions() const
{
    std::vector<glm::vec3> positions;

    for (const auto& group : m_collections) {
        const auto& points = group.collection->get_points();
        for (const auto& pt : points) {
            positions.push_back(pt.position);
        }
    }

    return positions;
}

std::vector<glm::vec3> PhysicsOperator::extract_colors() const
{
    std::vector<glm::vec3> colors;

    for (const auto& group : m_collections) {
        const auto& points = group.collection->get_points();
        for (const auto& pt : points) {
            colors.push_back(pt.color);
        }
    }

    return colors;
}

std::span<const uint8_t> PhysicsOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (m_collections.empty() || idx >= m_collections.size()) {
        return {};
    }
    return m_collections[idx].collection->get_vertex_data();
}

std::span<const uint8_t> PhysicsOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& group : m_collections) {
        auto span = group.collection->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

std::optional<double> PhysicsOperator::get_particle_velocity(size_t global_index) const
{
    size_t current_offset = 0;

    for (const auto& group : m_collections) {
        size_t group_size = group.physics_state.size();

        if (global_index < current_offset + group_size) {
            size_t local_index = global_index - current_offset;
            return static_cast<double>(glm::length(group.physics_state[local_index].velocity));
        }

        current_offset += group_size;
    }

    return std::nullopt;
}

Kakshya::VertexLayout PhysicsOperator::get_vertex_layout() const
{
    if (m_collections.empty()) {
        return {};
    }

    auto layout_opt = m_collections[0].collection->get_vertex_layout();
    if (!layout_opt.has_value()) {
        return {};
    }

    return *layout_opt;
}

size_t PhysicsOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& group : m_collections) {
        total += group.collection->get_vertex_count();
    }
    return total;
}

bool PhysicsOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_collections,
        [](const auto& group) { return group.collection->needs_gpu_update(); });
}

void PhysicsOperator::mark_vertex_data_clean()
{
    for (auto& group : m_collections) {
        group.collection->mark_vertex_data_dirty(false);
    }
}

size_t PhysicsOperator::get_point_count() const
{
    size_t total = 0;
    for (const auto& group : m_collections) {
        total += group.collection->get_point_count();
    }
    return total;
}

void PhysicsOperator::set_bounds(const glm::vec3& min, const glm::vec3& max)
{
    m_bounds_min = min;
    m_bounds_max = max;
}

void PhysicsOperator::set_attraction_point(const glm::vec3& point)
{
    m_attraction_point = point;
    m_has_attraction_point = true;
}

//-----------------------------------------------------------------------------
// ONE_TO_ONE Parameter Mapping
//-----------------------------------------------------------------------------

void PhysicsOperator::apply_one_to_one(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    size_t point_count = get_point_count();

    if (source->get_node_count() != point_count) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "ONE_TO_ONE size mismatch: {} particles vs {} source nodes",
            point_count, source->get_node_count());
        return;
    }

    if (param == "force_x" || param == "force_y" || param == "force_z") {
        apply_per_particle_force(param, source);
    } else if (param == "mass") {
        apply_per_particle_mass(source);
    } else {
        GraphicsOperator::apply_one_to_one(param, source);
    }
}

void PhysicsOperator::apply_per_particle_force(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    size_t global_index = 0;

    for (auto& group : m_collections) {
        for (auto& i : group.physics_state) {
            auto val = source->get_node_output(global_index++);
            if (!val)
                continue;

            auto force = static_cast<float>(*val);

            if (param == "force_x") {
                i.force.x += force;
            } else if (param == "force_y") {
                i.force.y += force;
            } else if (param == "force_z") {
                i.force.z += force;
            }
        }
    }
}

void PhysicsOperator::apply_per_particle_mass(
    const std::shared_ptr<NodeNetwork>& source)
{
    size_t global_index = 0;

    for (auto& group : m_collections) {
        for (auto& i : group.physics_state) {
            auto val = source->get_node_output(global_index++);
            if (!val)
                continue;

            i.mass = std::max(0.1F, static_cast<float>(*val));
        }
    }
}

//-----------------------------------------------------------------------------
// Physics Simulation
//-----------------------------------------------------------------------------

void PhysicsOperator::apply_forces()
{
    for (auto& group : m_collections) {
        for (auto& state : group.physics_state) {
            state.force = m_gravity * state.mass;
        }
    }

    if (m_has_attraction_point) {
        apply_attraction_forces();
    }

    if (m_turbulence_strength > 0.001F) {
        apply_turbulence();
    }

    if (m_spatial_interactions_enabled && m_interaction_radius > 0.0F) {
        apply_spatial_interactions();
    }
}

void PhysicsOperator::apply_turbulence()
{
    for (auto& group : m_collections) {
        for (auto& state : group.physics_state) {
            glm::vec3 random_force(
                m_random_generator(-1.0F, 1.0F),
                m_random_generator(-1.0F, 1.0F),
                m_random_generator(-1.0F, 1.0F));
            state.force += random_force * m_turbulence_strength;
        }
    }
}

void PhysicsOperator::apply_spatial_interactions()
{
    for (size_t g1 = 0; g1 < m_collections.size(); ++g1) {
        auto& group1 = m_collections[g1];
        auto& points1 = group1.collection->get_points();

        for (size_t i = 0; i < points1.size(); ++i) {
            const auto& pos_i = points1[i].position;
            auto& state_i = group1.physics_state[i];

            for (size_t g2 = 0; g2 < m_collections.size(); ++g2) {
                auto& group2 = m_collections[g2];
                auto& points2 = group2.collection->get_points();

                size_t start_j = (g1 == g2) ? i + 1 : 0;

                for (size_t j = start_j; j < points2.size(); ++j) {
                    const auto& pos_j = points2[j].position;
                    auto& state_j = group2.physics_state[j];

                    glm::vec3 delta = pos_j - pos_i;
                    float distance = glm::length(delta);

                    if (distance < m_interaction_radius && distance > 0.001F) {
                        glm::vec3 direction = delta / distance;

                        float spring_force = m_spring_stiffness * (distance - m_interaction_radius * 0.5F);

                        float repulsion_force = 0.0F;
                        if (distance < m_interaction_radius * 0.3F) {
                            repulsion_force = m_repulsion_strength / (distance * distance);
                        }

                        glm::vec3 force = direction * (spring_force - repulsion_force);

                        state_i.force += force;
                        state_j.force -= force;
                    }
                }
            }
        }
    }
}

void PhysicsOperator::apply_attraction_forces()
{
    for (auto& group : m_collections) {
        auto& points = group.collection->get_points();

        for (size_t i = 0; i < points.size(); ++i) {
            glm::vec3 to_attractor = m_attraction_point - points[i].position;
            float distance = glm::length(to_attractor);

            if (distance > 0.001F) {
                glm::vec3 direction = to_attractor / distance;
                float force_magnitude = m_attraction_strength / std::max(distance * distance, 0.1F);
                group.physics_state[i].force += direction * force_magnitude * group.physics_state[i].mass;
            }
        }
    }
}

void PhysicsOperator::integrate(float dt)
{
    for (auto& group : m_collections) {
        auto& points = group.collection->get_points();

        for (size_t i = 0; i < points.size(); ++i) {
            auto& state = group.physics_state[i];
            auto& vertex = points[i];

            glm::vec3 acceleration = state.force / state.mass;
            state.velocity += acceleration * dt;
            state.velocity *= (1.0F - m_drag);
            vertex.position += state.velocity * dt;

            state.force = glm::vec3(0.0F);
        }
    }
}

void PhysicsOperator::handle_boundary_conditions()
{
    if (m_bounds_mode == BoundsMode::NONE) {
        return;
    }

    constexpr float damping = 0.8F;

    for (auto& group : m_collections) {
        auto& points = group.collection->get_points();

        for (size_t i = 0; i < points.size(); ++i) {
            auto& vertex = points[i];
            auto& state = group.physics_state[i];

            for (int axis = 0; axis < 3; ++axis) {
                if (vertex.position[axis] < m_bounds_min[axis]) {
                    switch (m_bounds_mode) {
                    case BoundsMode::BOUNCE:
                        vertex.position[axis] = m_bounds_min[axis];
                        state.velocity[axis] *= -damping;
                        break;
                    case BoundsMode::WRAP:
                        vertex.position[axis] = m_bounds_max[axis];
                        break;
                    case BoundsMode::CLAMP:
                        vertex.position[axis] = m_bounds_min[axis];
                        state.velocity[axis] = 0.0F;
                        break;
                    case BoundsMode::NONE:
                        break;
                    }
                } else if (vertex.position[axis] > m_bounds_max[axis]) {
                    switch (m_bounds_mode) {
                    case BoundsMode::BOUNCE:
                        vertex.position[axis] = m_bounds_max[axis];
                        state.velocity[axis] *= -damping;
                        break;
                    case BoundsMode::WRAP:
                        vertex.position[axis] = m_bounds_min[axis];
                        break;
                    case BoundsMode::CLAMP:
                        vertex.position[axis] = m_bounds_max[axis];
                        state.velocity[axis] = 0.0F;
                        break;
                    case BoundsMode::NONE:
                        break;
                    }
                }
            }
        }
    }
}

void PhysicsOperator::sync_to_point_collection()
{
    for (auto& group : m_collections) {
        group.collection->mark_vertex_data_dirty(true);
    }
}

void* PhysicsOperator::get_data_at(size_t global_index)
{
    size_t offset = 0;
    for (auto& group : m_collections) {
        if (global_index < offset + group.collection->get_point_count()) {
            size_t local_index = global_index - offset;
            return &group.collection->get_points()[local_index];
        }
        offset += group.collection->get_point_count();
    }
    return nullptr;
}

void PhysicsOperator::apply_global_impulse(const glm::vec3& impulse)
{
    for (auto& group : m_collections) {
        for (auto& state : group.physics_state) {
            state.velocity += impulse / state.mass;
        }
    }
}

void PhysicsOperator::apply_impulse(size_t index, const glm::vec3& impulse)
{
    size_t offset = 0;
    for (auto& group : m_collections) {
        if (index < offset + group.collection->get_point_count()) {
            size_t local_index = index - offset;
            group.physics_state[local_index].velocity += impulse / group.physics_state[local_index].mass;
            return;
        }
        offset += group.collection->get_point_count();
    }
}

} // namespace MayaFlux::Nodes::Network
