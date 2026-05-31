#pragma once

#include "PathGeneratorNode.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class LineSegmentsNode
 * @brief Accumulates discrete unconnected line segments for LINE_LIST rendering.
 *
 * Specialisation of PathGeneratorNode that bypasses all interpolation machinery
 * and emits raw vertex pairs. Every two vertices in the output buffer form one
 * independent segment; there is no continuity between segments.
 *
 * Intended for:
 * - Primitives equivalent to ofDrawLine / ofDrawAxis
 * - Differential geometry annotation (normals, tangents, curvature vectors)
 *   derived from any vertex set produced elsewhere in the graph
 * - Explicit connectors between named positions in a PointCloudNetwork
 *
 * The node slots directly into PathOperator (via PathOperator::add_node) and
 * therefore participates in PointCloudNetwork alongside PathGeneratorNodes and
 * TopologyGeneratorNodes. The buffer/render layer distinguishes topology through
 * get_primitive_topology(), which this class fixes to LINE_LIST.
 *
 * Geometric helpers (add_tangent, add_normal, add_curvature, add_axis) delegate
 * to the corresponding Kinesis::GeometryPrimitives functions and append the
 * resulting pairs into this node's segment list. They do not modify the source
 * vertex set.
 *
 * Color and thickness state set via set_color() / set_thickness() apply to
 * segments added through the positional overloads of add_line() and add_axis().
 * Per-vertex overloads that accept LineVertex directly carry their own attributes.
 *
 * Usage (standalone):
 * @code
 * auto segs = std::make_shared<LineSegmentsNode>();
 * segs->add_line({ .position = { -0.5F, 0.0F, 0.0F }, .color = { 1.0F, 0.5F, 0.0F } },
 *               { .position = {  0.5F, 0.0F, 0.0F }, .color = { 1.0F, 0.5F, 0.0F } });
 * segs->add_normal(path->get_all_vertices(), 0.05F);
 *
 * auto buffer = std::make_shared<GeometryBuffer>(segs);
 * buffer->setup_rendering({ .target_window = window,
 *                           .topology = PrimitiveTopology::LINE_LIST });
 * @endcode
 *
 * Usage (inside PathOperator / PointCloudNetwork):
 * @code
 * auto segs = std::make_shared<LineSegmentsNode>();
 * segs->add_line(cloud_point_a, cloud_point_b);
 *
 * path_op->add_node(segs);
 * network_buffer->add_chain_operator_rendering(
 *     { .topology = PrimitiveTopology::LINE_LIST, .target_window = window });
 * @endcode
 */
class MAYAFLUX_API LineSegmentsNode : public PathGeneratorNode {
public:
    /**
     * @brief Construct an empty segment accumulator.
     * @param initial_capacity Initial vertex capacity (in segment endpoints, always even).
     */
    explicit LineSegmentsNode(size_t initial_capacity = 256);

    // -------------------------------------------------------------------------
    // Core segment API
    // -------------------------------------------------------------------------

    /**
     * @brief Add a segment between two vertices.
     * @param a Start vertex (position, color, thickness).
     * @param b End vertex (position, color, thickness).
     */
    void add_line(const LineVertex& a, const LineVertex& b);

    /**
     * @brief Add a directed axis segment originating from a point.
     *
     * Emits one segment from @p origin along @p direction scaled to @p length,
     * using current color and thickness.
     *
     * @param origin Base position.
     * @param direction Unit or non-unit direction vector (not normalised internally).
     * @param length Scalar length of the emitted segment.
     */
    void add_axis(const LineVertex& origin, const glm::vec3& direction, float length);

    /**
     * @brief Remove all accumulated segments and reset the vertex buffer.
     */
    void clear_segments();

    // -------------------------------------------------------------------------
    // Differential geometry annotation
    // -------------------------------------------------------------------------

    /**
     * @brief Append normal segments derived from a vertex path.
     *
     * Delegates to Kinesis::compute_path_normals. Each normal is emitted as
     * one LINE_LIST segment centred on the midpoint between consecutive vertices.
     * Appends to existing segments; does not clear first.
     *
     * @param path_vertices Source vertices defining the path.
     * @param length        Visual length of each normal segment.
     * @param stride        Sample every stride-th vertex (default: 1).
     */
    void add_normal(
        const std::vector<LineVertex>& path_vertices,
        float length,
        size_t stride = 1);

    /**
     * @brief Append tangent segments derived from a vertex path.
     *
     * Delegates to Kinesis::compute_path_tangents. Each tangent is emitted as
     * one LINE_LIST segment centred on the source vertex.
     * Appends to existing segments; does not clear first.
     *
     * @param path_vertices Source vertices defining the path.
     * @param length        Visual length of each tangent segment.
     * @param stride        Sample every stride-th vertex (default: 1).
     */
    void add_tangent(
        const std::vector<LineVertex>& path_vertices,
        float length,
        size_t stride = 1);

    /**
     * @brief Append curvature segments derived from a vertex path.
     *
     * Delegates to Kinesis::compute_path_curvature. Each curvature vector is
     * emitted as one LINE_LIST segment originating at the source vertex.
     * Appends to existing segments; does not clear first.
     *
     * @param path_vertices Source vertices defining the path.
     * @param scale         Magnitude scaling factor applied to the curvature vector.
     * @param stride        Sample every stride-th vertex (default: 1).
     */
    void add_curvature(
        const std::vector<LineVertex>& path_vertices,
        float scale,
        size_t stride = 1);

    /**
     * @brief Number of segments currently accumulated.
     *
     * Each segment is two vertices; this returns vertex_count / 2.
     */
    [[nodiscard]] size_t get_segment_count() const { return m_segments.size() / 2; }

    // -------------------------------------------------------------------------
    // GeometryWriterNode / PathGeneratorNode overrides
    // -------------------------------------------------------------------------

    /**
     * @brief Pack accumulated segments into the GPU vertex buffer.
     *
     * Bypasses all PathGeneratorNode interpolation. Copies m_segments
     * directly via set_vertices<LineVertex>.
     */
    void compute_frame() override;

    /**
     * @brief Returns LINE_LIST; fixed for this node type.
     */
    [[nodiscard]] Portal::Graphics::PrimitiveTopology get_primitive_topology() const override
    {
        return Portal::Graphics::PrimitiveTopology::LINE_LIST;
    }

private:
    std::vector<LineVertex> m_segments;

    void append_pairs(const std::vector<LineVertex>& pairs);
};

} // namespace MayaFlux::Nodes::GpuSync
