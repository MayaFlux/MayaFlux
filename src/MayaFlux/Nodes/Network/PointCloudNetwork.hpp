#pragma once

#include "NodeNetwork.hpp"

#include "Operators/PathOperator.hpp"
#include "Operators/TopologyOperator.hpp"

#include "MayaFlux/Kinesis/Stochastic.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class PointCloudNetwork
 * @brief Spatial relational network operating on unordered point sets.
 *
 * PointCloudNetwork represents a literal point cloud:
 * a collection of spatial samples (positions + optional attributes)
 * with no inherent identity, persistence, or physics semantics.
 *
 * The network itself performs no computation. All structural,
 * topological, or interpolative behavior is delegated to
 * attached NetworkOperator instances.
 *
 * --------------------------------------------------------------------------
 * Conceptual Model
 * --------------------------------------------------------------------------
 *
 * Designed for computational graphics and data visualization.
 * While it can ingest point cloud data from external sources
 * (lidar, scanning, etc.), its purpose is generating and visualizing
 * structure through algorithmic relationships — not surface reconstruction
 * or scene analysis.
 *
 * - ParticleNetwork models entities that evolve over time.
 * - PointCloudNetwork models spatial samples whose meaning
 *   emerges through algorithmically defined relationships.
 *
 * Points in this network:
 * - Have no identity beyond index.
 * - Do not simulate motion or forces.
 * - Do not own connectivity.
 * - Exist purely as a spatial substrate.
 *
 * Operators define interpretation:
 * - TopologyOperator infers connectivity (KNN, radius, Delaunay, MST, etc.)
 * - PathOperator interpolates structure through control points.
 *
 * Rendering and processing backends remain completely agnostic
 * to network type — they consume vertex data produced by operators.
 *
 * --------------------------------------------------------------------------
 * Modality-Agnostic Design
 * --------------------------------------------------------------------------
 *
 * Points may originate from any source:
 * - Procedural generation
 * - Texture sampling
 * - Image analysis
 * - Audio feature extraction
 * - External datasets
 *
 * Once spatialized, the network treats them uniformly as relational samples.
 *
 * --------------------------------------------------------------------------
 * Important Distinction
 * --------------------------------------------------------------------------
 *
 * If you need per-entity state, temporal evolution, or physics simulation,
 * use ParticleNetwork.
 *
 * If you need structural relationships between spatial samples,
 * use PointCloudNetwork.
 *
 * PointCloudNetwork is for STRUCTURE.
 * ParticleNetwork is for MOTION.
 *
 * Common use cases:
 * - Network graphs (social, neural, data visualization)
 * - Procedural topology generation (Delaunay, MST, proximity)
 * - Path visualization (trajectories, routes, flow lines)
 * - Data-driven connectivity inference
 * - Gradient-based color visualization
 *
 * --------------------------------------------------------------------------
 * Usage Example
 * --------------------------------------------------------------------------
 *
 * ```cpp
 * // Single topology network
 * auto cloud = std::make_shared<PointCloudNetwork>(500);
 * cloud->create_operator<TopologyOperator>(Kinesis::ProximityMode::K_NEAREST);
 * cloud->initialize();
 *
 * // Multiple independent topologies
 * auto cloud = std::make_shared<PointCloudNetwork>();
 * auto topo = cloud->create_operator<TopologyOperator>(Kinesis::ProximityMode::RADIUS);
 * topo->initialize_topologies({group1_points, group2_points}, mode);
 * cloud->set_operator(std::move(topo));
 *
 * // Path-based visualization
 * auto cloud = std::make_shared<PointCloudNetwork>();
 * auto paths = cloud->create_operator<PathOperator>();
 * paths->initialize_paths({path1, path2, path3}, InterpolationMode::CATMULL_ROM);
 * cloud->set_operator(std::move(paths));
 *
 * // Per-point color gradient
 * cloud->apply_color_gradient(start_color, end_color);
 * cloud->set_point_colors(custom_colors);
 * ```
 */
class MAYAFLUX_API PointCloudNetwork : public NodeNetwork {
public:
    enum class InitializationMode : uint8_t {
        UNIFORM_GRID,
        RANDOM_SPHERE,
        RANDOM_CUBE,
        PROCEDURAL,
        EMPTY
    };

