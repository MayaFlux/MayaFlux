#pragma once

#include "GeometryWriterNode.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include "MayaFlux/Kinesis/MotionCurves.hpp"
#include "MayaFlux/Kinesis/ProximityGraphs.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class TopologyGeneratorNode
 * @brief Generates dynamic mesh topology from sparse control points
 *
 * Core concept: Points define locations, ConnectionMode defines relationships.
 * Every time points are added/removed/moved, topology is regenerated.
 *
 * Philosophy:
 * - Points are spatial anchors
 * - Connections emerge from geometric relationships
 * - Topology IS the content, not decoration
 *
 * Extensions beyond connections:
 * - Attractor mode: Points influence field, no lines
 * - Gradient mode: Points define texture sampling positions
 * - Emitter mode: Points spawn particles
 *
 * Usage:
 * ```cpp
 * auto topo = std::make_shared<TopologyGenerator>(
 *     Kinesis::ProximityMode::K_NEAREST,
 *     true  // auto_connect
 * );
 *
 * topo->add_point(glm::vec3(0.0f, 0.0f, 0.0f));
 * topo->add_point(glm::vec3(1.0f, 0.0f, 0.0f));
 * topo->add_point(glm::vec3(0.5f, 1.0f, 0.0f));
 * // Delaunay triangulation automatically computed
 *
 * auto buffer = std::make_shared<GeometryBuffer>(topo);
 * buffer->setup_rendering({
 *     .target_window = window,
 *     .topology = PrimitiveTopology::LINE_LIST  // or TRIANGLE_LIST
 * });
 * ```
 * WARNING: Performance Characteristics
 *
 * Connection algorithms vary significantly in computational complexity.
 * Topology is fully regenerated when points change.
 *
 * Complexity Overview:
 * --------------------
 * - Sequential Chain:        O(n)
 * - Radius Threshold:        O(n²)
 * - K-Nearest Neighbors:     O(n² log k)
 * - Minimum Spanning Tree:   O(n² log n)
 * - Gabriel Graph:           O(n³)
 * - Relative Neighborhood:   O(n³)
 *
 * Practical Real-Time Guidance (approximate):
 * -------------------------------------------
 * - Sequential:             thousands of points
 * - Radius / KNN:           a few hundred points
 * - Gabriel / RNG:          tens of points for interactive updates
 *
 * Batch Update Pattern for Expensive Modes:
 * ------------------------------------------
 * std::vector<Point> points;
 * for (...) {
 *     points.push_back({position, color});
 * }
 * topo->set_points(points);  // Single O(n³) rebuild
 *
 * Interactive Drawing Note:
 * -------------------------
 * When adding points continuously (e.g. mouse-move drawing),
 * prefer SEQUENTIAL, RADIUS_THRESHOLD, or small-k K_NEAREST modes.
 *
 * TopologyGeneratorNode prioritizes correctness and determinism
 * over incremental graph maintenance. Expensive modes are intended
 * for moderate point counts or batch generation.
 */
class MAYAFLUX_API TopologyGeneratorNode : public GeometryWriterNode {
public:
    using CustomConnectionFunction = std::function<std::vector<std::pair<size_t, size_t>>(
        const Eigen::MatrixXd&)>;

    struct Point {
        glm::vec3 position;
        glm::vec3 color { 1.0F, 1.0F, 1.0F };
        float influence_radius { 1.0F };
        bool connectable { true };
    };

    /**
     * @brief Create topology generator
     * @param mode Connection generation rule
     * @param auto_connect If true, regenerate topology on every point addition
     * @param max_points Maximum point capacity
     */
    explicit TopologyGeneratorNode(
        Kinesis::ProximityMode mode = Kinesis::ProximityMode::SEQUENTIAL,
        bool auto_connect = true,
        size_t max_points = 256);

    /**
     * @brief Create with custom connection function
     * @param custom_func User-provided topology generation
     * @param auto_connect Auto-regenerate flag
     * @param max_points Maximum capacity
     */
    TopologyGeneratorNode(
        CustomConnectionFunction custom_func,
        bool auto_connect = true,
        size_t max_points = 256);

    /**
     * @brief Add point to topology
     * @param point Point data
     *
     * If auto_connect enabled, immediately regenerates connections.
     */
    void add_point(const Point& point);

    /**
     * @brief Add point with just position (uses defaults)
     */
    void add_point(const glm::vec3& position);

