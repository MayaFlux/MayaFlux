#pragma once
#include "GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Graphics/TopologyGeneratorNode.hpp"

namespace MayaFlux::Nodes::Network {

class MAYAFLUX_API TopologyOperator : public GraphicsOperator {
public:
    explicit TopologyOperator(
        Kinesis::ProximityMode mode = Kinesis::ProximityMode::K_NEAREST);

    /**
     * @brief Initialize a single topology with single set of vertices
     */
    void initialize(const std::vector<LineVertex>& vertices);

    /**
     * @brief Initialize multiple topologies with given positions and properties.
     * @param topologies Vector of position vectors, one per topology.
     * @param mode Proximity mode for all topologies.
     */
    void initialize_topologies(
        const std::vector<std::vector<LineVertex>>& topologies,
        Kinesis::ProximityMode mode);

    /**
     * @brief Add a single topology with full per-vertex control.
     * @param vertices Vector of LineVertex defining the topology's points and attributes.
     * @param mode Proximity mode for this topology.
     */
    void add_topology(
        const std::vector<LineVertex>& vertices,
        Kinesis::ProximityMode mode);

    void process(float dt) override;

    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    /**
     * @brief Extract current vertex data as LineVertex array
     * @return Vector of LineVertex with current positions, colors, thicknesses
     */
    [[nodiscard]] std::vector<LineVertex> extract_vertices() const;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Topology"; }
    [[nodiscard]] size_t get_point_count() const override;

    /**
     * @brief Set the connection radius for topology generation.
     * @param radius Connection radius.
     */
    void set_connection_radius(float radius);

    /**
     * @brief Set the number of neighbors (k) for K-Nearest topology mode.
     * @param k Number of neighbors.
     */
    void set_global_line_thickness(float thickness);

    /**
     * @brief Set the line color for all topologies.
     * @param color RGB color.
     */
    void set_global_line_color(const glm::vec3& color);

    /**
     * @brief Get number of topologies currently stored
     * @return Topology count
     */
    [[nodiscard]] size_t get_topology_count() const { return m_topologies.size(); }

    const char* get_vertex_type_name() const override { return "PathVertex"; }

protected:
    void* get_data_at(size_t global_index) override;

private:
    std::vector<std::shared_ptr<GpuSync::TopologyGeneratorNode>> m_topologies;

    mutable std::vector<uint8_t> m_vertex_data_aggregate;

    Kinesis::ProximityMode m_default_mode;
    float m_default_thickness { 2.0F };
};

} // namespace MayaFlux::Nodes::Network
