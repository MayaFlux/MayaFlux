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
     * @brief Get source point count (before topology expansion)
     */
    [[nodiscard]] virtual size_t get_point_count() const = 0;

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