    /**
     * @brief Remove point by index
     * @param index Point index to remove
     */
    void remove_point(size_t index);

    /**
     * @brief Update point position
     * @param index Point index
     * @param position New position
     */
    void update_point(size_t index, const glm::vec3& position);

    /**
     * @brief Update point data
     * @param index Point index
     * @param point New point data
     */
    void update_point(size_t index, const Point& point);

    /**
     * @brief Set all points at once
     * @param points Vector of point data
     */
    void set_points(const std::vector<Point>& points);

    /**
     * @brief Set points using just positions (color and other attributes defaulted)
     * @param positions Vector of point positions
     */
    void set_points(const std::vector<glm::vec3>& positions);

    /**
     * @brief Clear all points and connections
     */
    void clear();

    /**
     * @brief Manually trigger connection regeneration
     *
     * Call this if auto_connect is false and you've made multiple changes.
     */
    void regenerate_topology();

    /**
     * @brief Set connection mode
     * @param mode New connection rule
     */
    void set_connection_mode(Kinesis::ProximityMode mode);

    /**
     * @brief Enable/disable automatic connection regeneration
     * @param enable Auto-connect flag
     */
    void set_auto_connect(bool enable);

    /**
     * @brief Set K parameter for K_NEAREST mode
     * @param k Number of nearest neighbors
     */
    void set_k_neighbors(size_t k);

    /**
     * @brief Set radius for RADIUS_THRESHOLD mode
     * @param radius Maximum connection distance
     */
    void set_connection_radius(float radius);

    /**
     * @brief Set line color (applied to all connections)
     * @param color RGB color
     */
    void set_line_color(const glm::vec3& color);

    /**
     * @brief Set line thickness
     * @param thickness Line width
     */
    void set_line_thickness(float thickness);

    /**
     * @brief Get point count
     */
    [[nodiscard]] size_t get_point_count() const
    {
        return m_points.size();
    }

    /**
     * @brief Get connection count (edge count)
     */
    [[nodiscard]] size_t get_connection_count() const
    {
        return m_connections.size();
    }

    /**
     * @brief Get point by index
     */
    [[nodiscard]] const Point& get_point(size_t index) const;

    /**
     * @brief Get all points
     */
    [[nodiscard]] std::vector<Point> get_points() const;

    /**
     * @brief Get connection edges (pairs of point indices)
     */
    [[nodiscard]] const std::vector<std::pair<size_t, size_t>>& get_connections() const
    {
        return m_connections;
    }

    /**
     * @brief Compute frame - generates vertex data from points and connections
     */
    void compute_frame() override;

    /**
     * @brief Set custom connection function (for CUSTOM mode)
     * @param func User-provided topology generator
     */
    void set_path_interpolation_mode(Kinesis::InterpolationMode mode);

    /**
     * @brief Set number of samples per segment for interpolation
     * @param samples Number of samples to generate per connection segment
     *
     * Higher values produce smoother curves but increase vertex count.
     */
    void set_samples_per_segment(size_t samples);

    /**
     * @brief Enable or disable arc-length reparameterization for interpolation
     * @param enable If true, applies arc-length reparameterization for constant-speed traversal
     *
     * This can help maintain consistent visual speed along curves, especially for non-uniform point distributions.
     */
    void set_arc_length_reparameterization(bool enable);

private:
    Kinesis::ProximityMode m_mode;
    CustomConnectionFunction m_custom_func;
    Memory::HistoryBuffer<Point> m_points;
    std::vector<LineVertex> m_vertices;
    std::vector<std::pair<size_t, size_t>> m_connections;

    Kinesis::InterpolationMode m_path_interpolation_mode { Kinesis::InterpolationMode::CATMULL_ROM };
    size_t m_samples_per_segment { 20 }; ///< Controls smoothness vs performance
    bool m_use_arc_length_reparameterization { false }; ///< Optional constant-speed

    bool m_auto_connect;
    size_t m_k_neighbors { 3 };
    float m_connection_radius { 1.0F };

    glm::vec3 m_line_color { 1.0F, 1.0F, 1.0F };
    float m_line_thickness { 1.0F };

    void build_vertex_buffer();

    void build_interpolated_path(
        std::span<Point> points,
        size_t num_points);

    void build_direct_connections(std::span<Point> points, size_t num_points);

    Eigen::MatrixXd points_to_eigen() const;
};

} // namespace MayaFlux::Nodes::GpuSync
