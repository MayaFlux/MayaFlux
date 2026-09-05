#pragma once

#include "NetworkOperator.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Kinesis/Tendency/DualField.hpp"
#include "MayaFlux/Kinesis/Tendency/FieldBinding.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Nodes::Network {

using FieldTarget = Kinesis::FieldTarget;

/**
 * @struct SpatialFieldConfig
 * @brief Declarative configuration for the GPU-side spatial-relational work a
 *        GpuFieldOperator can drive beyond plain field bindings: uniform-grid
 *        spatial hashing, neighbour-density colouring, the deterministic
 *        claim/absorption protocol, and opt-in population dynamics.
 *
 * Domain-neutral. Every stage this selects operates on the vertex records a
 * GraphicsOperator produces, addressed through that operator's VertexLayout,
 * with no dependency on what kind of network or primary operator sits behind
 * them: a PhysicsOperator on a ParticleNetwork is one source, a
 * TopologyOperator or PathOperator on a PointCloudNetwork is another. A
 * handful of fields draw on concepts only a PhysicsOperator supplies
 * (accreted mass, collection boundaries) and degrade to a documented no-op
 * when none is present; each such field says so in its own entry.
 *
 * Filled once, at construction, with designated initializers: the same
 * shape as VolumeGridBuffer::FlowConfig, a plain value handed over once
 * rather than a sequence of enable_x() calls.
 *
 * Two kinds of field here, deliberately not distinguished by type but
 * documented per-field: cell_size/absorb_radius/cosmetic_swallow/density_color
 * are structural, as they decide *which* processors the geometry buffer
 * constructs in the first place, so changing them after wiring would mean
 * tearing down and rebuilding part of the chain, and there is no live path
 * for that yet. Every other field is a tuning value read by a processor
 * that exists regardless of its value; those do have a live path, through
 * the setters on whichever GpuFieldOperator subclass carries this config
 * (today ParticleFieldOperator; see each field's own doc for which setter).
 */
struct SpatialFieldConfig {
    /**
     * Spatial hash cell size. Nullopt derives it from the network's primary
     * operator when that operator is a PhysicsOperator (its
     * get_interaction_radius()); any other primary operator has no
     * equivalent concept and must set this explicitly. Only consulted when
     * absorb_radius or density_color below actually needs the hash built.
     * Structural: fixed at construction.
     */
    std::optional<float> cell_size;

    /**
     * Enables the deterministic claim/absorption pipeline at this distance.
     * Nullopt (the default) disables mutation entirely. Implies the
     * spatial hash is built regardless of density_color. Structural: fixed
     * at construction.
     */
    std::optional<float> absorb_radius;

    /**
     * Scales a claimant's effective capture radius by the cube root of its
     * own currently accreted mass (mutation_accreted_mass, uploaded fresh
     * each cycle from PhysicsOperator::get_accreted_mass_span when a
     * PhysicsOperator drives the network): max(absorb_radius,
     * cbrt(accreted_mass) * capture_growth). 0 (the default) makes every
     * point claim at the fixed absorb_radius regardless of accreted mass.
     * With no PhysicsOperator in the chain nothing maintains the accreted
     * mass tally, so this stays inert whatever its value: see
     * ClaimAccumulateProcessor.
     *
     * A fixed absorb_radius alone caps how much territory any body can ever
     * sweep, however much mass it already holds, so growth plateaus as soon
     * as that fixed neighbourhood is exhausted; scaling capture radius with
     * mass instead makes accretion self-reinforcing. Cube root specifically,
     * not mass directly: captured-mass rate scales with capture_radius
     * cubed (a 3D volume query), so a radius proportional to mass directly
     * gives a growth rate proportional to mass cubed, which reaches
     * infinite mass in finite time for ANY nonzero capture_growth -- not a
     * gradual curve, an instantaneous one-cycle explosion at a delay this
     * value controls. Cube-rooting mass first (the same real-world
     * reasoning PhysicsOperator::apply_bond_forces already uses for
     * physical spread) keeps the growth rate proportional to mass itself:
     * ordinary exponential growth, with a genuine, live-tunable doubling
     * time rather than a hidden singularity. Live-tunable via
     * set_capture_growth().
     */
    float capture_growth { 0.0F };

