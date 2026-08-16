#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

namespace MayaFlux::Buffers {

class VolumeSurfaceProcessor;
class SDFMeshProcessor;
class RenderProcessor;

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
     * @brief Populate m_fields from declarations and allocate one
     *        device-local raw slot per field slot.
     * @param decls Field declarations in declaration order.
     */
    void allocate_fields(const std::vector<FieldDecl>& decls);

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
