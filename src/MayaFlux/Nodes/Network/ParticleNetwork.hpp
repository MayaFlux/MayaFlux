#pragma once

#include "MayaFlux/Nodes/Graphics/PointNode.hpp"
#include "NodeNetwork.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ParticleNetwork
 * @brief Network of particles with physics simulation
 *
 * CONCEPT:
 * ========
 * N-body particle system where each particle is a PointNode (GeometryWriterNode).
 * Particles can interact based on topology (spatial forces, springs, etc.)
 * or remain independent (ballistic motion).
 *
 * TOPOLOGY USAGE:
 * ===============
 * - INDEPENDENT: No inter-particle forces (pure ballistic motion)
 * - SPATIAL: Particles within radius interact (flocking, springs, repulsion)
 * - GRID_2D/3D: Fixed neighbor relationships (cloth, lattice)
 * - RING/CHAIN: Sequential connections (rope, chain simulation)
 * - CUSTOM: User-defined force topology
 *
 * PARAMETER MAPPING:
 * ==================
 * External nodes can control particle behavior:
 *
 * BROADCAST (one node → all particles):
 * - "gravity": Global gravity strength
 * - "drag": Air resistance coefficient
 * - "turbulence": Chaos/noise strength
 * - "attraction": Global attraction point strength
 *
 * ONE_TO_ONE (network → per-particle control):
 * - "force_x/y/z": Per-particle force application
 * - "color": Per-particle color (from audio spectrum, etc.)
 * - "size": Per-particle size modulation
 * - "mass": Per-particle mass override
 *
 * USAGE:
 * ======
 * ```cpp
 * // Create 1000 particles with spatial interaction
 * auto particles = std::make_shared<ParticleNetwork>(
 *     1000,
 *     glm::vec3(-10, -10, -10),  // bounds min
 *     glm::vec3(10, 10, 10)       // bounds max
 * );
 * particles->set_topology(Topology::SPATIAL);
 * particles->set_interaction_radius(2.0F);
 * particles->set_output_mode(OutputMode::GRAPHICS_BIND);
 *
 * // Audio-reactive turbulence
 * auto mic = vega.AudioInput();
 * auto amplitude = vega.Envelope(mic);
 * particles->map_parameter("turbulence", amplitude, MappingMode::BROADCAST);
 *
 * node_graph_manager->add_network(particles, ProcessingToken::VISUAL_RATE);
 * ```
 */
class MAYAFLUX_API ParticleNetwork : public NodeNetwork {
public:
    /**
     * @struct ParticleNode
     * @brief Single particle with physics state
     */
    struct ParticleNode {
        std::shared_ptr<GpuSync::PointNode> point; ///< Actual GeometryWriterNode

        glm::vec3 velocity { 0.0F }; ///< Current velocity
        glm::vec3 acceleration { 0.0F }; ///< Current acceleration
        glm::vec3 force { 0.0F }; ///< Accumulated force this frame

        float mass = 1.0F; ///< Particle mass
        size_t index; ///< Index in network
    };

    /**
     * @enum BoundsMode
     * @brief How particles behave at spatial bounds
     */
    enum class BoundsMode : uint8_t {
        NONE, ///< No bounds checking
        BOUNCE, ///< Reflect off boundaries
        WRAP, ///< Teleport to opposite side
        CLAMP, ///< Stop at boundary
        DESTROY ///< Remove particle at boundary (respawn optional)
    };

    /**
     * @enum InitializationMode
     * @brief Particle spawn distribution
     */
    enum class InitializationMode : uint8_t {
        RANDOM_VOLUME, ///< Random positions in bounds volume
        RANDOM_SURFACE, ///< Random positions on bounds surface
        GRID, ///< Regular grid distribution
        SPHERE_VOLUME, ///< Random in sphere
        SPHERE_SURFACE, ///< Random on sphere surface
        CUSTOM ///< User-provided initialization function
    };

    //-------------------------------------------------------------------------
    // Construction
    //-------------------------------------------------------------------------

