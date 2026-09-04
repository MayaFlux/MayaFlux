#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class NetworkGeometryBuffer;

/**
 * @struct SpatialHashConfig
 * @brief Uniform grid parameters shared by every stage of the hash build.
 *
 * Cell size is ordinarily the owning ParticleNetwork's interaction radius:
 * a neighbour query at that radius only needs to visit the 27 cells around
 * the query particle's own cell. grid_min/grid_dims describe an axis-aligned
 * box of grid_dims.x * grid_dims.y * grid_dims.z cells covering the
 * network's bounds; a particle outside the box clamps to the nearest edge
 * cell rather than being dropped.
 *
 * particle_count, stride_words and position_word_offset describe the vertex
 * record layout the same way GpuFieldOperator does: word offset of the
 * position attribute within one record, and the record stride in words.
 * Fixed at construction, matching NetworkGeometryBuffer::declare_state's own
 * documented limitation: none of this tracks later growth of the network.
 */
struct SpatialHashConfig {
    glm::vec3 grid_min;
    glm::uvec3 grid_dims;
    float cell_size;
    uint32_t particle_count;
    uint32_t stride_words;
    uint32_t position_word_offset;

    /** @brief Total cell count, grid_dims.x * grid_dims.y * grid_dims.z. */
    [[nodiscard]] uint32_t cell_count() const
    {
        return grid_dims.x * grid_dims.y * grid_dims.z;
    }

    /**
     * @brief Build hash grid parameters from a NetworkGeometryBuffer's
     *        network and its primary operator.
     * @param buffer Buffer whose network supplies particle_count, vertex
     *        layout and bounds. Reads the layout from the network's own
     *        GraphicsOperator (e.g. PhysicsOperator::get_vertex_layout),
     *        not from the buffer's cached copy: that copy is written by
     *        NetworkGeometryProcessor during upload and unset before the
     *        first cycle, whereas the operator's own layout is valid the
     *        moment create_operator<PhysicsOperator>() (or equivalent)
     *        returns. Callable synchronously right after setup, with no
     *        need to wait for a processing cycle.
     * @param cell_size Grid cell size. Ordinarily PhysicsOperator's own
     *        interaction radius when the network is physics-driven; there
     *        is no generic source for this, since not every ParticleNetwork
     *        operator has an equivalent concept, so the caller supplies it
     *        directly.
     * @return Populated config, or nullopt when the buffer's network is not
     *         a ParticleNetwork, its primary operator is not a
     *         GraphicsOperator (or none is set), that operator's layout
     *         carries no word-aligned position attribute, or cell_size is
     *         not positive.
     */
    [[nodiscard]] static std::optional<SpatialHashConfig> from_network(
        const std::shared_ptr<NetworkGeometryBuffer>& buffer, float cell_size);

    /**
     * @brief Declare the four state fields the hash build stages read and
     *        write, sized from this config.
     * @param buffer Buffer to declare on. Fields already present under
     *        these names are left untouched (NetworkGeometryBuffer::declare_state
     *        rejects the duplicate and logs, matching its own documented
     *        behaviour).
     *
     * Declares hash_cell_count/hash_cell_start/hash_cell_cursor at
     * cell_count() elements each, and hash_particle_index at particle_count
     * elements. Call once before attaching HashClearProcessor/
     * HashCountProcessor/HashScanProcessor/HashScatterProcessor to the
     * buffer's chain. All four fields are single-slot: every stage fully
     * overwrites the ones it owns each cycle rather than reading a previous
     * cycle's value, so none of them need a second ping-pong slot.
     */
    void declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const;
};

/**
 * @class NetworkStateFieldProcessor
 * @brief ComputeProcessor operating on named state fields of a
 *        NetworkGeometryBuffer, plus optionally the buffer's own primary
 *        vertex storage.
 *
 * Mirrors VolumeFieldProcessor's relationship to VolumeGridBuffer: every
 * binding lives at a private set 0, descriptors are written directly
 * through ShaderFoundry rather than bind_buffer, and the write happens in
 * processing_function so a resize between cycles (which can reassign the
 * primary vertex buffer's handle) is picked up before execute_shader binds
 * the set into the command buffer.
 *
 * Unlike VolumeFieldProcessor there is no shared lattice to size dispatch
 * from and no per-field double-buffering to track here: every field a
 * concrete stage below declares is single-slot, fully overwritten each
 * cycle by whichever stage owns it. Dispatch sizing and push constant
 * content are each concrete stage's own responsibility.
 */
