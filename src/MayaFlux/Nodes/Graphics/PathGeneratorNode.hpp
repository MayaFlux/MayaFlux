#pragma once

#include "GeometryWriterNode.hpp"
#include "MayaFlux/Kinesis/MotionCurves.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class PathGeneratorNode
 * @brief Generates dense vertex paths from sparse control points or freehand drawing
 *
 * Supports two distinct workflows:
 * 1. Parametric curve editing via control points (full regeneration on changes)
 * 2. Incremental freehand drawing (append-only strokes with smoothing on completion)
 *
 * Both modes use Catmull-Rom or other interpolation methods to generate smooth curves.
 * Produces LINE_STRIP topology output.
 *
 * Philosophy:
 * - Dual-mode design: parametric editing + incremental drawing in one node
 * - Structural Reactivity: Only parametric paths (control points) are live. Changing interpolation
 *   modes, tension, or sampling density will rebuild the parametric curve but will NOT
 *   affect completed freehand strokes.
 * - Control points are sparse input (artist/algorithm provides key positions)
 * - Freehand strokes draw linearly in real-time, smooth on completion
 * - Output vertices are dense (GPU draws smooth curves)
 * - Interpolation happens CPU-side, leveraging Kinesis math primitives
 * - Fixed memory allocation (real-time safe after construction)
 * - Visual Reactivity: Changes to color and thickness are applied globally to both live
 *   control-point paths and "baked" freehand strokes.
 * - Memory Efficiency: Freehand strokes are converted to static vertices upon completion;
 *   raw mouse data is discarded, preventing re-interpolation of baked strokes.
 *
 * Parametric Mode Usage:
 * ```cpp
 * auto path = std::make_shared<PathGeneratorNode>(
 *     Kinesis::InterpolationMode::CATMULL_ROM,
 *     32,   // 32 vertices between each control point
 *     100   // Store up to 100 control points
 * );
 *
 * path->add_control_point({glm::vec3(0.0f, 0.0f, 0.0f)});
 * path->add_control_point({glm::vec3(0.5f, 0.5f, 0.0f)});
 * path->add_control_point({glm::vec3(1.0f, 0.0f, 0.0f)});
 * // Entire curve regenerates through all control points
 *
 * auto buffer = std::make_shared<GeometryBuffer>(path);
 * buffer->setup_rendering({
 *     .target_window = window,
 *     .topology = PrimitiveTopology::LINE_STRIP
 * });
 * ```
 *
 * Freehand Drawing Mode Usage:
 * ```cpp
 * auto path = std::make_shared<PathGeneratorNode>(
 *     Kinesis::InterpolationMode::CATMULL_ROM,
 *     32    // Smoothing resolution
 * );
 *
 * // Real-time drawing (linear segments)
 * window->on_mouse_move([path](double x, double y) {
 *     if (mouse_button_pressed) {
 *         path->draw_to(screen_to_ndc(x, y));  // Appends linear segment
 *     }
 * });
 *
 * // Smooth the stroke when finished
 * window->on_mouse_release([path]() {
 *     path->complete();  // Replaces linear segments with smooth curve
 * });
 *
 * auto buffer = std::make_shared<GeometryBuffer>(path);
 * buffer->setup_rendering({.target_window = window});
 * ```
 *
 * Implementation Details:
 * - Control points stored in fixed-capacity ring buffer (index [0] = newest)
 * - Freehand strokes use sliding 4-point window for Catmull-Rom interpolation
 * - Three vertex collections: control point geometry, completed strokes, in-progress stroke
 * - Parametric edits trigger full regeneration; freehand is append-only
 * - Both modes can coexist: control points and freehand strokes are independent
 */
class MAYAFLUX_API PathGeneratorNode : public GeometryWriterNode {
public:
    using CustomPathFunction = std::function<glm::vec3(std::span<const LineVertex>, double)>;