    /**
     * @brief Create particle network with spatial bounds
     * @param num_particles Number of particles
     * @param bounds_min Minimum spatial extent
     * @param bounds_max Maximum spatial extent
     * @param init_mode Initialization distribution
     */
    ParticleNetwork(
        size_t num_particles,
        const glm::vec3& bounds_min = glm::vec3(-10.0F),
        const glm::vec3& bounds_max = glm::vec3(10.0F),
        InitializationMode init_mode = InitializationMode::RANDOM_VOLUME);

    //-------------------------------------------------------------------------
    // NodeNetwork Interface Implementation
    //-------------------------------------------------------------------------

    void process_batch(unsigned int num_samples) override;

    [[nodiscard]] size_t get_node_count() const override
    {
        return m_particles.size();
    }

    void initialize() override;
    void reset() override;

    [[nodiscard]] std::unordered_map<std::string, std::string>
    get_metadata() const override;

    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;

    //-------------------------------------------------------------------------
    // Parameter Mapping
    //-------------------------------------------------------------------------

    void map_parameter(
        const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    void map_parameter(
        const std::string& param_name,
        const std::shared_ptr<NodeNetwork>& source_network) override;

    void unmap_parameter(const std::string& param_name) override;

    //-------------------------------------------------------------------------
    // Particle Access
    //-------------------------------------------------------------------------

    /**
     * @brief Get all particles (read-only for NetworkGeometryBuffer)
     */
    [[nodiscard]] const std::vector<ParticleNode>& get_particles() const
    {
        return m_particles;
    }

    /**
     * @brief Get specific particle
     */
    [[nodiscard]] const ParticleNode& get_particle(size_t index) const
    {
        return m_particles.at(index);
    }

    /**
     * @brief Get mutable particle (for custom manipulation)
     */
    [[nodiscard]] ParticleNode& get_particle_mut(size_t index)
    {
        return m_particles.at(index);
    }

    //-------------------------------------------------------------------------
    // Physics Configuration
    //-------------------------------------------------------------------------

    /**
     * @brief Set global gravity vector
     */
    void set_gravity(const glm::vec3& gravity) { m_gravity = gravity; }

    /**
     * @brief Get current gravity
     */
    [[nodiscard]] glm::vec3 get_gravity() const { return m_gravity; }

    /**
     * @brief Set drag coefficient (0.0 = no drag, 1.0 = full drag)
     */
    void set_drag(float drag) { m_drag = glm::clamp(drag, 0.0F, 1.0F); }

    /**
     * @brief Get current drag
     */
    [[nodiscard]] float get_drag() const { return m_drag; }

    /**
     * @brief Set spatial bounds
     */
    void set_bounds(const glm::vec3& min, const glm::vec3& max)
    {
        m_bounds_min = min;
        m_bounds_max = max;
    }

    /**
     * @brief Set bounds behavior
     */
    void set_bounds_mode(BoundsMode mode) { m_bounds_mode = mode; }

    /**
     * @brief Set interaction radius (for SPATIAL topology)
     */
    void set_interaction_radius(float radius)
    {
        m_interaction_radius = radius;
        m_neighbor_map_dirty = true;
    }

    /**
     * @brief Set spring stiffness (for SPATIAL/GRID topologies)
     */
    void set_spring_stiffness(float stiffness)
    {
        m_spring_stiffness = stiffness;
    }

    /**
     * @brief Set repulsion strength (for SPATIAL topology)
     */
    void set_repulsion_strength(float strength)
    {
        m_repulsion_strength = strength;
    }

    /**
     * @brief Set attraction point (particles pulled toward this point)
     */
    void set_attraction_point(const glm::vec3& point)
    {
        m_attraction_point = point;
        m_has_attraction_point = true;
    }

    /**
     * @brief Disable attraction point
     */
    void disable_attraction_point()
    {
        m_has_attraction_point = false;
    }

    /**
     * @brief Set time step for physics integration
     */
    void set_timestep(float dt) { m_timestep = dt; }

    //-------------------------------------------------------------------------
    // Particle Control
    //-------------------------------------------------------------------------

    /**
     * @brief Apply impulse to all particles
     */
    void apply_global_impulse(const glm::vec3& impulse);

    /**
     * @brief Apply impulse to specific particle
     */
    void apply_impulse(size_t index, const glm::vec3& impulse);

    /**
     * @brief Reinitialize all particle positions
     */
    void reinitialize_positions(InitializationMode mode);

    /**
     * @brief Reset all velocities to zero
     */
    void reset_velocities();

private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------

    std::vector<ParticleNode> m_particles;

    // Physics parameters
    glm::vec3 m_gravity { 0.0F, -9.8F, 0.0F };
    float m_drag = 0.01F;
    float m_timestep = 0.016F; // ~60Fps default

    // Spatial bounds
    glm::vec3 m_bounds_min { -10.0F };
    glm::vec3 m_bounds_max { 10.0F };
    BoundsMode m_bounds_mode = BoundsMode::BOUNCE;

    // Interaction parameters
    float m_interaction_radius = 2.0F;
    float m_spring_stiffness = 0.1F;
    float m_repulsion_strength = 0.5F;

    // Attraction point
    glm::vec3 m_attraction_point { 0.0F };
    bool m_has_attraction_point = false;
    float m_attraction_strength = 1.0F;

    // Initialization
    InitializationMode m_init_mode;

    // Neighbor tracking (for SPATIAL topology)
    std::unordered_map<size_t, std::vector<size_t>> m_neighbor_map;
    bool m_neighbor_map_dirty = true;

    //-------------------------------------------------------------------------
    // Physics Simulation
    //-------------------------------------------------------------------------

    /**
     * @brief Clear accumulated forces
     */
    void clear_forces();

    /**
     * @brief Apply gravity to all particles
     */
    void apply_gravity();

    /**
     * @brief Apply drag forces
     */
    void apply_drag();

    /**
     * @brief Apply interaction forces based on topology
     */
    void apply_interaction_forces();

    /**
     * @brief Apply attraction point force
     */
    void apply_attraction_force();

    /**
     * @brief Integrate forces → velocities → positions
     */
    void integrate(float dt);

    /**
     * @brief Handle boundary conditions
     */
    void handle_bounds();

    /**
     * @brief Update PointNode states from physics
     */
    void update_point_nodes();

    //-------------------------------------------------------------------------
    // Topology Management
    //-------------------------------------------------------------------------

    /**
     * @brief Rebuild neighbor map for SPATIAL topology
     */
    void rebuild_spatial_neighbors();

    /**
     * @brief Get neighbors for a particle based on current topology
     */
    [[nodiscard]] std::vector<size_t> get_neighbors(size_t index) const;

    //-------------------------------------------------------------------------
    // Initialization Helpers
    //-------------------------------------------------------------------------

    /**
     * @brief Initialize particles based on mode
     */
    void initialize_particle_positions(InitializationMode mode);

    /**
     * @brief Random position in bounds volume
     */
    [[nodiscard]] glm::vec3 random_position_volume() const;

    /**
     * @brief Random position on bounds surface
     */
    [[nodiscard]] glm::vec3 random_position_surface() const;

    /**
     * @brief Random position in sphere
     */
    [[nodiscard]] glm::vec3 random_position_sphere(float radius) const;

    /**
     * @brief Random position on sphere surface
     */
    [[nodiscard]] glm::vec3 random_position_sphere_surface(float radius) const;

    //-------------------------------------------------------------------------
    // Parameter Mapping Helpers
    //-------------------------------------------------------------------------

    /**
     * @brief Update mapped parameters before physics step
     */
    void update_mapped_parameters();

    /**
     * @brief Apply broadcast parameter to all particles
     */
    void apply_broadcast_parameter(const std::string& param, double value);

    /**
     * @brief Apply one-to-one parameter from another network
     */
    void apply_one_to_one_parameter(
        const std::string& param,
        const std::shared_ptr<NodeNetwork>& source);
};

} // namespace MayaFlux::Nodes::Network
