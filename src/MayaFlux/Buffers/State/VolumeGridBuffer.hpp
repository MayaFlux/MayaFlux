#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Buffers {

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
     * @brief Construct an unregistered multi-field volume.
     * @param width Cell count along X.
     * @param height Cell count along Y.
     * @param depth Cell count along Z.
     * @param fields Field declarations. Duplicated names are rejected with
     *        an error and the later declaration discarded.
     * @param bounds World-space extent the lattice covers, used to derive
     *        cell size for seeding and for gradient terms in shaders.
     */
    VolumeGridBuffer(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        std::initializer_list<FieldDecl> fields,
        Kinesis::AABB3D bounds);

    /**
     * @brief Construct from a runtime-built field list.
     * @param width Cell count along X.
     * @param height Cell count along Y.
     * @param depth Cell count along Z.
     * @param fields Field declarations.
     * @param bounds World-space extent the lattice covers.
     */
    VolumeGridBuffer(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        std::vector<FieldDecl> fields,
        Kinesis::AABB3D bounds);

    /**
     * @brief Destructor.
     *
     * Raw handles in m_resources.back_buffers are released by the backend
     * during buffer service teardown, as with RelaxationGridBuffer. This
     * class performs no manual Vulkan destruction.
     */
    ~VolumeGridBuffer() override = default;

    /**
     * @brief Establish the processing chain without attaching any stage.
     * @param token Processing domain, typically GRAPHICS_BACKEND.
     *
     * The volume declares no default processor and no stages of its own.
     * Simulation identity is the sequence of processors the caller adds,
     * not a property of this class.
     */
    void setup_processors(ProcessingToken token) override;

    /** @brief Cell count along X. */
    [[nodiscard]] uint32_t get_width() const { return m_width; }

    /** @brief Cell count along Y. */
    [[nodiscard]] uint32_t get_height() const { return m_height; }

    /** @brief Cell count along Z. */
    [[nodiscard]] uint32_t get_depth() const { return m_depth; }

    /** @brief Total cell count, width * height * depth. */
    [[nodiscard]] uint32_t get_cell_count() const { return m_width * m_height * m_depth; }

    /** @brief World-space extent the lattice covers. */
    [[nodiscard]] const Kinesis::AABB3D& get_bounds() const { return m_bounds; }

    /** @brief World-space size of one cell along each axis. */
    [[nodiscard]] glm::vec3 get_cell_size() const;

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

    /** @brief World-space centre of the cell at the given lattice index. */
    [[nodiscard]] glm::vec3 cell_centre(uint32_t x, uint32_t y, uint32_t z) const;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depth;
    Kinesis::AABB3D m_bounds;

    std::vector<std::string> m_field_order;
    std::unordered_map<std::string, Field> m_fields;

    std::shared_ptr<VKBuffer> m_transfer_staging; ///< Reused across seed and read calls.
};

} // namespace MayaFlux::Buffers
