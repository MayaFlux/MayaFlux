#include "PhysicsOperator.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

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
    initialize_collection(positions, colors, 1.0F, glm::vec3(1.0F), 1.0F);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PhysicsOperator initialized with {} points in 1 collection",
        positions.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Collections)
//-----------------------------------------------------------------------------

void PhysicsOperator::initialize_collection(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors,
    float mass_multiplier,
    glm::vec3 color_tint,
    float size_scale)
{
    if (positions.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot initialize collection with zero positions");
        return;
    }

    CollectionGroup group;
    group.collection = std::make_shared<GpuSync::PointCollectionNode>();
    group.mass_multiplier = mass_multiplier;
    group.color_tint = color_tint;
    group.size_scale = size_scale;

    group.physics_state.resize(positions.size());

    std::vector<GpuSync::PointVertex> vertices;
    vertices.reserve(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        glm::vec3 color = colors.empty() ? glm::vec3(1.0F) : colors[i];

        vertices.push_back(GpuSync::PointVertex {
            .position = positions[i],
            .color = color * color_tint,
            .size = m_point_size * size_scale });

        group.physics_state[i] = PhysicsState {
            .velocity = glm::vec3(0.0F),
            .force = glm::vec3(0.0F),
            .mass = 1.0F * mass_multiplier
        };
    }

    group.collection->set_points(vertices);
    group.collection->compute_frame();

    m_collections.push_back(std::move(group));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added collection #{} with {} points (mass_mult={:.2f}, size_scale={:.2f})",
        m_collections.size(), positions.size(), mass_multiplier, size_scale);
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

std::span<const uint8_t> PhysicsOperator::get_vertex_data_at(uint32_t idx) const
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

const Kakshya::VertexLayout& PhysicsOperator::get_vertex_layout() const
{
    if (m_collections.empty()) {
        static Kakshya::VertexLayout empty_layout;
        return empty_layout;
    }
    return *m_collections[0].collection->get_vertex_layout();
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

//-----------------------------------------------------------------------------
// Parameter Control
//-----------------------------------------------------------------------------

void PhysicsOperator::set_parameter(std::string_view param, double value)
{
    if (param == "gravity_x") {
        m_gravity.x = static_cast<float>(value);
    } else if (param == "gravity_y") {
        m_gravity.y = static_cast<float>(value);
    } else if (param == "gravity_z") {
        m_gravity.z = static_cast<float>(value);
    } else if (param == "drag") {
        m_drag = static_cast<float>(value);
    } else if (param == "interaction_radius") {
        m_interaction_radius = static_cast<float>(value);
    } else if (param == "spring_stiffness") {
        m_spring_stiffness = static_cast<float>(value);
    } else if (param == "point_size") {
        m_point_size = static_cast<float>(value);
        for (auto& group : m_collections) {
            auto& points = group.collection->get_points();
            for (auto& pt : points) {
                pt.size = m_point_size * group.size_scale;
            }
        }
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

void PhysicsOperator::set_bounds(const glm::vec3& min, const glm::vec3& max)
{
    m_bounds_min = min;
    m_bounds_max = max;
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
    constexpr float damping = 0.8F;

    for (auto& group : m_collections) {
        auto& points = group.collection->get_points();

        for (size_t i = 0; i < points.size(); ++i) {
            auto& vertex = points[i];
            auto& state = group.physics_state[i];

            if (vertex.position.x < m_bounds_min.x) {
                vertex.position.x = m_bounds_min.x;
                state.velocity.x *= -damping;
            } else if (vertex.position.x > m_bounds_max.x) {
                vertex.position.x = m_bounds_max.x;
                state.velocity.x *= -damping;
            }

            if (vertex.position.y < m_bounds_min.y) {
                vertex.position.y = m_bounds_min.y;
                state.velocity.y *= -damping;
            } else if (vertex.position.y > m_bounds_max.y) {
                vertex.position.y = m_bounds_max.y;
                state.velocity.y *= -damping;
            }

            if (vertex.position.z < m_bounds_min.z) {
                vertex.position.z = m_bounds_min.z;
                state.velocity.z *= -damping;
            } else if (vertex.position.z > m_bounds_max.z) {
                vertex.position.z = m_bounds_max.z;
                state.velocity.z *= -damping;
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

} // namespace MayaFlux::Nodes::Network
