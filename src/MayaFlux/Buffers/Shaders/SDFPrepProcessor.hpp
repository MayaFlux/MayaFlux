#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SDFPrepProcessor
 * @brief Default processor for the GPU-field marching-cubes chain.
 *
 * Owns the SDF corner grid buffer and the atomic vertex counter buffer.
 * Zeros both on each frame so downstream processors start from a clean state.
 * Performs no shader dispatch of its own.
 *
 * Active chain order:
 *   default  — SDFPrepProcessor   (this, owns + zeros grid/counter)
 *   pre      — SDFFieldProcessor  (writes grid via sdf_field.comp)
 *   flat[0]  — SDFMeshProcessor   (mc_emit, reads grid, emits verts)
 *   final    — RenderProcessor
 *
 * @see SDFFieldProcessor
 * @see SDFMeshProcessor
 */
class MAYAFLUX_API SDFPrepProcessor : public VKBufferProcessor {
public:
    /**
     * @param res_x  Cell count along X.
     * @param res_y  Cell count along Y.
     * @param res_z  Cell count along Z.
     */
    SDFPrepProcessor(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    ~SDFPrepProcessor() override = default;

    /**
     * @brief Rebuild owned buffers to match a new resolution.
     *
     * Must be called before the next frame when resolution changes.
     */
    void set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    /** @brief Grid buffer written by SDFFieldProcessor, read by SDFMeshProcessor. */
    [[nodiscard]] std::shared_ptr<VKBuffer> grid_buf() const { return m_grid_buf; }

    /** @brief Counter buffer zeroed here, written by mc_emit, read back by SDFMeshProcessor. */
    [[nodiscard]] std::shared_ptr<VKBuffer> counter_buf() const { return m_counter_buf; }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;

    std::shared_ptr<VKBuffer> m_grid_buf;
    std::shared_ptr<VKBuffer> m_counter_buf;

    void rebuild_buffers();

    [[nodiscard]] uint32_t corner_count() const noexcept
    {
        return (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    }
};

} // namespace MayaFlux::Buffers
