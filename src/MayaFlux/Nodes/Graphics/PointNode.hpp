#pragma once

#include "GeometryWriterNode.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class PointNode
 * @brief Single 3D point in space
 *
 * GeometryWriterNode that renders exactly ONE point.
 * Can be used standalone or as building block in NodeNetworks.
 *
 * Usage:
 * ```cpp
 * auto point = std::make_shared<PointNode>(glm::vec3(0.0f, 0.0f, 0.0f));
 * auto buffer = std::make_shared<GeometryBuffer>(point);
 * buffer->initialize();
 * ```
 */
class MAYAFLUX_API PointNode : public GeometryWriterNode {
public:
    /**
     * @brief Create point at origin
     */
    PointNode();

    /**
     * @brief Create point at specific position
     * @param position Initial 3D position
     * @param color Initial color
     * @param size Initial point size
     */
    explicit PointNode(const glm::vec3& position, const glm::vec3& color = glm::vec3(1.0F), float size = 10.0F);

    /**
     * @brief Set point position
     * @param position New 3D position
     */
    void set_position(const glm::vec3& position);

    /**
     * @brief Set point size
     * @param size New point size
     */
    void set_size(float size);

    /**
     * @brief Set point color
     * @param color New point color
     */
    void set_color(const glm::vec3& color);

    /**
     * @brief Get current position
     * @return 3D position
     */
    [[nodiscard]] glm::vec3 get_position() const { return m_position; }

    /**
     * @brief Get current color
     * @return Point color
     */
    [[nodiscard]] glm::vec3 get_color() const { return m_color; }

    /**
     * @brief Get current size
     * @return Point size
     */
    [[nodiscard]] float get_size() const { return m_size; }

    /**
     * @brief Compute frame - upload single point to vertex buffer
     */
    void compute_frame() override;

private:
    glm::vec3 m_position;
    glm::vec3 m_color;
    float m_size { 10.0F };
};

} // namespace MayaFlux::Nodes
