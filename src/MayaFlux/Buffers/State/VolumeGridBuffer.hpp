#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

namespace MayaFlux::Buffers {

class VolumeSurfaceProcessor;
class SDFMeshProcessor;
class RenderProcessor;
class VolumeGridBuffer;
class AdvectProcessor;
class BuoyancyProcessor;
class WallProcessor;
class DivergenceProcessor;
class DiffuseProcessor;
class PressureProcessor;
class SolenoidalProcessor;

/**
 * @struct ScalarRef
 * @brief A scalar field's name, issued by the volume that owns it.
 *
 * Exists so a field is spelled once, at declaration, rather than at every
 * site that addresses it. A name typed twice is two independent literals
 * that must agree; a mismatch surfaces as a stage that runs and does
 * nothing, which is the hardest failure in this subsystem to diagnose.
 *
 * Converts implicitly to the field name, so a ref passes anywhere a
 * processor expects a string. The type distinction bites where a
 * signature demands one kind: a parameter taking ScalarRef cannot be
 * handed a vector field.
 *
 * Carries a weak reference to its issuer so a ref from one volume used
 * against another can be rejected rather than silently resolving to a
 * same-named field.
 */
struct ScalarRef {
    std::string name;
    std::weak_ptr<VolumeGridBuffer> owner;

    static constexpr size_t stride = sizeof(float);

    operator const std::string&() const { return name; }

    /**
     * @brief Whether this ref was issued by the given volume.
     * @param volume Volume to test against.
     */
    [[nodiscard]] bool issued_by(const VolumeGridBuffer* volume) const
    {
        auto o = owner.lock();
        return o && o.get() == volume;
    }
};

/**
 * @struct VectorRef
 * @brief A vector field's name, issued by the volume that owns it.
 *
 * As ScalarRef, for fields of glm::vec4 stride. The fourth component is
 * carried through by every current stage and read by none.
 */
struct VectorRef {
    std::string name;
    std::weak_ptr<VolumeGridBuffer> owner;

    static constexpr size_t stride = sizeof(glm::vec4);

    operator const std::string&() const { return name; }

    /**
     * @brief Whether this ref was issued by the given volume.
     * @param volume Volume to test against.
     */
    [[nodiscard]] bool issued_by(const VolumeGridBuffer* volume) const
    {
        auto o = owner.lock();
        return o && o.get() == volume;
    }
};

/**
 * @class VolumeGridBuffer
 * @brief GPU-resident state for multi-field simulations evaluated over a
 *        fixed 3D topology: incompressible fluid, gas and smoke, reaction
 *        systems, and any process where several quantities co-evolve over
 *        the same cell lattice.
 *
 * Where RelaxationGridBuffer carries one field over a 2D topology and
 * advances it with one rule dispatch per cycle, this carries N named
 * fields over a 3D topology and advances them with a chain of processors,
 * each responsible for one stage. A velocity field, a pressure field, a
 * divergence scratch field and one or more carried scalars are the
 * ordinary case; nothing in the class knows what any of them mean.
 *
 * All field storage lives as raw Vulkan handle pairs inside
 * VKBufferResources::back_buffers, allocated once at construction as
 * device-local with transfer source and destination usage. Fields are
 * addressed by name; each declares its own component stride and whether
 * it needs a second slot for ping-pong. Single-slot fields resolve read
 * and write to the same handle, which suits scratch quantities
 * recomputed from nothing each stage.
 *
 * No VKBuffer object represents any field. Processors write descriptors
 * for these handles directly against ShaderFoundry, as
 * RelaxationStepProcessor does, resolving them through read_handle() and
 * write_handle().
 *
 * Chain layout:
 *   default   - VolumeSurfaceProcessor, resampling the surfaced field
 *   flat[0]   - SDFMeshProcessor, extracting the isosurface
 *   flat[1..] - simulation stages, added by the caller at any point
 *   final     - RenderProcessor
 *
 * Extraction occupies the default and first flat slots so that
 * simulation stages appended by the caller always follow it. The surface
 * therefore reflects the previous cycle's field rather than the current
 * one, which at frame rate is not observable.
 *
 * The VKBuffer base owns no vertex output. Extraction is a separate
 * concern: attach SDFMeshProcessor against a scalar field for an
 * isosurface, or a bespoke processor for raymarching or debug points.
 * The buffer emits nothing on its own.
 *
 * Usage:
 * @code
 * auto vol = std::make_shared<VolumeGridBuffer>(
 *     64, 64, 64,
 *     { { "velocity", sizeof(glm::vec4) },
 *       { "pressure", sizeof(float) },
 *       { "divergence", sizeof(float), false },
 *       { "density", sizeof(float) } },
 *     Kinesis::AABB3D { { -1, -1, -1 }, { 1, 1, 1 } });
 *
 * vol->seed("density", Kinesis::SpatialField { ... });
 * vol->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * @endcode
 */
class MAYAFLUX_API VolumeGridBuffer : public VKBuffer {
public:
    /**
     * @struct FieldDecl
     * @brief One field's declaration within a volume.
     */
    struct FieldDecl {
        std::string name; ///< Lookup key, unique within the volume.
        size_t stride_bytes; ///< Bytes per cell.
        bool double_buffered = true; ///< False resolves read and write to one slot.
    };

