#pragma once

#include "PointNode.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class PointCollectionNode
 * @brief Unstructured collection of 3D points for visualization
 *
 * Pure rendering helper - no relationships between points.
 * Use for: static data viz, debug markers, reference grids.
 *
 * For dynamic/physics-driven points, use ParticleNetwork instead.
 */
class MAYAFLUX_API PointCollectionNode : public GeometryWriterNode {
public:
    /**
     * @brief Create empty point source
     * @param initial_capacity Reserve space for N points
     */
    explicit PointCollectionNode(size_t initial_capacity = 1024);

    /**
     * @brief Create with initial points
     * @param points Vector of PointVertex
     */
    explicit PointCollectionNode(std::vector<PointVertex> points);

    /**
     * @brief Add a point to the collection
     * @param position 3D position
     */
    // void add_point(const glm::vec3& position, const glm::vec3& color = glm::vec3(1.0f), float size = 5.0f);
    void add_point(const PointVertex& point);

    /**
     * @brief Add multiple points
     * @param positions Vector of positions
     */
    void add_points(const std::vector<PointVertex>& points);

    /**
     * @brief Set all points (replaces existing)
     * @param positions New positions
     */
    void set_points(const std::vector<PointVertex>& points);

    /**
     * @brief Update specific point position
     * @param index Point index
     * @param position New position
     */
    void update_point(size_t index, const PointVertex& point);

    /**
     * @brief Get point position
     * @param index Point index
     * @return Position
     */
    [[nodiscard]] PointVertex get_point(size_t index) const;

    /**
     * @brief Get all points
     * @return Vector of positions
     */
    [[nodiscard]] const std::vector<PointVertex>& get_points() const
    {
        return m_points;
    }

    /**
     * @brief Get all points
     * @return Vector of positions
     */
    [[nodiscard]] std::vector<PointVertex>& get_points()
    {
        return m_points;
    }

    /**
     * @brief Clear all points
     */
    void clear_points();

    /**
     * @brief Get number of points
     * @return Point count
     */
    [[nodiscard]] size_t get_point_count() const
    {
        return m_points.size();
    }

    /**
     * @brief Compute frame - uploads positions to vertex buffer
     */
    void compute_frame() override;

private:
    std::vector<PointVertex> m_points;
};

} // namespace MayaFlux::Nodes
