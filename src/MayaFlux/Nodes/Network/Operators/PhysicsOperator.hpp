#pragma once
#include "GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Graphics/PointCollectionNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct PhysicsState
 * @brief Physics-specific data parallel to PointVertex array
 *
 * Stored separately to avoid polluting vertex types with
 * physics data. Indexed in parallel with PointCollectionNode's
 * internal vertex array.
 */
struct PhysicsState {
    glm::vec3 velocity { 0.0F };
    glm::vec3 force { 0.0F };
    float mass { 1.0F };
};

/**
 * @class PhysicsOperator
 * @brief N-body physics simulation with point rendering
 *
 * Delegates rendering to PointCollectionNode. Physics state
 * (velocity, force, mass) stored in parallel array. Each frame:
 * 1. Apply forces
 * 2. Integrate motion
 * 3. Update PointCollectionNode vertices
 * 4. PointCollectionNode handles GPU upload
 */
class MAYAFLUX_API PhysicsOperator : public GraphicsOperator {
public:
    struct CollectionGroup {
        std::shared_ptr<GpuSync::PointCollectionNode> collection;
        std::vector<PhysicsState> physics_state;

        float mass_multiplier;
        glm::vec3 color_tint;
        float size_scale;
    };

    PhysicsOperator();

    void initialize(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& colors = {}) override;

    /**
     * @brief Initialize a new physics collection with given positions and colors.
     * @param positions Particle positions.
     * @param colors Particle colors (optional).
     * @param mass_multiplier Multiplier for particle mass.
     * @param color_tint Color tint for the collection.
     * @param size_scale Size scale for the collection.
     */
    void initialize_collection(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& colors = {},
        float mass_multiplier = 1.0F,
        glm::vec3 color_tint = glm::vec3(1.0F),
        float size_scale = 1.0F);

    void process(float dt) override;

    [[nodiscard]] std::vector<glm::vec3> extract_positions() const override;
    [[nodiscard]] std::vector<glm::vec3> extract_colors() const override;

    [[nodiscard]] std::span<const uint8_t> get_vertex_data_at(uint32_t idx) const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] const Kakshya::VertexLayout& get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Physics"; }

    [[nodiscard]] size_t get_point_count() const override;

    /**
     * @brief Set the gravity vector.
     * @param gravity Gravity vector.
     */
    void set_gravity(const glm::vec3& gravity) { m_gravity = gravity; }

    /**
     * @brief Set the drag coefficient.
     * @param drag Drag value.
     */
    void set_drag(float drag) { m_drag = drag; }

    /**
     * @brief Set the interaction radius for physics calculations.
     * @param radius Interaction radius.
     */
    void set_interaction_radius(float radius) { m_interaction_radius = radius; }

    /**
     * @brief Set the spring stiffness for interactions.
     * @param stiffness Spring stiffness value.
     */
    void set_spring_stiffness(float stiffness) { m_spring_stiffness = stiffness; }

    /**
     * @brief Set the simulation bounds.
     * @param min Minimum bounds.
     * @param max Maximum bounds.
     */
    void set_bounds(const glm::vec3& min, const glm::vec3& max);

    /**
     * @brief Set the rendered point size.
     * @param size Point size.
     */
    void set_point_size(float size) { m_point_size = size; }

    /**
     * @brief Get velocity magnitude for specific particle
     * @param global_index Particle index across all collections
     */
    [[nodiscard]] std::optional<double> get_particle_velocity(size_t global_index) const;

    /**
     * @brief Get the current gravity vector.
     * @return Gravity vector.
     */
    [[nodiscard]] glm::vec3 get_gravity() const { return m_gravity; }

    /**
     * @brief Get the current drag coefficient.
     * @return Drag value.
     */
    [[nodiscard]] float get_drag() const { return m_drag; }

private:
    std::vector<CollectionGroup> m_collections;
    mutable std::vector<uint8_t> m_vertex_data_aggregate;

    glm::vec3 m_gravity { 0.0F, -9.81F, 0.0F };
    float m_drag { 0.01F };
    float m_interaction_radius { 1.0F };
    float m_spring_stiffness { 0.5F };
    float m_point_size { 5.0F };
    glm::vec3 m_bounds_min { -10.0F };
    glm::vec3 m_bounds_max { 10.0F };

    void apply_forces();
    void integrate(float dt);
    void handle_boundary_conditions();
    void sync_to_point_collection();
};

} // namespace MayaFlux::Nodes::Network::Operators