    /**
     * @struct SurfaceConfig
     * @brief Isosurface extraction parameters for a scalar field.
     *
     * Supplied at construction because the extraction resolution
     * determines this buffer's own vertex storage size: mc_emit allocates
     * slots by atomicAdd without a capacity check, so storage is sized to
     * the worst case of fifteen vertices per voxel. At 48 cubed that is
     * roughly 95 MB, at 64 cubed roughly 225 MB.
     */
    struct SurfaceConfig {
        std::string field_name; ///< Scalar field surfaced. Stride must be sizeof(float).
        glm::uvec3 resolution; ///< Extraction cell count per axis, over the volume's own bounds.
        float threshold; ///< Field value the surface is placed at.
    };

    /**
     * @struct FlowConfig
     * @brief Parameters for the incompressible flow stage arrangement.
     *
     * Carries what varies between simulations. What does not vary, the
     * stage ordering, is fixed by setup_flow: buoyancy must precede the
     * projection or the solve removes the divergence it injects, the wall
     * condition must follow every stage that writes velocity, and carried
     * scalars must be advected by the projected velocity rather than the
     * raw one. Those are correctness constraints, not preferences, and
     * they are the part callers get wrong.
     */
    struct FlowConfig {
        /**
         * @struct Carried
         * @brief A scalar transported by the velocity field.
         */
        struct Carried {
            ScalarRef field; ///< Field advected.
            float dissipation { 1.0F }; ///< Per-cycle multiplier. One conserves.
        };

        /**
         * @struct Buoyancy
         * @brief Body force accumulated into velocity from two scalars.
         */
        struct Buoyancy {
            ScalarRef temperature; ///< Drives motion along direction.
            ScalarRef density; ///< Drives motion against it.
            glm::vec3 direction { 0.0F, 1.0F, 0.0F }; ///< Axis and magnitude.
            float temperature_gain { 1.0F };
            float density_gain { 0.0F };
            float ambient { 0.0F }; ///< Temperature at which the rise term vanishes.
        };

        VectorRef velocity; ///< Required.
        ScalarRef divergence; ///< Required. Single-slot is correct.
        ScalarRef pressure; ///< Required.
        VectorRef scratch; ///< Required only when viscosity is above zero.

        float time_step { 1.0F / 60.0F }; ///< Applied to every stage that integrates.
        float viscosity { 0.0F }; ///< Zero omits the diffusion stage entirely.
        uint32_t jacobi_iterations { 32 };
        bool walls { true }; ///< Free-slip on the six faces, twice per cycle.

        std::vector<Carried> carried;
        std::optional<Buoyancy> buoyancy;
    };

