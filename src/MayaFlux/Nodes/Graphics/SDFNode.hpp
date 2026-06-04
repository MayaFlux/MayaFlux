#pragma once

#include "MeshWriterNode.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class SDFNode
 * @brief MeshWriterNode that extracts a TRIANGLE_LIST isosurface from a
 *        Kinesis::SpatialField each frame via marching cubes.
 *
 * The field is any glm::vec3 -> float callable wrapped in a SpatialField.
 * Kinesis::TendencyFactories and composition operators (combine, chain,
 * select) provide the full vocabulary without any new types.
 *
 * The grid is re-evaluated and a new mesh extracted only when the node is
 * marked dirty by a setter call. compute_frame() is otherwise a no-op,
 * making the node safe to tick at VISUAL_RATE for static fields.
 *
 * Usage:
 * @code
 * auto node = std::make_shared<SDFNode>(
 *     Kinesis::SpatialField { .fn = [](const glm::vec3& p) {
 *         return glm::length(p) - 1.0F;  // unit sphere
 *     }},
 *     glm::vec3(-1.5F), glm::vec3(1.5F),
 *     32, 32, 32);
 *
 * auto buf = vega.GeometryBuffer(node) | Graphics;
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 *
 * Audio-reactive deformation:
 * @code
 * auto radius = make_persistent_shared<std::atomic<float>>(1.0F);
 *
 * node->set_field(Kinesis::SpatialField { .fn = [radius](const glm::vec3& p) {
 *     return glm::length(p) - radius->load(std::memory_order_relaxed);
 * }});
 *
 * // Audio metro writes radius, graphics metro marks node dirty.
 * @endcode
 */
class MAYAFLUX_API SDFNode : public MeshWriterNode {
public:
    /**
     * @brief Construct and evaluate the initial isosurface.
     *
     * @param field      SpatialField: glm::vec3 -> float.
     * @param bounds_min World-space minimum corner of the evaluation volume.
     * @param bounds_max World-space maximum corner of the evaluation volume.
     * @param res_x      Cell count along X. Clamped to minimum 1.
     * @param res_y      Cell count along Y. Clamped to minimum 1.
     * @param res_z      Cell count along Z. Clamped to minimum 1.
     * @param iso_level  Isosurface threshold. Default 0.0.
     */
    SDFNode(
        Kinesis::SpatialField field,
        const glm::vec3& bounds_min,
        const glm::vec3& bounds_max,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z,
        float iso_level = 0.0F);

    ~SDFNode() override = default;

    /**
     * @brief Replace the scalar field and mark dirty.
     * @param field New SpatialField.
     */
    void set_field(Kinesis::SpatialField field);

    /**
     * @brief Replace the evaluation volume and mark dirty.
     * @param bounds_min New minimum corner.
     * @param bounds_max New maximum corner.
     */
    void set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max);

    /**
     * @brief Replace the grid resolution and mark dirty.
     * @param res_x Cell count along X. Clamped to minimum 1.
     * @param res_y Cell count along Y. Clamped to minimum 1.
     * @param res_z Cell count along Z. Clamped to minimum 1.
     */
    void set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    /**
     * @brief Replace the iso_level threshold and mark dirty.
     * @param iso_level New threshold value.
     */
    void set_iso_level(float iso_level);

    /**
     * @brief Re-extract the isosurface if dirty, then upload via parent.
     */
    void compute_frame() override;

private:
    Kinesis::SpatialField m_field;
    glm::vec3 m_bounds_min;
    glm::vec3 m_bounds_max;
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;
    float m_iso_level;
    bool m_dirty { false };

    void rebuild();
};

} // namespace MayaFlux::Nodes::GpuSync