    /**
     * @brief Create path generator with interpolation mode
     * @param mode Interpolation method
     * @param samples_per_segment Vertices generated between control points
     * @param max_control_points Maximum control points in history
     * @param tension Tension parameter for applicable modes
     */
    explicit PathGeneratorNode(
        Kinesis::InterpolationMode mode = Kinesis::InterpolationMode::QUADRATIC_BEZIER,
        Eigen::Index samples_per_segment = 32,
        size_t max_control_points = 64,
        double tension = 0.5);

    /**
     * @brief Create path generator with custom interpolation function
     * @param custom_func User-provided interpolation function
     * @param samples_per_segment Vertices per segment
     * @param max_control_points Maximum control points in history
     */
    PathGeneratorNode(
        CustomPathFunction custom_func,
        Eigen::Index samples_per_segment = 32,
        size_t max_control_points = 64);

    /**
     * @brief Add control point with full LineVertex data
     * @param vertex LineVertex containing position, color, thickness
     *
     * Pushes vertex to front of ring buffer (index [0]).
     * Oldest vertex discarded if buffer full.
     */
    void add_control_point(const LineVertex& vertex);

    /**
     * @brief Extend path with full LineVertex data
     * @param vertex LineVertex containing target position, color, thickness
     *
     * Generates interpolated vertices between last added point and vertex.position.
     * Appends generated vertices to existing geometry. No history awareness beyond last point.
     */
    void draw_to(const LineVertex& vertex);

    /**
     * @brief Set all control points with full LineVertex data
     * @param vertices Vector of LineVertex data (ordered newest to oldest)
     *
     * Clears existing history and fills buffer with new vertices.
     * If vertices.size() > capacity, only most recent vertices kept.
     */
    void set_control_points(const std::vector<LineVertex>& vertices);

    /**
     * @brief Update specific control point with full LineVertex data
     * @param index Control point index (0 = newest)
     * @param vertex New LineVertex data
     */
    void update_control_point(size_t index, const LineVertex& vertex);

    /**
     * @brief Get control point
     * @param index Control point index (0 = newest)
     * @return Control point position
     */
    [[nodiscard]] LineVertex get_control_point(size_t index) const;

    /**
     * @brief Get all control points as vector
     * @return Vector of control point positions (ordered newest to oldest)
     */
    [[nodiscard]] std::vector<LineVertex> get_control_points() const;

    /**
     * @brief Clear all control points and generated vertices
     */
    void clear_path();

    /**
     * @brief Set path color (applied to all generated vertices)
     * @param color RGB color
     * @param force_uniform If true, ignores per-vertex color and uses this color for all vertices
     */
    void set_path_color(const glm::vec3& color, bool force_uniform = true);

    /**
     * @brief Set uniform color mode
     * @param should_force If true, all vertices will use m_current_color instead of per-vertex color
     */
    void force_uniform_color(bool should_force);

    /**
     * @brief Check if uniform color mode is enabled
     * @return True if uniform color is forced, false if per-vertex color is used
     */
    bool should_force_uniform_color() const { return m_force_uniform_color; }

    /**
     * @brief Set path thickness (applied to all generated vertices)
     * @param thickness Line thickness
     * @param force_uniform If true, ignores per-segment thickness and uses this thickness for all vertices
     */
    void set_path_thickness(float thickness, bool force_uniform = true);

    /**
     * @brief Set uniform thickness mode
     * @param should_force If true, all vertices will use m_current_thickness instead of per-segment thickness
     */
    void force_uniform_thickness(bool should_force);

    /**
     * @brief Get current path color
     * @return RGB color
     */
    [[nodiscard]] const glm::vec3& get_path_color() const { return m_current_color; }

    /**
     * @brief Get current path thickness
     * @return Line thickness
     */
    [[nodiscard]] const float& get_path_thickness() const { return m_current_thickness; }

    /**
     * @brief Set interpolation mode
     * @note Structural change: Only affects control-point paths.
     * Baked freehand strokes remain unchanged.
     */
    void set_interpolation_mode(Kinesis::InterpolationMode mode);

    /**
     * @brief Set samples per segment
     * @note Structural change: Only affects control-point paths.
     */
    void set_samples_per_segment(Eigen::Index samples);