    /**
     * @struct FlowStages
     * @brief Every stage setup_flow built, in no particular order.
     *
     * Returned rather than stored so each remains reachable for retuning,
     * feeding, or inspection. Members are null where the config omitted
     * the corresponding stage.
     */
    struct FlowStages {
        std::shared_ptr<AdvectProcessor> self_advect;
        std::shared_ptr<BuoyancyProcessor> buoyancy;
        std::shared_ptr<DiffuseProcessor> diffuse;
        std::shared_ptr<WallProcessor> wall_advected;
        std::shared_ptr<DivergenceProcessor> divergence;
        std::shared_ptr<PressureProcessor> pressure;
        std::shared_ptr<SolenoidalProcessor> solenoidal;
        std::shared_ptr<WallProcessor> wall_projected;
        std::vector<std::shared_ptr<AdvectProcessor>> carriers; ///< Parallel to config.carried.
    };

    /**
     * @brief Construct an unregistered multi-field volume.
     * @param lattice Discretized extent every field is stored over.
     * @param fields Field declarations. Duplicated names are rejected with
     *        an error and the later declaration discarded.
     * @param surface Optional isosurface extraction parameters.
     */
    VolumeGridBuffer(
        Kinesis::Lattice3D lattice,
        std::initializer_list<FieldDecl> fields,
        std::optional<SurfaceConfig> surface = std::nullopt);

    /**
     * @brief Construct from a runtime-built field list.
     * @param lattice Discretized extent every field is stored over.
     * @param fields Field declarations.
     * @param surface Optional isosurface extraction parameters.
     */
    VolumeGridBuffer(
        Kinesis::Lattice3D lattice,
        std::vector<FieldDecl> fields,
        std::optional<SurfaceConfig> surface = std::nullopt);

    /**
     * @brief Construct a volume with no fields, to be declared after.
     * @param lattice Discretized extent every field is stored over.
     * @param surface Optional isosurface extraction parameters. Its field
     *        name resolves at setup_processors, so it may name a field
     *        declared after construction.
     *
     * Extraction storage is sized here because it depends only on the
     * extraction resolution, so the SurfaceConfig cannot move later.
     */
    explicit VolumeGridBuffer(
        Kinesis::Lattice3D lattice,
        std::optional<SurfaceConfig> surface = std::nullopt);

    /**
     * @brief Destructor.
     *
     * Raw handles in m_resources.back_buffers are released by the backend
     * during buffer service teardown, as with RelaxationGridBuffer. This
     * class performs no manual Vulkan destruction.
     */
    ~VolumeGridBuffer() override;

    /**
     * @brief Establish the processing chain without attaching any stage.
     * @param token Processing domain, typically GRAPHICS_BACKEND.
     *
     * The volume declares no default processor and no stages of its own.
     * Simulation identity is the sequence of processors the caller adds,
     * not a property of this class.
     *
     * When a SurfaceConfig was supplied at construction, two stages are
     * appended: the field-to-corner-grid resample and the marching cubes
     * extraction writing into this buffer's own vertex storage. Simulation
     * stages added before this call keep their position ahead of them.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor drawing the extracted surface.
     * @param config Render target. Vertex and fragment shaders default to
     *        the untextured triangle pair; topology is forced to
     *        TRIANGLE_LIST.
     *
     * Requires a SurfaceConfig at construction. Without one this buffer
     * has no vertex storage and nothing to draw.
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Build and append the incompressible flow stages.
     * @param config Fields and parameters. Every ref must have been issued
     *        by this volume.
     * @return The stages built, all already in the chain.
     *
     * Appends in the order: self-advection, buoyancy, diffusion, wall,
     * divergence, pressure, solenoidal, wall, then one advection per
     * carried scalar. Carriers run last so they are transported by the
     * projected velocity.
     *
     * Stages added to the chain before this call keep their position
     * ahead of it, which is where an influx stage belongs.
     */
    FlowStages setup_flow(const FlowConfig& config);

    /** @brief The surface extraction stage, valid after setup_rendering. */
    [[nodiscard]] std::shared_ptr<VolumeSurfaceProcessor> surface_processor() const { return m_surface_processor; }

