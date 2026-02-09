#pragma once

#include "NodeNetwork.hpp"
#include "Operators/GraphicsOperator.hpp"

#include "MayaFlux/Kinesis/Stochastic.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ParticleNetwork
 * @brief Motion-focused point network with swappable operators
 *
 * Philosophy: ParticleNetwork is for MOTION. Operators define how
 * points move (physics, flocking, fields). When you need connectivity
 * visualization (topology, paths), use PointCloudNetwork instead.
 *
 * Supported operators (motion-based):
 * - PhysicsOperator: Ballistic motion, gravity, springs, collisions
 * - FieldOperator: Flow fields, attractors, force fields (future)
 * - FlockingOperator: Boids, swarm intelligence (future)
 *
 * Unsupported operators (connectivity-based):
 * - TopologyOperator: Technically works, but use PointCloudNetwork
 * - PathOperator: Makes no semantic sense for particles
 *
 * PARAMETER MAPPING:
 * ==================
 * External nodes can control motion behavior:
 *
 * BROADCAST (one node → all particles):
 * - "gravity_x/y/z": Gravity components
 * - "drag": Air resistance coefficient
 * - "turbulence": Chaos/noise strength
 * - "interaction_radius": Spatial interaction distance
 * - "spring_stiffness": Spring force strength
 *
 * USAGE:
 * ======
 * ```cpp
 * auto particles = vega.ParticleNetwork(1000);
 * auto physics = particles->create_operator<PhysicsOperator>();
 * physics->set_gravity(glm::vec3(0, -9.81, 0));
 * physics->set_interaction_radius(2.0F);
 *
 * // Audio-reactive turbulence
 * auto chaos = vega.Random() | Audio;
 * particles->map_parameter("turbulence", chaos, MappingMode::BROADCAST);
 *
 * // Runtime switch to field-driven motion
 * auto field = particles->create_operator<FieldOperator>();
 * field->add_attractor(glm::vec3(0, 0, 0), 5.0F);
 * ```
 */
class MAYAFLUX_API ParticleNetwork : public NodeNetwork {
public:
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
    // NodeNetwork Interface
    //-------------------------------------------------------------------------

    void initialize() override;
    void reset() override;
    void process_batch(unsigned int num_samples) override;

    /**
     * @brief Get number of particles in network
     *
     * Returns total particle count across all internal collections.
     * For ParticleNetwork, "nodes" ARE particles - each particle is
     * a mappable entity for parameter control.
     */
    [[nodiscard]] size_t get_node_count() const override;

    /**
     * @brief Get point count (alias for get_node_count)
     *
     * Provided for clarity in graphics contexts. For ParticleNetwork,
     * points and nodes are the same concept.
     */
    [[nodiscard]] size_t get_point_count() const { return get_node_count(); }

    /**
     * @brief Get output value for specific particle
     * @param index Particle index (0 to get_node_count()-1)
     *
     * Returns per-particle output (e.g., velocity magnitude).
     * Used for ONE_TO_ONE parameter mapping from this network
     * to other networks.
     */
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;

    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    //-------------------------------------------------------------------------
    // Operator Management
    //-------------------------------------------------------------------------

    /**
     * @brief Set active operator (runtime switching)
     */
    void set_operator(std::unique_ptr<NetworkOperator> op);

    /**
     * @brief Create and set operator in one call
     */
    template <typename OpType, typename... Args>
    OpType* create_operator(Args&&... args)
    {
        auto op = std::make_unique<OpType>(std::forward<Args>(args)...);
        auto* ptr = op.get();
        set_operator(std::move(op));
        return ptr;
    }

    /**
     * @brief Get current operator with type-safe cast
     */
    template <typename OpType>
    OpType* as_operator()
    {
        return dynamic_cast<OpType*>(m_operator.get());
    }

    template <typename OpType>
    const OpType* as_operator() const
    {
        return dynamic_cast<const OpType*>(m_operator.get());
    }

    NetworkOperator* get_operator() { return m_operator.get(); }
    const NetworkOperator* get_operator() const { return m_operator.get(); }
    bool has_operator() const { return m_operator != nullptr; }

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------

    void set_timestep(float dt) { m_timestep = dt; }
    float get_timestep() const { return m_timestep; }

    void set_bounds(const glm::vec3& min, const glm::vec3& max)
    {
        m_bounds_min = min;
        m_bounds_max = max;
    }

    glm::vec3 get_bounds_min() const { return m_bounds_min; }
    glm::vec3 get_bounds_max() const { return m_bounds_max; }

    void set_topology(Topology topology) override;

    //-------------------------------------------------------------------------
    // Parameter Mapping (Delegates to Operator)
    //-------------------------------------------------------------------------

    void map_parameter(
        const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    void unmap_parameter(const std::string& param_name) override;

private:
    std::unique_ptr<NetworkOperator> m_operator;
    Kinesis::Stochastic::Stochastic m_random_gen;

    size_t m_num_points;
    glm::vec3 m_bounds_min;
    glm::vec3 m_bounds_max;
    InitializationMode m_init_mode;
    float m_timestep { 0.016F };

    void ensure_initialized();
    std::vector<glm::vec3> generate_initial_positions();
    glm::vec3 generate_single_position(InitializationMode mode, size_t index, size_t total);

    //-------------------------------------------------------------------------
    // Parameter Mapping Helpers
    //-------------------------------------------------------------------------

    /**
     * @brief Update mapped parameters before physics step
     */
    void update_mapped_parameters();
};

} // namespace MayaFlux::Nodes::Network
