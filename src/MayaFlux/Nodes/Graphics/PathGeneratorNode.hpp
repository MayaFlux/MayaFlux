#pragma once

#include "GeometryWriterNode.hpp"
#include "MayaFlux/Kinesis/MotionCurves.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class PathGeneratorNode
 * @brief Generates dense vertex paths from sparse control points using interpolation
 *
 * Takes control points added over time and generates smooth curves through them
 * using various interpolation methods. Produces LINE_STRIP topology output.
 *
 * Control points stored in fixed-capacity ring buffer for efficient history management.
 * Most recent control point is at index [0], oldest at [capacity-1].
 *
 * Philosophy:
 * - Control points are sparse input (artist/algorithm provides key positions)
 * - Output vertices are dense (GPU draws smooth curves)
 * - Interpolation happens CPU-side, leveraging Kinesis math primitives
 * - Fixed memory allocation (real-time safe after construction)
 *
 * Usage:
 * ```cpp
 * auto path = std::make_shared<PathGeneratorNode>(
 *     PathMode::CATMULL_ROM,
 *     32,   // 32 vertices between each control point
 *     100   // Store up to 100 control points
 * );
 *
 * path->add_control_point(glm::vec3(0.0f, 0.0f, 0.0f));
 * path->add_control_point(glm::vec3(0.5f, 0.5f, 0.0f));
 * path->add_control_point(glm::vec3(1.0f, 0.0f, 0.0f));
 *
 * auto buffer = std::make_shared<GeometryBuffer>(path);
 * buffer->setup_rendering({
 *     .target_window = window,
 *     .topology = PrimitiveTopology::LINE_STRIP
 * });
 * ```
 */
class MAYAFLUX_API PathGeneratorNode : public GeometryWriterNode {
public:
    using CustomPathFunction = std::function<glm::vec3(std::span<const glm::vec3>, double)>;

    /**
     * @brief Create path generator with interpolation mode
     * @param mode Interpolation method
     * @param samples_per_segment Vertices generated between control points
     * @param max_control_points Maximum control points in history
     * @param tension Tension parameter for applicable modes
     */
    explicit PathGeneratorNode(
        Kinesis::InterpolationMode mode = Kinesis::InterpolationMode::CATMULL_ROM,
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
     * @brief Add control point to path history
     * @param position 3D control point position
     *
     * Pushes point to front of ring buffer (becomes index [0]).
     * Oldest point automatically discarded if buffer full.
     */
    void add_control_point(const glm::vec3& position);

    /**
     * @brief Set all control points at once (replaces history)
     * @param points Vector of control point positions (ordered newest to oldest)
     *
     * Clears existing history and fills buffer with new points.
     * If points.size() > capacity, only most recent points kept.
     */
    void set_control_points(const std::vector<glm::vec3>& points);

    /**
     * @brief Update specific control point
     * @param index Control point index (0 = newest)
     * @param position New position
     */
    void update_control_point(size_t index, const glm::vec3& position);

    /**
     * @brief Get control point
     * @param index Control point index (0 = newest)
     * @return Control point position
     */
    [[nodiscard]] glm::vec3 get_control_point(size_t index) const;

    /**
     * @brief Get all control points as vector
     * @return Vector of control point positions (ordered newest to oldest)
     */
    [[nodiscard]] std::vector<glm::vec3> get_control_points() const;

    /**
     * @brief Clear all control points and generated vertices
     */
    void clear_path();

    /**
     * @brief Set path color (applied to all generated vertices)
     * @param color RGB color
     */
    void set_path_color(const glm::vec3& color);

    /**
     * @brief Set path thickness (applied to all generated vertices)
     * @param thickness Line thickness
     */
    void set_path_thickness(float thickness);

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
     * @param mode New interpolation mode
     */
    void set_interpolation_mode(Kinesis::InterpolationMode mode);

    /**
     * @brief Set samples per segment
     * @param samples Vertices between control points
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
    [[nodiscard]] size_t get_control_point_count() const
    {
        return m_control_points.size();
    }

    /**
     * @brief Get maximum control point capacity
     * @return Maximum control points
     */
    [[nodiscard]] size_t get_control_point_capacity() const
    {
        return m_control_points.capacity();
    }

    /**
     * @brief Get number of generated vertices
     * @return Vertex count
     */
    [[nodiscard]] size_t get_generated_vertex_count() const
    {
        return m_vertices.size();
    }

    /**
     * @brief Compute frame - generates interpolated vertices from control points
     */
    void compute_frame() override;

private:
    Kinesis::InterpolationMode m_mode;
    CustomPathFunction m_custom_func;
    Memory::HistoryBuffer<glm::vec3> m_control_points;
    std::vector<LineVertex> m_vertices;

    Eigen::Index m_samples_per_segment;
    double m_tension;

    glm::vec3 m_current_color { 1.0F, 1.0F, 1.0F };
    float m_current_thickness { 2.0F };

    bool m_geometry_dirty { true };
    bool m_arc_length_parameterization { false };

    static constexpr size_t INVALID_SEGMENT = std::numeric_limits<size_t>::max();
    size_t m_dirty_segment_start { INVALID_SEGMENT };
    size_t m_dirty_segment_end { INVALID_SEGMENT };

    void generate_path_vertices();
    void generate_direct_path();
    void generate_custom_path();
    void generate_interpolated_path();
    void regenerate_geometry();
    void regenerate_segment_range(size_t start_ctrl_idx, size_t end_ctrl_idx);
    void emit_vertices_from_positions(std::span<const glm::vec3> positions);
};

} // namespace MayaFlux::Nodes::GpuSync