    /**
     * Whether ClaimSwallowProcessor's cosmetic pass (position snap onto
     * root, size/colour from this cycle's swallow_count) is added to the
     * chain. Default true: existing absorb_radius behaviour is unchanged.
     *
     * Set false when a caller wants claim resolution purely for its CPU
     * side effect, PhysicsOperator::sync_bonds_from_claims (bond force and
     * the persistent accreted-mass tally it maintains), without the GPU
     * cosmetic pass fighting it: swallow_count resets every cycle (see
     * ClaimInitProcessor), so it can only ever represent this instant's
     * local cluster size, never true accumulation over time. A caller after
     * real, permanent growth reads PhysicsOperator's accreted mass instead
     * and drives size/colour from that on the CPU side, and doesn't want
     * ClaimSwallowProcessor overwriting the same vertices with its own,
     * necessarily transient, numbers. ClaimInit/Claim/Flatten/Accumulate
     * still run regardless: they are what ClaimAccumulateProcessor's
     * readback needs to populate PhysicsOperator's bonds at all. Structural:
     * fixed at construction.
     */
    bool cosmetic_swallow { true };

    /**
     * Enables neighbour-density colouring (ember-to-white-hot ramp).
     * Structural: fixed at construction.
     */
    bool density_color { false };

    /**
     * HashDensityColorProcessor: neighbour count at which the density ramp
     * saturates to fully warm. Lower values make sparser clusters read as
     * dense; live-tunable via set_density_saturation_count().
     */
    float density_saturation_count { 24.0F };

    /**
     * ClaimSwallowProcessor: a survivor's point size the cycle it has
     * swallowed nothing. Live-tunable via set_swallow_base_size().
     */
    float swallow_base_size { 10.0F };

    /**
     * ClaimSwallowProcessor: point-size increase per particle a survivor
     * swallowed this cycle. Live-tunable via set_swallow_growth_rate().
     */
    float swallow_growth_rate { 0.6F };

    /**
     * ClaimSwallowProcessor: point-size ceiling regardless of swallow
     * count, so one runaway cluster can't dominate the screen.
     * Live-tunable via set_swallow_max_size().
     */
    float swallow_max_size { 70.0F };

    /**
     * ClaimSwallowProcessor: brightness multiplier applied to an absorbed
     * particle's cluster colour (0 invisible, 1 as bright as the
     * survivor). Live-tunable via set_swallow_dim_factor().
     */
    float swallow_dim_factor { 0.08F };

    /**
     * Whether ClaimProcessor and HashDensityColorProcessor may see across
     * cluster boundaries. False (the default) means a point in one cluster
     * can never claim, be claimed by, or be counted as a density neighbour
     * of a point in another: every hash-based neighbour query they run is
     * scoped to hash_cluster_id[i], the per-vertex field
     * NetworkGeometryBuffer::ensure_cluster_ids() derives from whatever
     * GraphicsOperator::build_cluster_ids() returns (a PhysicsOperator tags
     * one cluster per collection, a TopologyOperator or PathOperator one per
     * set; every entry 0, and this guard a no-op, when the operator holds a
     * single set). True restores the single global neighbourhood both
     * processors used before hash_cluster_id existed: every point in the
     * buffer is a candidate for every other, regardless of cluster.
     * Live-tunable via set_cross_cluster().
     */
    bool cross_cluster { false };

