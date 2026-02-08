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
     * @brief Initialize operator with positions from previous operator
     * @param positions Initial point positions (graphics coordinate space)
     * @param colors Optional colors (empty = use defaults)
     */
    virtual void initialize(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& colors = {})
        = 0;

    /**
     * @brief Extract current positions (for operator switching)
     */
    [[nodiscard]] [[nodiscard]] virtual std::vector<glm::vec3> extract_positions() const = 0;

    /**
     * @brief Extract current colors (for operator switching)
     */
    [[nodiscard]] virtual std::vector<glm::vec3> extract_colors() const = 0;

    /**
     * @brief Get vertex data for GPU upload
     */
    [[nodiscard]] virtual std::span<const uint8_t> get_vertex_data() const = 0;

    /**
     * @brief Get vertex layout describing vertex structure
     */
    [[nodiscard]] virtual const Kakshya::VertexLayout& get_vertex_layout() const = 0;

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
};

} // namespace MayaFlux::Nodes::Network::Operators
