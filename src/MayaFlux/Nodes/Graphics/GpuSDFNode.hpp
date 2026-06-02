#pragma once

#include "MeshWriterNode.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Transformers/GpuTransformer.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GpuSDFNode
 * @brief MeshWriterNode that extracts a TRIANGLE_LIST isosurface from a
 *        Kinesis::SpatialField using two GPU marching-cubes compute passes.
 *
 * The SDF grid is evaluated on CPU once per dirty cycle and fed into a pair
 * of GpuTransformer operations:
 *
 *   Pass 1 (mc_count.comp): reads the grid, writes a per-voxel uint triangle
 *   count. The node performs an exclusive prefix-sum on the readback on CPU.
 *
 *   Pass 2 (mc_emit.comp): reads the grid and the prefix offsets, writes
 *   packed MeshVertex data and uint32_t indices at the correct offsets.
 *
 * The output bytes are sliced to the actual vertex count and handed directly
 * to set_mesh(). compute_frame() is a no-op when not dirty.
 *
 * Usage:
 * @code
 * auto node = std::make_shared<GpuSDFNode>(
 *     Kinesis::SpatialField { .fn = [](const glm::vec3& p) {
 *         return glm::length(p) - 1.0F;
 *     }},
 *     glm::vec3(-1.5F), glm::vec3(1.5F),
 *     32, 32, 32);
 *
 * auto buf = vega.GeometryBuffer(node) | Graphics;
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API GpuSDFNode : public MeshWriterNode {
public:
    /**
     * @param field      SpatialField evaluated on CPU to fill the grid.
     * @param bounds_min World-space minimum corner.
     * @param bounds_max World-space maximum corner.
     * @param res_x      Cell count along X. Clamped to minimum 1.
     * @param res_y      Cell count along Y. Clamped to minimum 1.
     * @param res_z      Cell count along Z. Clamped to minimum 1.
     * @param iso_level  Isosurface threshold. Default 0.0.
     */
    GpuSDFNode(
        Kinesis::SpatialField field,
        const glm::vec3& bounds_min,
        const glm::vec3& bounds_max,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z,
        float iso_level = 0.0F);

    ~GpuSDFNode() override = default;

    /** @brief Replace the scalar field and mark dirty. */
    void set_field(Kinesis::SpatialField field);

    /** @brief Replace the evaluation volume and mark dirty. */
    void set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max);

    /**
     * @brief Replace the grid resolution and mark dirty.
     *
     * Rebuilds both GpuTransformer instances to match the new buffer sizes.
     */
    void set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    /** @brief Replace the iso_level threshold and mark dirty. */
    void set_iso_level(float iso_level);

    /** @brief Force a re-dispatch on the next compute_frame(). */
    void set_dirty() { m_dirty = true; }

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

    // Shared push constant layout for both shaders.
    struct McPC {
        glm::vec3 bounds_min;
        float iso_level;
        glm::vec3 step;
        uint32_t res_x;
        uint32_t res_y;
        uint32_t res_z;
        uint32_t _pad[2] {};
    };
    static_assert(sizeof(McPC) % 16 == 0);

    using McExecutor = Yantra::ShaderExecutionContext<>;
    using McTransformer = Yantra::GpuTransformer<>;

    std::shared_ptr<McExecutor> m_count_exec;
    std::shared_ptr<McExecutor> m_emit_exec;
    std::shared_ptr<McTransformer> m_count_op;
    std::shared_ptr<McTransformer> m_emit_op;

    void build_operations(const std::vector<uint32_t>& edge_flat, const std::vector<int32_t>& tri_flat);

    void rebuild();

    [[nodiscard]] uint32_t voxel_count() const noexcept
    {
        return m_res_x * m_res_y * m_res_z;
    }

    [[nodiscard]] uint32_t corner_count() const noexcept
    {
        return (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    }

    [[nodiscard]] uint32_t worst_case_vertices() const noexcept
    {
        return voxel_count() * 15U;
    }

    void build_count(
        const std::vector<float>& grid,
        const std::vector<uint32_t>& edge_flat,
        const std::vector<int32_t>& tri_flat,
        const McPC& pc);

    void build_emit(
        const std::vector<float>& grid,
        const std::vector<uint32_t>& edge_flat,
        const std::vector<int32_t>& tri_flat,
        const std::vector<uint32_t>& prefix,
        const McPC& pc);
};

} // namespace MayaFlux::Nodes::GpuSync
