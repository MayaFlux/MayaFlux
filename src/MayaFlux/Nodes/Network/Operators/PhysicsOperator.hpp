#pragma once
#include "GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Graphics/PointCollectionNode.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Kinesis/VertexSampler.hpp"

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

    ~PhysicsOperator() override { m_shutdown.store(true, std::memory_order_release); }

    /**
     * @brief Initialize with a single physics collection
     * @param vertices PointVertex array with position, color, size
     */
    void initialize(const std::vector<PointVertex>& vertices);

    /**
     * @brief Initialize multiple physics collections
     * @param collections Vector of PointVertex vectors (one per collection)
     */
    void initialize_collections(
        const std::vector<std::vector<PointVertex>>& collections);

    /**
     * @brief Add a single physics collection
     * @param vertices PointVertex array with position, color, size
     * @param mass_multiplier Mass multiplier for all particles in this collection
     */
    void add_collection(
        const std::vector<PointVertex>& vertices,
        float mass_multiplier = 1.0F);

    void process(float dt) override;

    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    /**
     * @brief Extract current vertex data as PointVertex array
     * @return Vector of PointVertex with current positions, colors, sizes
     */
    [[nodiscard]] std::vector<PointVertex> extract_vertices() const;

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
     * @brief Get the interaction radius for physics calculations.
     * @return Interaction radius.
     */
    [[nodiscard]] float get_interaction_radius() const { return m_interaction_radius; }

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
    void set_attraction_strength(float strength) { m_attraction_strength = strength; }

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
     * @brief Add an external force field evaluated per-particle per-frame
     * @param field VectorField: glm::vec3 (position) -> glm::vec3 (force)
     *
     * Fields are evaluated additively alongside existing hardcoded forces
     * (gravity, attraction, turbulence, spatial interactions). Evaluated
     * after gravity, before integration.
     */
    void add_force_field(Kinesis::VectorField field);

    /**
     * @brief Remove all external force fields
     */
    void clear_force_fields();

    /**
     * @brief Adopt this cycle's GPU claim/absorption clustering as bonds.
     * @param claimed_by One entry per particle, global index order: entry i
     *        equals i if i is a cluster root, or the root's index if i was
     *        absorbed into it. Same shape as MutationClaimProcessor's
     *        mutation_claimed_by state field, since it is meant to be called
     *        directly with that field's readback.
     *
     * Bonded (non-root) particles are pulled toward their root each cycle by
     * apply_bond_forces, a spring toward bond_rest_length rather than the
     * root's own absorb_radius, so the CPU-simulated particle stays close to
     * its cluster even on a cycle where the GPU claim graph (fully rebuilt
     * from scratch every cycle, see ClaimInitProcessor) briefly finds no
     * claim for it. Replaces any previously adopted bonds outright: there is
     * no incremental merge, the whole table is the current cycle's snapshot.
     *
     * @note Entries in claimed_by are validated against m_accreted_mass's
     *       size before use as an index; an out-of-range entry is skipped
     *       rather than trusted, since claimed_by comes from a GPU readback.
     */
    void sync_bonds_from_claims(std::span<const uint32_t> claimed_by);

    /**
     * @brief Drop every adopted bond.
     *
     * Called when the claim graph reports nothing claimed this cycle
     * (see ClaimAccumulateProcessor), so stale bonds from an earlier cycle
     * don't keep pulling particles together after they've genuinely drifted
     * apart.
     */
    void clear_bonds();

    /**
     * @brief Spring constant pulling a bonded particle toward its root.
     * @param stiffness Force per unit distance from bond_rest_length.
     */
    void set_bond_stiffness(float stiffness) { m_bond_stiffness = stiffness; }
    [[nodiscard]] float get_bond_stiffness() const { return m_bond_stiffness; }

    /**
     * @brief Distance a bond settles at.
     * @param rest_length Below this distance the bond pushes apart (repel);
     *        above it, the bond pulls together (attract). 0 pulls a bonded
     *        particle fully onto its root, matching the GPU cosmetic swallow.
     */
    void set_bond_rest_length(float rest_length) { m_bond_rest_length = rest_length; }
    [[nodiscard]] float get_bond_rest_length() const { return m_bond_rest_length; }

    /** @brief Number of particles currently bonded to a root other than themselves. */
    [[nodiscard]] size_t bond_count() const;

    /**
     * @brief Enable or disable adopting bonds from sync_bonds_from_claims.
     * @param enable False also clears any bonds currently held, so the
     *        effect is immediate rather than waiting for the next zero-claim
     *        cycle.
     *
     * Purely a toggle for comparing bonded vs. unbonded behaviour live: the
     * GPU claim/swallow cosmetic pass is unaffected either way, since it
     * never reads this flag. Only apply_bond_forces (a real physics effect)
     * is gated by it.
     */
    void enable_bonds(bool enable);

    [[nodiscard]] bool bonds_enabled() const { return m_bonds_enabled; }

    /**
     * @brief This particle's currently accreted mass.
     * @return 1.0 (the seed value every particle starts at) before bonds
     *         have ever synced, or if global_index is out of range.
     *
     * Populated by sync_bonds_from_claims, which is the only place mass
     * moves: a satellite's mass transfers onto its current root every cycle
     * it remains claimed and is never created or destroyed (see that
     * method's own doc). Unlike ClaimAccumulateProcessor's GPU-side
     * swallow_count, which ClaimInitProcessor wipes every cycle, this is a
     * genuine permanent record of accumulated material, suitable for a
     * growth effect that actually takes real time rather than saturating as
     * soon as local density reaches steady state.
     */
    [[nodiscard]] float get_accreted_mass(size_t global_index) const;

    /**
     * @brief Every particle's currently accreted mass, in global index order.
     * @return Empty if there are no particles yet; otherwise always
     *         get_point_count() elements, lazily seeded to 1.0 each on the
     *         first call, the same seeding sync_bonds_from_claims does, so a
     *         caller can upload this from cycle one without waiting for the
     *         first claim event to size it.
     *
     * Not const: may lazily initialise m_accreted_mass on first call.
     */
    [[nodiscard]] std::span<const float> get_accreted_mass_span();

    /**
     * @brief Sum of every particle's accreted mass.
     * @return particle count before bonds have ever synced (each particle
     *         starts at mass 1.0), and exactly that same value forever
     *         after, since transfer neither creates nor destroys mass.
     *         Useful as a caller's own reference point for what fraction of
     *         the whole system a given body currently represents.
     */
    [[nodiscard]] float get_total_mass() const;

    /**
     * @brief Fixed dt substituted when set_force_internal_dt(true).
     * @param dt Timestep in seconds. Default 0.016F.
     */
    void set_internal_dt(float dt) { m_internal_dt = dt; }
    [[nodiscard]] float get_internal_dt() const { return m_internal_dt; }

    /**
     * @brief Get number of active external force fields
     */
    [[nodiscard]] size_t force_field_count() const { return m_force_fields.size(); }

    /**
     * @brief Direct access to collections for advanced per-particle control
     * @warning Only for ParticleNetwork's ONE_TO_ONE parameter mapping
     */
    std::vector<CollectionGroup>& get_collections() { return m_collections; }

    /**
     * @brief Per-particle collection index, global index order.
     * @return get_point_count() elements, entry i equal to the index of the
     *         CollectionGroup particle i belongs to in get_collections()
     *         order. All zero when there is at most one collection.
     *
     * The single source of truth for "which complete vertex-array pack does
     * this particle belong to", consumed by any GPU-side stage that needs
     * to respect collection boundaries in a neighbour query (the spatial
     * hash's ClaimProcessor/HashDensityColorProcessor, and optionally
     * GpuFieldOperator's own cluster-scoped bindings): each such consumer
     * uploads this once, at wiring time, into whatever state field it
     * declares for the purpose, rather than every consumer reaching into
     * get_collections() and re-deriving the same prefix sum independently.
     *
     * Overrides GraphicsOperator::build_cluster_ids(), whose default (every
     * entry 0) is exactly what this returns when there is at most one
     * collection; the override only differs once a second one exists.
     */
    [[nodiscard]] std::vector<uint32_t> build_cluster_ids() const override;

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

    /**
     * @brief Seed physics state from upstream operator's vertex data
     *
     * Extracts positions, colors, sizes from upstream and initializes
     * physics state (velocity = 0, mass = 1) for each particle. Supports
     * PointVertex input; other vertex types are ignored with a warning.
     *
     * @param upstream Upstream operator to seed from
     */
    void seed_from_upstream(const GraphicsOperator* upstream) override;

    const char* get_vertex_type_name() const override { return "PointVertex"; }

