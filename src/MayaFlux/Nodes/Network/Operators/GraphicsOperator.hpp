#pragma once

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "NetworkOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class GraphicsOperator
 * @brief Operator that produces GPU-renderable geometry
 *
 * Adds graphics-specific interface (vertex data, position extraction)
 * on top of base NetworkOperator. Uses glm::vec3 for positions since
 * that's the graphics domain standard.
 */
class MAYAFLUX_API GraphicsOperator : public NetworkOperator {
public:
    /**
     * @brief Get vertex data for GPU upload
     */
    [[nodiscard]] virtual std::span<const uint8_t> get_vertex_data() const = 0;

    /**
     * @brief Get vertex data for specific collection (if multiple)
     * @param idx Collection index
     */
    [[nodiscard]] virtual std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx = 0) const = 0;

    /**
     * @brief Get vertex layout describing vertex structure
     */
    [[nodiscard]] virtual Kakshya::VertexLayout get_vertex_layout() const = 0;

    /**
     * @brief Get number of vertices (may differ from point count for topology/path)
     */
    [[nodiscard]] virtual size_t get_vertex_count() const = 0;

    /**
     * @brief Check if geometry changed this frame
     */
    [[nodiscard]] virtual bool is_vertex_data_dirty() const = 0;

    /**
     * @brief Clear dirty flag after GPU upload
     */
    virtual void mark_vertex_data_clean() = 0;

    /**
     * @struct DirtyVertexRange
     * @brief A contiguous span of vertex records that changed since the last
     *        mark_vertex_data_clean().
     *
     * group_index selects which vertex-array pack the span's bytes come from,
     * for a consumer that reads one pack at a time through
     * get_vertex_data_for_collection(); it is 0 for an operator holding a
     * single pack. vertex_offset and vertex_count are in global vertex-index
     * order, the same concatenation order get_vertex_data() and
     * build_cluster_ids() produce.
     */
    struct DirtyVertexRange {
        uint32_t group_index {};
        uint32_t vertex_offset {};
        uint32_t vertex_count {};
    };

    /**
     * @brief Vertex sub-ranges that changed since the last
     *        mark_vertex_data_clean().
     * @return One entry per contiguous dirty region, in global vertex-index
     *         order, or empty when nothing changed.
     *
     * The default reports a single range spanning every vertex whenever
     * is_vertex_data_dirty() is true, so a consumer that uploads per-range
     * stays identical to one that re-uploads the whole buffer. An operator
     * holding several independent vertex-array packs (PathOperator's paths,
     * TopologyOperator's graphs) overrides this to report only the packs
     * whose own geometry changed this cycle, so an edit to one pack leaves
     * the uploaded bytes of the others, and any GPU-side accumulation already
     * written into them, untouched.
     */
    [[nodiscard]] virtual std::vector<DirtyVertexRange> dirty_vertex_ranges() const
    {
        if (!is_vertex_data_dirty()) {
            return {};
        }
        return { DirtyVertexRange { .vertex_count = static_cast<uint32_t>(get_vertex_count()) } };
    }

    /**
     * @brief Whether every vertex mutation on this operator sets a group's
     *        dirty flag, so dirty_vertex_ranges() is the complete record of
     *        what changed since the last mark_vertex_data_clean().
     *
     * False by default: a consumer must re-upload the whole buffer. True for
     * operators (PathOperator, TopologyOperator) whose per-group flags are
     * authoritative, letting a consumer upload only the reported ranges and
     * then call mark_vertex_data_clean(). An operator that reports true must
     * also route every path through which its vertices change (add, edit,
     * clear, interpolation-parameter changes) to a group dirty flag.
     */
    [[nodiscard]] virtual bool supports_incremental_upload() const { return false; }

    /**
     * @brief Get source point count (before topology expansion)
     */
    [[nodiscard]] virtual size_t get_point_count() const = 0;

    /**
     * @brief Per-vertex collection index, global index order.
     * @return get_vertex_count() elements, not get_point_count(): the
     *         consumer indexes it in lockstep with the vertex buffer a
     *         GpuFieldOperator dispatch or the spatial hash actually reads
     *         (vertices[i], not points[i]), and for an operator whose
     *         rendered vertex count differs from its source point count
     *         (TopologyOperator/PathOperator after interpolation) those are
     *         two different numbers. Default: every entry 0, meaning "one
     *         population, no cluster distinction" -- correct for every
     *         operator that carries no notion of multiple complete
     *         vertex-array packs, which is every GraphicsOperator except
     *         PhysicsOperator today. get_point_count() and get_vertex_count()
     *         coincide for PhysicsOperator's point-sprite geometry, so this
     *         default is exactly as correct there as a point_count-sized
     *         array would have been.
     *
     * The source NetworkGeometryBuffer::ensure_cluster_ids() reads from when
     * declaring and populating the hash_cluster_id state field a
     * cluster-aware GPU stage (ClaimProcessor, HashDensityColorProcessor, a
     * cluster-scoped GpuFieldOperator binding) consumes. An operator that
     * does carry multiple collections overrides this once, here, and every
     * such consumer picks it up with no further wiring: none of them
     * dynamic_cast to a concrete operator type to get it. An override must
     * size and order its result the same way: by rendered vertex, in the
     * same concatenation order get_vertex_data() itself produces.
     */
    [[nodiscard]] virtual std::vector<uint32_t> build_cluster_ids() const
    {
        return std::vector<uint32_t>(get_vertex_count(), 0U);
    }

    /**
     * @brief Apply ONE_TO_ONE parameter mapping
     *
     * Default implementation handles common graphics properties:
     * - "color": Per-point color
     * - "size": Per-point size (for point rendering)
     */
    void apply_one_to_one(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source) override;

    /**
     * @brief Get human-readable vertex type name (for validation/debugging)
     */
    [[nodiscard]] virtual const char* get_vertex_type_name() const = 0;

    /**
     * @brief Whether this operator contributes a vertex slice to rendering.
     *
     * Default true. Set false for transform-only chain operators (e.g. a
     * FieldOperator deforming upstream vertices) that must not add an
     * independent render slice.
     */
    [[nodiscard]] bool participates_in_rendering() const { return m_participates_in_rendering; }
    void set_participates_in_rendering(bool value) { m_participates_in_rendering = value; }

    /**
     * @brief Whether this operator requests upstream vertex state before process().
     *
     * Default false. Set true for operators that derive their initial vertex
     * data from the preceding operator in the chain rather than from an
     * explicit initialize() call.
     */
    [[nodiscard]] bool consumes_upstream() const { return m_consumes_upstream; }
    void set_consumes_upstream(bool value) { m_consumes_upstream = value; }

    /**
     * @brief Receive upstream vertex state before process() is called.
     *
     * Called by OperatorChain::process() only when consumes_upstream() is true.
     * Implementations seed their internal vertex buffer from the upstream
     * operator's current output. Default no-op.
     *
     * @param upstream Last GraphicsOperator that ran before this one in the
     *                 chain, or the primary operator if this is the first chain
     *                 entry. Null if no upstream GraphicsOperator exists.
     */
    virtual void seed_from_upstream(const GraphicsOperator* upstream) { }

    /**
     * @brief Override the dt passed by the caller with a fixed internal value.
     *
     * Useful when the owning network passes 0.0F or a sample-count dt that is
     * meaningless for time-based integration (e.g. PhysicsOperator in a
     * PointCloudNetwork chain). When true, process() ignores the incoming dt
     * and substitutes m_internal_dt instead.
     */
    void set_force_internal_dt(bool value) { m_force_internal_dt = value; }
    [[nodiscard]] bool uses_force_internal_dt() const { return m_force_internal_dt; }

protected:
    /**
     * @brief Get mutable access to point at global index
     * @return Pointer to vertex data, or nullptr if index invalid
     *
     * Subclasses must implement to provide per-point access
     */
    virtual void* get_data_at(size_t global_index) = 0;

    bool m_participates_in_rendering { true };
    bool m_consumes_upstream {};
    bool m_force_internal_dt {};
};

} // namespace MayaFlux::Nodes::Network::Operators