    /**
     * @brief Set tension parameter (for Catmull-Rom)
     * @param tension Tension value
     */
    void set_tension(double tension);

    /**
     * @brief Enable/disable arc-length parameterization
     * @param enable If true, reparameterize by arc length for uniform spacing
     */
    void parameterize_arc_length(bool enable);

    /**
     * @brief Get number of control points currently stored
     * @return Control point count
     */
    [[nodiscard]] size_t get_control_point_count() const { return m_control_points.size(); }

    /**
     * @brief Get maximum control point capacity
     * @return Maximum control points
     */
    [[nodiscard]] size_t get_control_point_capacity() const { return m_control_points.capacity(); }

    /**
     * @brief Get number of generated vertices
     * @return Vertex count
     */
    [[nodiscard]] size_t get_generated_vertex_count() const { return m_vertices.size(); }

    /**
     * @brief Get combined vertex count (control points + completed strokes + in-progress stroke)
     * @return Total vertex count
     */
    [[nodiscard]] size_t get_all_vertex_count() const { return m_combined_cache.size(); }

    /**
     * @brief Get all generated vertices (control points + completed strokes + in-progress stroke)
     * @return Vector of all vertices
     */
    [[nodiscard]] const std::vector<LineVertex>& get_all_vertices() const { return m_combined_cache; }

    /**
     * @brief Compute frame - generates interpolated vertices from control points
     */
    void compute_frame() override;

    /**
     * @brief Finish incremental drawing stroke
     *
     * Clears the sliding window. Call this when pen lifts or stroke ends.
     * Next draw_to() will start a fresh stroke.
     */
    void complete();

    /**
     * @brief Set primitive topology for rendering
     * @param topology Primitive topology (e.g. LINE_LIST, TRIANGLE_LIST)
     *
     * This determines how the vertex data is interpreted when rendered.
     * For example, LINE_LIST treats every pair of vertices as a line segment,
     * while TRIANGLE_LIST treats every triplet of vertices as a triangle.
     */
    void set_primitive_topology(Portal::Graphics::PrimitiveTopology topology) { m_primitive_topology = topology; }

    [[nodiscard]] inline Portal::Graphics::PrimitiveTopology get_primitive_topology() const override
    {
        return m_primitive_topology;
    }

private:
    Kinesis::InterpolationMode m_mode;
    CustomPathFunction m_custom_func;
    Memory::HistoryBuffer<LineVertex> m_control_points;
    std::vector<LineVertex> m_vertices;
    std::vector<LineVertex> m_draw_vertices;
    std::vector<LineVertex> m_completed_draws;

    std::vector<LineVertex> m_combined_cache;

    std::vector<LineVertex> m_draw_window;

    Eigen::Index m_samples_per_segment;
    double m_tension;

    glm::vec3 m_current_color { 1.0F, 1.0F, 1.0F };
    float m_current_thickness { 2.0F };

    bool m_force_uniform_color {};
    bool m_force_uniform_thickness {};
    bool m_geometry_dirty { true };
    bool m_arc_length_parameterization {};
    Portal::Graphics::PrimitiveTopology m_primitive_topology { Portal::Graphics::PrimitiveTopology::LINE_STRIP };

    static constexpr size_t INVALID_SEGMENT { std::numeric_limits<size_t>::max() };
    size_t m_dirty_segment_start { INVALID_SEGMENT };
    size_t m_dirty_segment_end { INVALID_SEGMENT };

    void generate_path_vertices();
    void generate_direct_path();
    void generate_custom_path();
    void generate_interpolated_path();
    void regenerate_geometry();
    void regenerate_segment_range(size_t start_ctrl_idx, size_t end_ctrl_idx);

    void generate_curve_segment(
        const std::vector<LineVertex>& curve_verts,
        size_t start_idx,
        std::vector<LineVertex>& output);

    void append_line_segment(
        const LineVertex& v0,
        const LineVertex& v1,
        std::vector<LineVertex>& output);
};

} // namespace MayaFlux::Nodes::GpuSync