protected:
    void* get_data_at(size_t global_index) override;

private:
    std::vector<CollectionGroup> m_collections;
    mutable std::vector<uint8_t> m_vertex_data_aggregate;
    std::vector<Kinesis::VectorField> m_force_fields;

    Kinesis::Stochastic::Stochastic m_random_generator;

    glm::vec3 m_gravity { 0.0F, -9.81F, 0.0F };
    float m_drag { 0.01F };
    float m_interaction_radius { 1.0F };
    float m_spring_stiffness { 0.5F };
    float m_point_size { 5.0F };
    float m_turbulence_strength { 0.0F };
    Kinesis::SamplerBounds m_bounds { .min = glm::vec3 { -10.0F }, .max = glm::vec3 { 10.0F } };
    BoundsMode m_bounds_mode { BoundsMode::BOUNCE };
    bool m_spatial_interactions_enabled {};
    float m_repulsion_strength { 0.5F };

    glm::vec3 m_attraction_point { 0.0F };
    bool m_has_attraction_point { false };
    float m_attraction_strength { 1.0F };
    float m_internal_dt { 0.016F };

    std::vector<uint32_t> m_bond_root; ///< Empty when no bonds are adopted; see sync_bonds_from_claims.
    float m_bond_stiffness { 2.0F };
    float m_bond_rest_length { 0.0F };
    bool m_bonds_enabled { true };

    std::vector<float> m_accreted_mass; ///< Lazily seeded to 1.0 per particle; see sync_bonds_from_claims.

    static std::optional<PhysicsParameter> string_to_parameter(std::string_view param);

    void apply_forces();
    void apply_spatial_interactions();
    void apply_attraction_forces();
    void apply_turbulence();
    void apply_bond_forces();
    void integrate(float dt);
    void handle_boundary_conditions();
    void sync_to_point_collection();

    /**
     * @struct GroupIndex
     * @brief A global particle index resolved to its owning collection.
     */
    struct GroupIndex {
        size_t group;
        size_t local;
    };

    /** @brief Resolve a global index the same way apply_impulse/get_data_at do. */
    [[nodiscard]] std::optional<GroupIndex> resolve_global_index(size_t global_index) const;

    void apply_per_particle_force(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source);

    void apply_per_particle_mass(
        const std::shared_ptr<NodeNetwork>& source);

    mutable std::atomic<uint32_t> m_access_token { 0 };
    std::atomic<bool> m_shutdown { false };
};

} // namespace MayaFlux::Nodes::Network