    /**
     * Fraction of extra reserve capacity the geometry buffer allocates
     * beyond the live point count, for PopulationSpawnProcessor to claim:
     * reserve_count = ceil(live_count * reserve_fraction), and the
     * vertex buffer plus every hash/claim/mutation state field is sized to
     * live_count + reserve_count from the moment this operator is wired,
     * never resized again afterward. 0 (the default) allocates no reserve
     * and wires none of PopulationInitProcessor/PopulationSpawnProcessor/the
     * alive-gating on HashCountProcessor/HashScatterProcessor/ClaimProcessor/
     * ClaimAccumulateProcessor at all -- population dynamics is entirely
     * opt-in and costs nothing unset.
     *
     * Requires absorb_radius to also be set: destruction is destroy-on-
     * absorption, reusing mutation_claimed_by, so there is nothing for this
     * to destroy without claims running. A nonzero reserve_fraction with no
     * absorb_radius is treated as unset and logged.
     *
     * What spawns and dies here is deliberately not reflected to
     * PhysicsOperator: see PopulationConfig's own doc for the full
     * reasoning. Structural: fixed at construction, like absorb_radius
     * itself, since it changes which processors get built.
     */
    float reserve_fraction { 0.0F };

    /**
     * PopulationSpawnProcessor: real neighbour count (same 27-cell query
     * HashDensityColorProcessor performs) a live point must clear before
     * it spawns a copy of itself into a fresh reserve slot. Only consulted
     * when reserve_fraction enables population dynamics at all.
     * Live-tunable via set_spawn_density_threshold().
     */
    float spawn_density_threshold { 30.0F };
};

/**
 * @class GpuFieldOperator
 * @brief Chain operator that declares Tendency field deformation as a compute
 *        shader rather than evaluating it on the CPU.
 *
 * Holds no vertex data and no Vulkan objects. It owns bindings and emits a
 * ShaderSpec; a ComputeProcessor in the buffer chain owns the dispatch. This is
 * a NetworkOperator rather than a GraphicsOperator, so
 * NetworkGeometryProcessor's dynamic_cast fails and the operator contributes an
 * empty render slice without any participates_in_rendering bookkeeping.
 *
 * Vertex-agnostic. Addressing is derived from the supplied VertexLayout, whose
 * attribute offsets are identical across for_points, for_lines, for_meshes and
 * for_raw, so one operator serves any of them. Pass for_raw(stride) for
 * pre-packed data that fits no struct.
 *
 * Bound fields are DualFields: the same authored text that FieldOperator would
 * consume through .cpu is emitted here through .source as a GLSL function. A
 * field can be moved between the two operators without being rewritten.
 *
 * Field domain is always the vertex position read from the record, matching
 * Tendency<glm::vec3, R>. FieldMode::ABSOLUTE is therefore only meaningful when
 * the target is not POSITION: writing an absolute position would require a
 * reference copy of the original vertices, which this operator does not
 * allocate. bind() rejects that combination.

 * Consumers compile a pipeline from build_spec() and hold it. revision()
 * increments whenever that spec changes, so a consumer records the revision it
 * built against and rebuilds when it differs. Rebinding is authoring-time work
 * and is not synchronised against a running process_batch, matching
 * OperatorChain's contract.
 *
 * @code
 * namespace MayaFlux::Fields {
 * using namespace MayaFlux::ShaderCompat;
 * const auto swirl = MF_FIELD(swirl, [](vec3 p) -> vec3 {
 *     return cross(vec3(0.0f, 1.0f, 0.0f), p) * 2.0f;
 * });
 * }
 *
 * auto op = net->get_operator_chain()->emplace<GpuFieldOperator>(
 *     Kakshya::VertexLayout::for_points());
 * op->bind(FieldTarget::POSITION | FieldTarget::NORMAL, Fields::swirl);
 * @endcode
 */
class MAYAFLUX_API GpuFieldOperator : public NetworkOperator {
public:
    /**
     * @param layout Vertex layout describing the record the shader will write.
     *               stride_bytes and every bound attribute's offset_in_vertex
     *               must be divisible by 4; construction fails loudly otherwise.
     */
    explicit GpuFieldOperator(Kakshya::VertexLayout layout);

    ~GpuFieldOperator() override = default;