class MAYAFLUX_API NetworkStateFieldProcessor : public ComputeProcessor {
public:
    /**
     * @struct FieldBinding
     * @brief One shader binding and where it draws from.
     *
     * No read/write distinction: every field this class or its subclasses
     * declare is single-slot (NetworkGeometryBuffer::declare_state with
     * double_buffered=false), and for a single-slot field read_state_handle
     * and write_state_handle always resolve to the same handle. If a future
     * caller genuinely needs a double-buffered hash field, that's the moment
     * to add the distinction back, against a real binding that needs it.
     */
    struct FieldBinding {
        std::string name; ///< Shader binding name, as declared in ShaderConfig.
        uint32_t binding; ///< Binding index within set 0.
        std::string field; ///< State field name, or empty for the buffer's own vertex storage.
    };

    /** @brief The attached buffer, or null if attachment failed validation. */
    [[nodiscard]] const std::shared_ptr<NetworkGeometryBuffer>& get_network_buffer() const { return m_buffer; }

protected:
    /**
     * @brief Construct from a generated ShaderSpec.
     * @param bindings Binding table. Entries are registered into
     *        m_config.bindings in order, all at set 0.
     * @param spec ShaderSpec implementing the stage.
     */
    NetworkStateFieldProcessor(std::vector<FieldBinding> bindings, const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Cache and validate the buffer, then call on_buffer_ready.
     * @param buffer The attached buffer, expected to be a NetworkGeometryBuffer.
     *
     * On any validation failure the cached buffer is reset, which makes
     * on_before_execute reject every subsequent cycle.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /** @brief Write every binding in the table for the current buffer state. */
    void on_descriptors_created() override;

    /**
     * @brief Reject buffers that are not the validated NetworkGeometryBuffer.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Rewrite field descriptors, then run the shader.
     *
     * The primary vertex buffer's handle can change on resize, so bindings
     * are rewritten every cycle rather than once, the same reasoning
     * VolumeFieldProcessor applies to fields that may have swapped.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Hook for subclass setup, called at the end of on_attach.
     *
     * The buffer is non-null and validated when this runs.
     */
    virtual void on_buffer_ready() { }

    /**
     * @brief Configure manual dispatch for one thread per element, and
     *        stage the given push constant data.
     * @tparam T Trivially copyable push constant struct.
     * @param element_count Elements the dispatch must cover. Group count is
     *        ceil(element_count / workgroup_x), floored at 1, reading
     *        workgroup_x from the compiled spec rather than a repeated
     *        literal, matching VertexFieldProcessor::calculate_dispatch_size.
     * @param params Push constant data to stage for the next dispatch.
     *
     * For the common shape most stages share: element_count threads total,
     * the compiled workgroup size per group, one dispatch per cycle.
     * HashScanProcessor's fixed single-invocation dispatch doesn't fit this
     * shape and configures itself directly instead.
     */
    template <typename T>
    void dispatch_one_thread_per(uint32_t element_count, const T& params)
    {
        const uint32_t local_x = std::max(1U, get_dispatch_config().workgroup_x);
        const uint32_t groups = std::max(1U, (element_count + local_x - 1) / local_x);
        set_manual_dispatch(groups, 1, 1);
        set_push_constant_data(params);
    }

private:
    /** @brief Register the binding table into m_config.bindings. */
    void register_bindings();

    /**
     * @brief Check every named state field exists on the attached buffer.
     * @return True if the cached buffer satisfies the binding table.
     */
    bool validate_fields();

    /** @brief Issue descriptor writes for every entry in the binding table. */
    void write_field_descriptors();

    std::vector<FieldBinding> m_bindings;
    std::shared_ptr<NetworkGeometryBuffer> m_buffer;
};

/**
 * @class HashClearProcessor
 * @brief Zeros the per-cell particle histogram before HashCountProcessor
 *        accumulates into it.
 *
 * Only "hash_cell_count" needs an explicit zero: hash_cell_start and
 * hash_cell_cursor are fully overwritten by HashScanProcessor, and
 * hash_particle_index is fully overwritten by HashScatterProcessor. Neither
 * is read-modify-write, so neither carries stale data forward.
 */
class MAYAFLUX_API HashClearProcessor : public NetworkStateFieldProcessor {
public:
    explicit HashClearProcessor(const SpatialHashConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t cell_count_total;
    };

