#pragma once

#include "ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SDFFieldProcessor
 * @brief Preprocessor that evaluates an SDF field into a corner grid on the GPU.
 *
 * Dispatches sdf_field.comp, which includes sdf_primitives.glsl and writes
 * one float per grid corner into the grid buffer owned by SDFPrepProcessor.
 * SDFMeshProcessor (flat slot) reads this buffer as its SDF input.
 *
 * Receives the grid buffer at construction so it can bind it without
 * depending on the attached buffer's own storage.
 *
 * Push constant layout (must match sdf_field.comp exactly, 48 bytes):
 *   offset  0  vec3   bounds_min
 *   offset 12  float  time
 *   offset 16  vec3   step
 *   offset 28  uint   res_x
 *   offset 32  uint   res_y
 *   offset 36  uint   res_z
 *   offset 40  uint   _pad[2]
 *
 * Usage — wired by ComputeMeshBuffer::setup_processors when GPU field mode
 * is active:
 * @code
 * chain->add_preprocessor(m_field_processor, self);
 * @endcode
 */
class MAYAFLUX_API SDFFieldProcessor : public ComputeProcessor {
public:
    /**
     * @param grid_buf   HOST_STORAGE buffer sized to (res_x+1)*(res_y+1)*(res_z+1) floats.
     *                   Owned and zeroed each frame by SDFPrepProcessor.
     * @param bounds_min World-space minimum corner.
     * @param bounds_max World-space maximum corner.
     * @param res_x      Cell count along X.
     * @param res_y      Cell count along Y.
     * @param res_z      Cell count along Z.
     */
    SDFFieldProcessor(
        std::shared_ptr<VKBuffer> grid_buf,
        const glm::vec3& bounds_min,
        const glm::vec3& bounds_max,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z);

    ~SDFFieldProcessor() override = default;

    /** @brief Replace the evaluation volume. */
    void set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max);

    /** @brief Advance the time uniform (drives animation in sdf_field.comp). */
    void set_time(float t) { m_pc.time = t; }
    [[nodiscard]] float get_time() const { return m_pc.time; }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    struct PC {
        glm::vec3 bounds_min;
        float time { 0.0F };
        glm::vec3 step;
        uint32_t res_x;
        uint32_t res_y;
        uint32_t res_z;
        uint32_t _pad[2] {};
    };
    static_assert(sizeof(PC) % 16 == 0);

    PC m_pc;
    std::shared_ptr<VKBuffer> m_grid_buf;
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;

    void update_step();
};

} // namespace MayaFlux::Buffers