    // -------------------------------------------------------------------------
    // Field binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a three-component field to one or more vec3 targets.
     * @param target Mask over POSITION, COLOR, NORMAL and TANGENT.
     * @param field  Dual-source field with a usable shader half.
     * @param cluster Nullopt (the default) applies the field to every vertex
     *        regardless of which cluster it belongs to, the
     *        only behaviour that existed before this parameter. A value
     *        restricts the field to vertices whose cluster id equals it: see
     *        needs_cluster_id() for what that costs a consumer that never
     *        uses this.
     *
     * Binding nothing and returning is the response to an empty mask, a bit the
     * layout does not carry, a component-count mismatch on any bit, a field
     * whose shader half failed to parse, or a function name already emitted
     * with a different body. Validation covers the whole mask before anything
     * is stored, so a partly-valid mask binds nothing rather than part.
     *
     * Several fields may drive the same target. They sum before the write,
     * matching FieldOperator, and NORMAL and TANGENT normalise after the sum.
     * A cluster-scoped field only contributes to that sum for vertices in its
     * own cluster; an unscoped field sharing the target still contributes to
     * every vertex regardless.
     */
    void bind(FieldTarget target, const Kinesis::DualVectorField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Bind a scalar field. Target must be exactly SCALAR.
     * @param cluster See the DualVectorField overload's own doc.
     */
    void bind(FieldTarget target, const Kinesis::DualSpatialField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Bind a two-component field. Target must be exactly UV.
     * @param cluster See the DualVectorField overload's own doc.
     */
    void bind(FieldTarget target, const Kinesis::DualUVField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Bind a three-component field of position and time.
     * @param target Mask over POSITION, COLOR, NORMAL and TANGENT.
     * @param field  Temporal field with a usable shader half.
     * @param cluster See the DualVectorField overload's own doc.
     *
     * Validation matches the DualField overload. The emitted function takes a
     * second float argument supplied from the time push constant.
     */
    void bind(FieldTarget target, const Kinesis::TemporalVectorField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Bind a scalar field of position and time. Target must be SCALAR.
     * @param cluster See the DualVectorField overload's own doc.
     */
    void bind(FieldTarget target, const Kinesis::TemporalSpatialField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Bind a two-component field of position and time. Target must be UV.
     * @param cluster See the DualVectorField overload's own doc.
     */
    void bind(FieldTarget target, const Kinesis::TemporalUVField& field,
        std::optional<uint32_t> cluster = std::nullopt);

    /**
     * @brief Clear the given targets.
     *
     * Removes each bit in the mask from every binding that carries it. A
     * binding left with no targets is dropped; one that still drives another
     * target survives. Cluster scope plays no part in the match: unbind(t)
     * removes every binding driving t, scoped or not.
     */
    void unbind(FieldTarget target);

    /**
     * @brief Number of bound fields. One binding may drive several targets.
     */
    [[nodiscard]] size_t binding_count() const { return m_bindings.size(); }

    /**
     * @brief Whether build_spec() needs a per-vertex cluster id this cycle.
     * @return True only if at least one current binding was given a cluster
     *         argument. False for every operator that has never called a
     *         cluster-scoped bind() overload -- the ordinary case -- in
     *         which build_spec() emits exactly the shader it always has,
     *         with no cluster_id binding, no extra SSBO read, and no extra
     *         branch anywhere in the kernel. VertexFieldProcessor checks
     *         this to decide whether it needs to resolve and bind a
     *         hash_cluster_id state field at all.
     */
    [[nodiscard]] bool needs_cluster_id() const;

    /**
     * @brief Descriptor binding index the vertex SSBO occupies.
     *
     * Must match what the owning processor pushes. Default 0.
     */
    void set_vertex_binding(uint32_t binding);

    /**
     * @brief Get the descriptor binding index the vertex SSBO occupies.
     */
    [[nodiscard]] uint32_t get_vertex_binding() const noexcept { return m_vertex_binding; }

    /**
     * @brief Workgroup size along x. Default 256.
     */
    void set_workgroup_size(uint32_t x);

    /**
     * @brief Monotonic counter incremented whenever build_spec()'s result changes.
     *
     * A consumer that compiles a pipeline from build_spec() records the
     * revision it built against and rebuilds when it differs. A bool would not
     * survive two consumers, since the first to observe it would clear it.
     */
    [[nodiscard]] uint64_t revision() const noexcept { return m_revision; }

    // -------------------------------------------------------------------------
    // Shader
    // -------------------------------------------------------------------------

    /**
     * @brief Assemble the compute spec for the current bindings.
     *
     * Emits one GLSL function per distinct bound field, then a kernel that
     * guards against the dispatch tail, reads the position once, and writes
     * every touched target in a single pass. Fields sharing a target sum
     * before the write; NORMAL and TANGENT normalise after the sum. POSITION
     * accumulates onto the existing vertex, every other target is assigned,
     * matching FieldOperator.
     *
     * The kernel addresses a range rather than the whole buffer, since a
     * NetworkGeometryBuffer aggregates one slice per producing operator. The
     * range and the record stride arrive as push constants written by the
     * processor, so neither the caller nor the field author offsets anything,
     * and a layout or slice change needs no recompile. Attribute offsets are
     * baked, since those describe the record the spec was built against.
     *
     * Cached until a bind, unbind or configuration change invalidates it.
     * Returns nullopt when nothing is bound or the layout carries no position
     * attribute.
     *
     * A cluster_id SSBO binding and the read that feeds it are emitted only
     * when needs_cluster_id() is true: an operator with no cluster-scoped
     * binding gets exactly the shader it always has, unchanged.
     */
    [[nodiscard]] std::optional<Portal::Graphics::ShaderSpec> build_spec() const;

    /**
     * @brief Layout the emitted shader addresses.
     */
    [[nodiscard]] const Kakshya::VertexLayout& get_layout() const { return m_layout; }

    // -------------------------------------------------------------------------
    // NetworkOperator interface
    // -------------------------------------------------------------------------

    /**
     * @brief No per-cycle work.
     *
     * The operator holds only declarations. Dispatch, push constants and the
     * vertex range all belong to the VertexFieldProcessor that consumes
     * build_spec(). Present because NetworkOperator requires it.
     */
    void process(float dt) override;

    /**
     * @brief No settable parameters.
     *
     * dt is owned by process() and rewritten every cycle, so accepting it here
     * would silently discard the value. When mapped parameters reach chain
     * operators, they land as new push constant fields rather than as writes to
     * this one.
     */
    void set_parameter(std::string_view param, double value) override;

    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "GpuFieldOperator";
    }

protected:
    /**
     * @brief Clear the cached spec and bump revision().
     *
     * Protected rather than private so a subclass with its own config
     * surface (e.g. ParticleFieldOperator carrying a SpatialFieldConfig)
     * can signal a change through the same revision a consumer already
     * checks for field-binding changes, rather than needing a second,
     * parallel change-notification mechanism.
     */
    void invalidate();

private:
    /**
     * @struct Binding
     * @brief One target and the field driving it.
     */
    struct Binding {
        FieldTarget targets;
        Kinesis::FieldSource source;
        uint32_t components;
        bool temporal;
        std::optional<uint32_t> cluster;
    };

    Kakshya::VertexLayout m_layout;
    uint32_t m_stride_words {};
    uint32_t m_vertex_binding {};
    uint32_t m_workgroup_size { 256 };
    std::vector<Binding> m_bindings;
    uint64_t m_revision {};

    mutable std::optional<Portal::Graphics::ShaderSpec> m_spec_cache;

    /**
     * @brief Locate a target's word offset and component count in the layout.
     * @return Nullopt when the layout carries no attribute of that modality.
     */
    [[nodiscard]] std::optional<std::pair<uint32_t, uint32_t>>
    resolve_target(FieldTarget target) const;

    /**
     * @brief Shared validation for the three bind overloads.
     */
    bool accept(FieldTarget target, const Kinesis::FieldSource& source,
        uint32_t components);

    /**
     * @brief Store a binding after validation.
     */
    void store(FieldTarget, const Kinesis::FieldSource&, uint32_t, bool,
        std::optional<uint32_t> cluster);
};

} // namespace MayaFlux::Nodes::Network