    /** @brief The marching cubes stage, valid after setup_rendering. */
    [[nodiscard]] std::shared_ptr<SDFMeshProcessor> mesh_processor() const { return m_mesh_processor; }

    /** @brief The lattice every field is discretized over. */
    [[nodiscard]] const Kinesis::Lattice3D& get_lattice() const { return m_lattice; }

    /** @brief Cell count along X. */
    [[nodiscard]] uint32_t get_width() const { return m_lattice.resolution.x; }

    /** @brief Cell count along Y. */
    [[nodiscard]] uint32_t get_height() const { return m_lattice.resolution.y; }

    /** @brief Cell count along Z. */
    [[nodiscard]] uint32_t get_depth() const { return m_lattice.resolution.z; }

    /** @brief Total cell count. */
    [[nodiscard]] uint32_t get_cell_count() const { return static_cast<uint32_t>(m_lattice.cell_count()); }

    /** @brief World-space extent the lattice covers. */
    [[nodiscard]] const Kinesis::AABB3D& get_bounds() const { return m_lattice.bounds; }

    /** @brief World-space size of one cell along each axis. */
    [[nodiscard]] glm::vec3 get_cell_size() const { return m_lattice.cell_size(); }

    /** @brief Whether a field of this name was declared. */
    [[nodiscard]] bool has_field(const std::string& name) const;

    /** @brief Byte size of one slot of the named field, or 0 if undeclared. */
    [[nodiscard]] size_t get_field_bytes(const std::string& name) const;

    /** @brief Names of every declared field, in declaration order. */
    [[nodiscard]] std::vector<std::string> get_field_names() const;

    /**
     * @brief Handle a stage should read the named field from.
     * @param name Field name.
     * @return Vulkan buffer handle, or nullptr if undeclared.
     */
    [[nodiscard]] vk::Buffer read_handle(const std::string& name) const;

    /**
     * @brief Handle a stage should write the named field to.
     * @param name Field name.
     * @return Vulkan buffer handle, or nullptr if undeclared.
     *
     * Equals read_handle() for single-slot fields.
     */
    [[nodiscard]] vk::Buffer write_handle(const std::string& name) const;

    /**
     * @brief Exchange read and write slots for the named field.
     * @param name Field name. No effect on single-slot fields.
     *
     * Called by whichever stage last wrote the field, after its dispatch,
     * so the next stage reading it observes the new values. A stage that
     * writes a field it also reads must swap; a stage writing a scratch
     * field consumed immediately after need not.
     */
    void swap_field(const std::string& name);

    /**
     * @brief Declare a double-buffered scalar field.
     * @param name Lookup key, unique within this volume.
     * @return Ref naming the field, or a ref with an empty name if the
     *         declaration was rejected.
     */
    ScalarRef declare_scalar(std::string name);

    /**
     * @brief Declare a single-slot scalar field.
     * @param name Lookup key, unique within this volume.
     * @return Ref naming the field, or a ref with an empty name if the
     *         declaration was rejected.
     *
     * Read and write resolve to one handle, which suits a quantity
     * recomputed from other fields every cycle and never carried across
     * cycles: divergence is the ordinary case. A stage that reads a
     * neighbourhood of a field it also writes cannot use one of these,
     * and VolumeFieldProcessor rejects that arrangement at attach.
     */
    ScalarRef declare_scratch(std::string name);

    /**
     * @brief Declare a double-buffered vector field.
     * @param name Lookup key, unique within this volume.
     * @return Ref naming the field, or a ref with an empty name if the
     *         declaration was rejected.
     *
     * Vector fields are always double-buffered. The single-slot case
     * saves one slot, a megabyte at 64 cubed, and every vector field in
     * use is either advected or diffused, both of which require two.
     */
    VectorRef declare_vector(std::string name);

    /**
     * @brief Write initial values into a scalar field from a Kinesis field.
     * @param name Field name. Must have stride sizeof(float).
     * @param field Sampled at each cell centre in world space.
     */
    void seed(const std::string& name, const Kinesis::SpatialField& field);