    Params m_params;
};

/**
 * @class HashCountProcessor
 * @brief Buckets each particle into its grid cell and accumulates a
 *        per-cell histogram via atomicAdd.
 *
 * One thread per particle. hash_cell_count must already be zeroed by
 * HashClearProcessor this cycle.
 */
class MAYAFLUX_API HashCountProcessor : public NetworkStateFieldProcessor {
public:
    explicit HashCountProcessor(const SpatialHashConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
    };

    Params m_params;
};

/**
 * @class HashScanProcessor
 * @brief Sequential exclusive prefix sum over the per-cell histogram.
 *
 * Single invocation (workgroup and dispatch both {1,1,1}) looping over
 * every cell. Not parallel, deliberately: a parallel workgroup-shared-memory
 * scan is bounded to one workgroup's element count, which would cap grid
 * resolution; this has no such ceiling. Cell counts in the thousands cost
 * microseconds sequentially, which is cheap relative to the rest of the
 * frame. Revisit only if a real caller measures this as a bottleneck.
 *
 * Writes both hash_cell_start (the offset each cell's particles begin at
 * in hash_particle_index) and hash_cell_cursor (seeded to the same values,
 * the atomic write cursor HashScatterProcessor advances).
 */
class MAYAFLUX_API HashScanProcessor : public NetworkStateFieldProcessor {
public:
    explicit HashScanProcessor(const SpatialHashConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t cell_count_total;
    };

    Params m_params;
};

/**
 * @class HashScatterProcessor
 * @brief Writes each particle's index into hash_particle_index at the slot
 *        its cell's atomic cursor claims.
 *
 * One thread per particle, same cell math as HashCountProcessor. After this
 * stage, hash_particle_index[hash_cell_start[c] .. +hash_cell_count[c]) is
 * the index list of every particle in cell c.
 */
class MAYAFLUX_API HashScatterProcessor : public NetworkStateFieldProcessor {
public:
    explicit HashScatterProcessor(const SpatialHashConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
    };

    Params m_params;
};

/**
 * @class HashDensityColorProcessor
 * @brief Colours each particle by local neighbour count, read from the
 *        completed hash: proof that the hash build is actually usable for
 *        a neighbour query, not just self-consistent.
 *
 * One thread per particle. Walks the query particle's own cell plus its 26
 * neighbours (27 total, clamped at grid edges), and for each candidate in
 * hash_particle_index[hash_cell_start[c] .. +hash_cell_count[c]) checks the
 * real distance rather than trusting cell membership alone, since a cell is
 * a cube and two points in adjacent cells can be closer than two points in
 * the same one. Neighbour count maps to a cool-to-warm colour ramp written
 * into the vertex record's own colour attribute.
 *
 * This is the reason a uniform grid earns its keep over the brute-force
 * O(n^2) PhysicsOperator::apply_spatial_interactions already does on the
 * CPU: at tens of thousands of particles this is O(n * 27 * average
 * occupancy) instead of O(n^2), and it runs as one GPU dispatch instead of
 * a CPU loop. Must run after HashScatterProcessor.
 */
class MAYAFLUX_API HashDensityColorProcessor : public NetworkStateFieldProcessor {
public:
    /**
     * @param config Grid parameters, shared with the build stages that
     *        must already have populated hash_cell_start/hash_cell_count/
     *        hash_particle_index this cycle.
     * @param color_word_offset Word offset of the vertex record's colour
     *        attribute, from VertexLayout::find_word_offset with
     *        DataModality::VERTEX_COLORS_RGB. Construction does not
     *        validate this against the buffer; a layout with no colour
     *        attribute produces a processor that writes past the record
     *        into whatever follows it.
     */
    HashDensityColorProcessor(const SpatialHashConfig& config, uint32_t color_word_offset);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        uint32_t color_offset;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
    };

    Params m_params;
};

} // namespace MayaFlux::Buffers
