#pragma once
#include "GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Graphics/PathGeneratorNode.hpp"

namespace MayaFlux::Nodes::Network {

class MAYAFLUX_API PathOperator : public GraphicsOperator {
public:
    struct PathCollection {
        std::shared_ptr<GpuSync::PathGeneratorNode> generator;
        std::vector<glm::vec3> control_points;
        glm::vec3 color_tint;
        float thickness_scale;
    };

    explicit PathOperator(
        Kinesis::InterpolationMode mode = Kinesis::InterpolationMode::CATMULL_ROM,
        Eigen::Index samples_per_segment = 32);

    void initialize(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& colors = {}) override;

    /**
     * @brief Initialize multiple paths with given control points and properties.
     * @param paths Vector of control point vectors, one per path.
     * @param mode Interpolation mode for all paths.
     * @param color_tints Optional vector of color tints (one per path, default white).
     * @param thickness_scales Optional vector of thickness scales (one per path, default 1.0).
     */
    void initialize_paths(
        const std::vector<std::vector<glm::vec3>>& paths,
        Kinesis::InterpolationMode mode,
        const std::vector<glm::vec3>& color_tints = {},
        const std::vector<float>& thickness_scales = {});

    /**
     * @brief Add a new path with given control points and properties.
     * @param control_points Control points defining the path.
     * @param mode Interpolation mode for the path.
     * @param color_tint Color tint applied to the path (default white).
     * @param thickness_scale Thickness multiplier for the path (default 1.0).
     */
    void add_path(
        const std::vector<glm::vec3>& control_points,
        Kinesis::InterpolationMode mode,
        glm::vec3 color_tint = glm::vec3(1.0F),
        float thickness_scale = 1.0F);

    void process(float dt) override;

    [[nodiscard]] std::vector<glm::vec3> extract_positions() const override;
    [[nodiscard]] std::vector<glm::vec3> extract_colors() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Path"; }
    [[nodiscard]] size_t get_point_count() const override;

    /**
     * @brief Set the number of samples per segment for all paths.
     * @param samples Number of samples to generate per segment between control points.
     */
    void set_samples_per_segment(Eigen::Index samples);

    /**
     * @brief Set the tension parameter for all paths (if supported by mode).
     * @param tension Tension value (e.g., for cubic Hermite interpolation).
     */
    void set_tension(double tension);

    /**
     * @brief Set the global thickness for all paths.
     * @param thickness Thickness value to apply to all paths.
     */
    void set_global_thickness(float thickness);

    /**
     * @brief Set the global color tint for all paths.
     * @param color Color tint to apply to all paths (multiplied with individual tints).
     */
    void set_global_color(const glm::vec3& color);

    /**
     * @brief Get the number of paths currently managed by this operator.
     * @return Number of path collections.
     */
    [[nodiscard]] size_t get_path_count() const { return m_paths.size(); }

    const char* get_vertex_type_name() const override { return "PathVertex"; }

protected:
    void* get_data_at(size_t global_index) override;

private:
    std::vector<PathCollection> m_paths;

    mutable std::vector<uint8_t> m_vertex_data_aggregate;

    Kinesis::InterpolationMode m_default_mode;
    Eigen::Index m_default_samples_per_segment;
    float m_default_thickness { 2.0F };
};

} // namespace MayaFlux::Nodes::Network