    /**
     * @brief Write initial values into a vector field from a Kinesis field.
     * @param name Field name. Must have stride sizeof(glm::vec4).
     * @param field Sampled at each cell centre in world space. The fourth
     *        component of every cell is zeroed.
     */
    void seed(const std::string& name, const Kinesis::VectorField& field);

    /**
     * @brief Write initial values into a field from raw host memory.
     * @param name Field name.
     * @param data Pointer to at least @p size bytes.
     * @param size Byte count; must equal get_field_bytes(name).
     *
     * Writes into the current read slot. For double-buffered fields the
     * write slot is left untouched, which is correct when the first stage
     * to touch the field reads before it writes.
     */
    void seed_raw(const std::string& name, const void* data, size_t size);

    /**
     * @brief Add sampled values into a scalar field.
     * @param name Field name. Must have stride sizeof(float).
     * @param field Sampled at each cell centre in world space and added
     *        to whatever the field already holds.
     *
     * Blocking. Reads the current read slot to host memory, adds, and
     * writes back, so it costs a full round trip at the lattice's byte
     * size. Intended for authored injection on a coarse clock, not as a
     * chain stage.
     */
    void accumulate(const std::string& name, const Kinesis::SpatialField& field);

    /**
     * @brief Add sampled values into a vector field.
     * @param name Field name. Must have stride sizeof(glm::vec4).
     * @param field Sampled at each cell centre in world space and added
     *        to the first three components. The fourth is left as found.
     */
    void accumulate(const std::string& name, const Kinesis::VectorField& field);

    /**
     * @brief Copy the current read slot of a field to host memory.
     * @param name Field name.
     * @param data Destination pointer, at least get_field_bytes(name) bytes.
     * @param size Byte count; must equal get_field_bytes(name).
     *
     * Blocking. Records a fenced device-to-host copy and waits on it from
     * the calling thread. Not for per-frame use on the graphics thread.
     */
    void read_field(const std::string& name, void* data, size_t size);

private:
    struct Field {
        size_t stride_bytes;
        uint32_t slot_a;
        uint32_t slot_b;
        bool read_is_a;
    };

    /**
     * @brief Populate m_fields from declarations.
     * @param decls Field declarations in declaration order.
     */
    void allocate_fields(const std::vector<FieldDecl>& decls);

    /**
     * @brief Allocate one field's slots and register it.
     * @param name Lookup key.
     * @param stride_bytes Bytes per cell.
     * @param double_buffered Whether to allocate a second slot.
     * @return True if the field was registered.
     */
    bool allocate_field(const std::string& name, size_t stride_bytes, bool double_buffered);

    /**
     * @brief Resolve a field by name.
     * @param name Field name.
     * @param context Caller identifier used in the error path.
     * @return Pointer to the field, or nullptr with an error logged.
     */
    [[nodiscard]] const Field* find_field(const std::string& name, const char* context) const;

    Kinesis::Lattice3D m_lattice;

    std::vector<std::string> m_field_order;
    std::unordered_map<std::string, Field> m_fields;
    std::optional<SurfaceConfig> m_surface;

    std::shared_ptr<VKBuffer> m_transfer_staging; ///< Reused across seed and read calls.
    std::shared_ptr<VolumeSurfaceProcessor> m_surface_processor;
    std::shared_ptr<SDFMeshProcessor> m_mesh_processor;
    std::shared_ptr<VKBuffer> m_counter_buf; ///< Atomic vertex counter for the extraction stage.

    TransferHandle m_pending_transfer; ///< In-flight seed upload, resolved before the next transfer.

    /**
     * @brief Bytes of vertex storage the extraction resolution requires.
     * @param surface Extraction parameters, or nullopt for no extraction.
     * @return Worst-case vertex bytes, or 1 when no surface is configured.
     */
    static size_t surface_storage_bytes(const std::optional<SurfaceConfig>& surface);
};

} // namespace MayaFlux::Buffers
