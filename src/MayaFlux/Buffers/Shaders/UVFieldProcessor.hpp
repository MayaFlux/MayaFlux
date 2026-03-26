#pragma once

#include "ComputeProcessor.hpp"

#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {

/**
 * @enum UVProjectionMode
 * @brief Projection mode encoded in push constants for uv_field.comp
 */
enum class UVProjectionMode : uint8_t { ///< Backing type matches push constant uint in uv_field.comp
    CARTESIAN = 0,
    CYLINDRICAL = 1,
    SPHERICAL = 2,
    AXIAL_BLEND = 3
};

/**
 * @class UVFieldProcessor
 * @brief Compute pass that writes UV and optionally samples colour into a vertex SSBO
 *
 * Runs after NetworkGeometryProcessor has uploaded vertex data and before
 * RenderProcessor draws.  The vertex buffer (Usage::VERTEX, already carries
 * eStorageBuffer) is bound as an SSBO at binding 0.  A VKImage is bound as a
 * combined image sampler at binding 1.
 *
 * The compute shader evaluates the chosen projection for every vertex in one
 * dispatch, writing glm::vec2 UV at byte offset 28 and, when a texture source
 * is present, glm::vec3 colour (sampled from the image at the computed UV) at
 * byte offset 12.
 *
 * Push constant layout (uv_field.comp must match exactly, 80 bytes):
 *   offset  0  uint  vertex_count
 *   offset  4  uint  mode          (UVProjectionMode)
 *   offset  8  uint  write_colour  (0 = UV only, 1 = also write sampled colour)
 *   offset 12  uint  _pad
 *   offset 16  vec3  param_origin
 *   offset 28  float param_scale
 *   offset 32  vec3  param_axis
 *   offset 44  float param_aux     (cylindrical height / triplanar blend)
 *
 * Usage:
 * @code
 * auto uv_proc = std::make_shared<UVFieldProcessor>();
 * uv_proc->set_projection(UVProjectionMode::CYLINDRICAL);
 * uv_proc->set_axis(glm::vec3(0.0F, 1.0F, 0.0F));
 * uv_proc->set_origin(glm::vec3(0.0F));
 * uv_proc->set_scale(1.0F);
 * uv_proc->set_aux(2.0F);              // cylinder height
 * uv_proc->set_texture(vk_image);      // optional: also writes colour
 * chain->add_postprocessor(uv_proc, self);
 * @endcode
 */
class MAYAFLUX_API UVFieldProcessor : public ComputeProcessor {
public:
    UVFieldProcessor();
    ~UVFieldProcessor() override = default;

    // -------------------------------------------------------------------------
    // Projection configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Set projection mode
     * @param mode Projection algorithm applied in the shader
     */
    void set_projection(UVProjectionMode mode);

    /**
     * @brief Set world-space origin of the projection
     * @param origin Anchor point for UV tile (planar/cylindrical/spherical)
     */
    void set_origin(const glm::vec3& origin);

    /**
     * @brief Set projection axis
     * @param axis Normal for planar, rotation axis for cylindrical (normalised)
     */
    void set_axis(const glm::vec3& axis);

    /**
     * @brief Set UV scale
     * @param scale Multiplier applied to UV output (larger = tighter tiling)
     */
    void set_scale(float scale);

    /**
     * @brief Set auxiliary projection parameter
     * @param aux Cylinder height for CYLINDRICAL, blend exponent for TRIPLANAR
     */
    void set_aux(float aux);

    // -------------------------------------------------------------------------
    // Texture source
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a texture to sample colour from at computed UV coordinates
     * @param image  Source VKImage (must be in eShaderReadOnlyOptimal layout)
     * @param config Sampler configuration (default: linear, repeat)
     *
     * When set, the shader also writes sampled colour to vertex colour (offset 12).
     * When not set (or called with nullptr), only UV is written.
     */
    void set_texture(
        std::shared_ptr<Core::VKImage> image,
        const Portal::Graphics::SamplerConfig& config = {});

    /**
     * @brief Remove the texture source
     *
     * Reverts to UV-only mode (colour is not touched).
     */
    void clear_texture();

    [[nodiscard]] std::string_view get_type_name() const { return "UVField"; }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_descriptors_created() override;
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    // -------------------------------------------------------------------------
    // Push constant mirror (kept in sync with shader layout)
    // -------------------------------------------------------------------------
    struct PushConstants {
        uint32_t vertex_count { 0 };
        uint32_t mode { static_cast<uint32_t>(UVProjectionMode::CARTESIAN) };
        uint32_t write_colour { 0 };
        uint32_t _pad { 0 };
        glm::vec3 param_origin { 0.0F };
        float param_scale { 1.0F };
        glm::vec3 param_axis { 0.0F, 0.0F, 1.0F };
        float param_aux { 1.0F };
    };
    static_assert(sizeof(PushConstants) == 48,
        "UVFieldProcessor::PushConstants layout mismatch — update uv_field.comp");

    PushConstants m_pc;
    std::shared_ptr<Core::VKImage> m_texture;
    vk::Sampler m_sampler;
    Portal::Graphics::SamplerConfig m_sampler_config;
};

} // namespace MayaFlux::Buffers