    /**
     * @brief Create empty point cloud network
     */
    PointCloudNetwork();

    /**
     * @brief Create network with initial point count and bounds
     * @param num_points Number of points to generate
     * @param bounds_min Minimum coordinate bounds
     * @param bounds_max Maximum coordinate bounds
     * @param init_mode How to distribute initial positions
     */
    explicit PointCloudNetwork(
        size_t num_points,
        const glm::vec3& bounds_min = glm::vec3(-1.0F),
        const glm::vec3& bounds_max = glm::vec3(1.0F),
        InitializationMode init_mode = InitializationMode::RANDOM_CUBE);

    void initialize() override;
    void reset() override;
    void process_batch(unsigned int num_samples) override;
    void set_topology(Topology topology) override;

    [[nodiscard]] size_t get_node_count() const override;
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    /**
     * @brief Set all point vertices
     * @param vertices Vector of LineVertex containing position, color, thickness
     */
    void set_vertices(const std::vector<LineVertex>& vertices);

    /**
     * @brief Set colors for all points
     * @param colors Vector of RGB colors
     */
    // void set_colors(const std::vector<glm::vec3>& colors);

    /**
     * @brief Apply linear color gradient across points
     * @param start_color Color for first point
     * @param end_color Color for last point
     */
    void apply_color_gradient(const glm::vec3& start_color, const glm::vec3& end_color);

    /**
     * @brief Apply radial color gradient from center
     * @param center_color Color at center
     * @param edge_color Color at maximum distance
     * @param center Center point for gradient calculation
     */
    void apply_radial_gradient(
        const glm::vec3& center_color,
        const glm::vec3& edge_color,
        const glm::vec3& center = glm::vec3(0.0F));

    /**
     * @brief Get all point vertices
     */
    [[nodiscard]] std::vector<LineVertex> get_vertices() const;

    /**
     * @brief Update single vertex completely
     * @param index Point index
     * @param vertex New LineVertex data
     */
    void update_vertex(size_t index, const LineVertex& vertex);

    /**
     * @brief Set global line color for topology/path rendering
     * @param color RGB color
     */
    void set_line_color(const glm::vec3& color);

    /**
     * @brief Set connection radius for topology generation (TopologyOperator only)
     * @param radius Maximum distance for connections
     */
    void set_connection_radius(float radius);

    /**
     * @brief Set K value for K-nearest neighbors (TopologyOperator only)
     * @param k Number of nearest neighbors
     */
    void set_k_neighbors(size_t k);

    /**
     * @brief Set line thickness for topology/path rendering
     * @param thickness Line thickness in pixels
     */
    void set_line_thickness(float thickness);

    /**
     * @brief Set samples per segment for path interpolation (PathOperator only)
     * @param samples Number of samples between control points
     */
    void set_samples_per_segment(size_t samples);

    /**
     * @brief Set tension for Catmull-Rom interpolation (PathOperator only)
     * @param tension Tension parameter (0.0 = loose, 1.0 = tight)
     */
    void set_tension(double tension);

    NetworkOperator* get_operator() override { return m_operator.get(); }
    const NetworkOperator* get_operator() const override { return m_operator.get(); }
    bool has_operator() const override { return m_operator != nullptr; }

    /**
     * @brief Create and set operator in one call
     */
    template <typename OpType, typename... Args>
    OpType* create_operator(Args&&... args)
    {
        auto op = std::make_unique<OpType>(std::forward<Args>(args)...);
        auto* ptr = op.get();
        set_operator(std::move(op));
        return ptr;
    }

    void set_operator(std::unique_ptr<NetworkOperator> op);

private:
    size_t m_num_points { 0 };
    glm::vec3 m_bounds_min { -1.0F };
    glm::vec3 m_bounds_max { 1.0F };
    InitializationMode m_init_mode { InitializationMode::RANDOM_CUBE };
    Kinesis::Stochastic::Stochastic m_random_gen;

    std::unique_ptr<NetworkOperator> m_operator;

    std::vector<LineVertex> m_cached_vertices;

    [[nodiscard]] std::vector<LineVertex> generate_initial_positions();

    /**
     * @brief Update mapped parameters before path/topology processing
     */
    void update_mapped_parameters();
};

} // namespace MayaFlux::Nodes::Network
