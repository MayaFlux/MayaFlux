#pragma once

#include "ComputeProcessor.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SDFMeshProcessor
 * @brief ComputeProcessor that dispatches mc_emit.comp directly into the
 *        owning VKBuffer (Usage::VERTEX), bypassing all CPU readback.
 *
 * Binding layout (matches mc_emit.comp):
 *   set=0, binding=0  SDF corner grid        STORAGE (host-visible, written each dirty frame)
 *   set=0, binding=1  Edge table             STORAGE (uploaded once at init)
 *   set=0, binding=2  Tri table              STORAGE (uploaded once at init)
 *   set=0, binding=3  Vertex output          STORAGE (the VKBuffer this processor is attached to)
 *   set=0, binding=4  Atomic vertex counter  STORAGE (host-visible, read back after dispatch)
 *
 * The SDF grid is evaluated on CPU via a parallel for_each into the grid
 * buffer's mapped pointer each dirty frame. No staging or upload call is
 * needed: the buffer is eHostVisible | eHostCoherent.
 *
 * After dispatch, the counter buffer's mapped pointer is read to obtain the
 * actual vertex count, which is forwarded to the owner via the callback
 * supplied at construction.
 *
 * Dirtiness is managed by the owner. on_before_execute returns false when
 * not dirty, suppressing dispatch for that frame.
 */
class MAYAFLUX_API SDFMeshProcessor : public ComputeProcessor {
public:
    /**
     * @param field      SpatialField evaluated on CPU each dirty frame.
     * @param bounds_min World-space minimum corner.
     * @param bounds_max World-space maximum corner.
     * @param res_x      Cell count along X (minimum 1).
     * @param res_y      Cell count along Y (minimum 1).
     * @param res_z      Cell count along Z (minimum 1).
     * @param iso_level  Isosurface threshold.
     */
    SDFMeshProcessor(
        Kinesis::SpatialField field,
        const glm::vec3& bounds_min,
        const glm::vec3& bounds_max,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z,
        float iso_level);

    ~SDFMeshProcessor() override = default;

    /** @brief Replace the scalar field and mark dirty. */
    void set_field(Kinesis::SpatialField field);

    /** @brief Replace the evaluation volume and mark dirty. */
    void set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max);

    /**
     * @brief Replace the grid resolution and mark dirty.
     *
     * Triggers recreation of the grid and counter buffers on the next attach
     * or dirty frame. Descriptor rebuild is forced.
     */
    void set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    /** @brief Replace the iso_level threshold and mark dirty. */
    void set_iso_level(float iso_level);

    /** @brief Force a re-dispatch on the next frame. */
    void set_dirty() { m_dirty = true; }

    [[nodiscard]] bool is_dirty() const { return m_dirty; }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_descriptors_created() override;
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;
    void on_after_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    Kinesis::SpatialField m_field;
    glm::vec3 m_bounds_min;
    glm::vec3 m_bounds_max;
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;
    float m_iso_level;
    bool m_dirty { true };

    std::shared_ptr<VKBuffer> m_grid_buf;
    std::shared_ptr<VKBuffer> m_edge_buf;
    std::shared_ptr<VKBuffer> m_tri_buf;
    std::shared_ptr<VKBuffer> m_counter_buf;

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

    void rebuild_owned_buffers();
    void evaluate_grid();

    [[nodiscard]] uint32_t corner_count() const noexcept
    {
        return (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    }

    [[nodiscard]] uint32_t voxel_count() const noexcept
    {
        return m_res_x * m_res_y * m_res_z;
    }

    [[nodiscard]] uint32_t worst_case_vertices() const noexcept
    {
        return voxel_count() * 15U;
    }
};

} // namespace MayaFlux::Buffers
