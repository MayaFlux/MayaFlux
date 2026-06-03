#pragma once

#include "MayaFlux/Buffers/Shaders/SDFMeshProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class ComputeMeshBuffer
 * @brief VKBuffer that evaluates a SpatialField isosurface via marching cubes
 *        on the GPU and renders directly without CPU readback.
 *
 * SDFMeshProcessor dispatches mc_emit.comp into this buffer each dirty frame.
 * RenderProcessor draws from the same VKBuffer. No CPU-GPU round-trip occurs
 * after the initial lookup table upload.
 *
 * Usage:
 * @code
 * auto buf = std::make_shared<ComputeMeshBuffer>(
 *     make_field(), glm::vec3(-1.6F), glm::vec3(1.6F), 40, 40, 40)
 *     | Graphics;
 * buf->setup_rendering({ .target_window = window });
 *
 * schedule_metro(1.0 / 60.0, [buf, make_field]() {
 *     buf->set_field(make_field());
 *     buf->set_dirty();
 * }, "sdf_anim", Vruta::ProcessingToken::FRAME_ACCURATE);
 * @endcode
 */
class MAYAFLUX_API ComputeMeshBuffer : public VKBuffer {
public:
    /**
     * @param field      SpatialField evaluated on CPU each dirty frame.
     * @param bounds_min World-space minimum corner.
     * @param bounds_max World-space maximum corner.
     * @param res_x      Cell count along X (minimum 1).
     * @param res_y      Cell count along Y (minimum 1).
     * @param res_z      Cell count along Z (minimum 1).
     * @param iso_level  Isosurface threshold (default 0.0).
     */
    ComputeMeshBuffer(
        Kinesis::SpatialField field,
        const glm::vec3& bounds_min,
        const glm::vec3& bounds_max,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z,
        float iso_level = 0.0F);

    ~ComputeMeshBuffer() override = default;

    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Create the RenderProcessor and attach it to the processing chain.
     * @param config  At minimum, supply target_window. Shader paths default to
     *                triangle.vert.spv / triangle.frag.spv (or mesh_textured.frag.spv
     *                when a texture has been set).
     */
    void setup_rendering(const RenderConfig& config);

    // -------------------------------------------------------------------------
    // Field and geometry control
    // -------------------------------------------------------------------------

    /** @brief Replace the scalar field and mark dirty. */
    void set_field(Kinesis::SpatialField field);

    /** @brief Replace the evaluation volume and mark dirty. */
    void set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max);

    /**
     * @brief Replace the grid resolution.
     *
     * Resizes the internal grid and counter buffers. Marks dirty.
     */
    void set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z);

    /** @brief Replace the iso_level threshold and mark dirty. */
    void set_iso_level(float iso_level);

    /** @brief Request a re-dispatch on the next graphics frame. */
    void set_dirty();

    // -------------------------------------------------------------------------
    // Texture
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a diffuse texture sampled in the fragment shader.
     * @param image   GPU image (nullptr clears the binding).
     * @param binding Descriptor name (default: "diffuseTex").
     *
     * Selects mesh_textured.frag.spv when setup_rendering is called after this.
     * Calling after setup_rendering forwards the binding to RenderProcessor directly.
     */
    void set_texture(
        std::shared_ptr<Core::VKImage> image,
        std::string binding = "diffuseTex");

private:
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;

    std::shared_ptr<SDFMeshProcessor> m_sdf_processor;

    std::shared_ptr<Core::VKImage> m_diffuse_texture;
    std::string m_diffuse_binding { "diffuseTex" };

    // Field parameters held until m_sdf_processor is constructed.
    Kinesis::SpatialField m_field;
    glm::vec3 m_bounds_min;
    glm::vec3 m_bounds_max;
    float m_iso_level;

    [[nodiscard]] static size_t worst_case_bytes(
        uint32_t res_x, uint32_t res_y, uint32_t res_z) noexcept
    {
        return static_cast<size_t>(res_x) * res_y * res_z * 15U * sizeof(Kakshya::MeshVertex);
    }
};

} // namespace MayaFlux::Buffers
