#pragma once
#include "GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Graphics/PointCollectionNode.hpp"

#include "MayaFlux/Kinesis/Stochastic.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @enum PhysicsParameter
 * @brief Identifiers for physics parameters that can be set via parameter mapping
 */
enum class PhysicsParameter : uint8_t {
    GRAVITY_X,
    GRAVITY_Y,
    GRAVITY_Z,
    DRAG,
    INTERACTION_RADIUS,
    SPRING_STIFFNESS,
    REPULSION_STRENGTH,
    SPATIAL_INTERACTIONS,
    POINT_SIZE,
    ATTRACTION_STRENGTH,
    TURBULENCE
};

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

    /**
     * @enum BoundsMode
     * @brief How particles behave at spatial bounds
     */
    enum class BoundsMode : uint8_t {
        NONE, ///< No bounds checking
        BOUNCE, ///< Reflect off boundaries with damping
        WRAP, ///< Teleport to opposite side
        CLAMP ///< Stop at boundary
    };

    PhysicsOperator();

    void initialize(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& colors = {}) override;

    /**
     * @brief Initialize multiple physics collections
     * @param collections Vector of PointVertex vectors (one per collection)
     */
    void initialize_collections(
        const std::vector<std::vector<GpuSync::PointVertex>>& collections);

    /**
     * @brief Add a single physics collection
     * @param vertices PointVertex array with position, color, size
     * @param mass_multiplier Mass multiplier for all particles in this collection
     */
    void add_collection(
        const std::vector<GpuSync::PointVertex>& vertices,
        float mass_multiplier = 1.0F);

    void process(float dt) override;

    [[nodiscard]] std::vector<glm::vec3> extract_positions() const override;
    [[nodiscard]] std::vector<glm::vec3> extract_colors() const override;

    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
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
     * @brief Set the current bounds mode.
     * @param mode Bounds mode to set.
     */
    void set_bounds_mode(BoundsMode mode) { m_bounds_mode = mode; }

    /**
     * @brief Enable or disable spatial interactions between particles.
     * @param enable True to enable, false to disable.
     */
    void enable_spatial_interactions(bool enable) { m_spatial_interactions_enabled = enable; }

    /**
     * @brief Set the strength of repulsion between particles when spatial interactions are enabled.
     * @param strength Repulsion strength value.
     */
    void set_repulsion_strength(float strength) { m_repulsion_strength = strength; }

    /**
     * @brief Set the strength of attraction towards the attraction point.
     * @param strength Attraction strength value.
     */
    void set_turbulence_strength(float strength) { m_turbulence_strength = strength; }

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

    /**
     * @brief Get the current bounds mode.
     * @return Current bounds mode.
     */
    [[nodiscard]] BoundsMode get_bounds_mode() const { return m_bounds_mode; }

    /**
     * @brief Check if spatial interactions between particles are enabled.
     * @return True if enabled, false otherwise.
     */
    [[nodiscard]] bool spatial_interactions_enabled() const { return m_spatial_interactions_enabled; }

    /**
     * @brief Get the current repulsion strength for spatial interactions.
     * @return Repulsion strength value.
     */
    [[nodiscard]] float get_repulsion_strength() const { return m_repulsion_strength; }

    void set_attraction_point(const glm::vec3& point);

    void clear_attraction_point() { m_has_attraction_point = false; }

    [[nodiscard]] bool has_attraction_point() const { return m_has_attraction_point; }

    [[nodiscard]] glm::vec3 get_attraction_point() const { return m_attraction_point; }

    /**
     * @brief Apply impulse to all particles
     */
    void apply_global_impulse(const glm::vec3& impulse);

    /**
     * @brief Apply impulse to specific particle
     */
    void apply_impulse(size_t index, const glm::vec3& impulse);

    /**
     * @brief Direct access to collections for advanced per-particle control
     * @warning Only for ParticleNetwork's ONE_TO_ONE parameter mapping
     */
    std::vector<CollectionGroup>& get_collections() { return m_collections; }

    /**
     * @brief Apply ONE_TO_ONE parameter for physics-specific properties
     *
     * Supports:
     * - "force_x/y/z": Per-particle force application
     * - "mass": Per-particle mass
     * - "color": Per-particle color (delegated to base)
     * - "size": Per-particle size (delegated to base)
     */
    void apply_one_to_one(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source) override;

    const char* get_vertex_type_name() const override { return "PointVertex"; }

protected:
    void* get_data_at(size_t global_index) override;

private:
    std::vector<CollectionGroup> m_collections;
    mutable std::vector<uint8_t> m_vertex_data_aggregate;

    Kinesis::Stochastic::Stochastic m_random_generator;

    glm::vec3 m_gravity { 0.0F, -9.81F, 0.0F };
    float m_drag { 0.01F };
    float m_interaction_radius { 1.0F };
    float m_spring_stiffness { 0.5F };
    float m_point_size { 5.0F };
    float m_turbulence_strength { 0.0F };
    glm::vec3 m_bounds_min { -10.0F };
    glm::vec3 m_bounds_max { 10.0F };
    BoundsMode m_bounds_mode { BoundsMode::BOUNCE };
    bool m_spatial_interactions_enabled {};
    float m_repulsion_strength { 0.5F };

    glm::vec3 m_attraction_point { 0.0F };
    bool m_has_attraction_point { false };
    float m_attraction_strength { 1.0F };

    static std::optional<PhysicsParameter> string_to_parameter(std::string_view param);

    void apply_forces();
    void apply_spatial_interactions();
    void apply_attraction_forces();
    void apply_turbulence();
    void integrate(float dt);
    void handle_boundary_conditions();
    void sync_to_point_collection();

    void apply_per_particle_force(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source);

    void apply_per_particle_mass(
        const std::shared_ptr<NodeNetwork>& source);
};

} // namespace MayaFlux::Nodes::Network
