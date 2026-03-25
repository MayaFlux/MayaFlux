#pragma once

#include "MayaFlux/Buffers/Shaders/UVFieldProcessor.hpp"
#include "NetworkGeometryBuffer.hpp"

namespace MayaFlux::Buffers {
class UVFieldProcessor;
}

namespace MayaFlux::Buffers {

/**
 * @class NetworkTextureBuffer
 * @brief NetworkGeometryBuffer extended with a GPU UV projection and texture sampling pass
 *
 * Inherits everything from NetworkGeometryBuffer and inserts a UVFieldProcessor
 * as a postprocessor between geometry upload and rendering.
 *
 * Processing chain order per frame:
 *   1. Default processor  — NetworkGeometryProcessor (inherited, uploads vertices)
 *   2. Postprocessor      — UVFieldProcessor: writes UV at offset 28, optionally
 *                           writes texture-sampled colour at offset 12
 *   3. Final processor    — RenderProcessor (inherited via setup_rendering)
 *
 * Usage is identical to NetworkGeometryBuffer plus UV configuration:
 * @code
 * auto tex = ImageReader::load_texture("noise.png");
 * auto buffer = std::make_shared<NetworkTextureBuffer>(particles, tex);
 * buffer->get_uv_processor()->set_projection(UVProjectionMode::CYLINDRICAL);
 * buffer->get_uv_processor()->set_axis(glm::vec3(0.0F, 1.0F, 0.0F));
 * buffer->get_uv_processor()->set_aux(2.0F);
 * buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buffer->setup_rendering({ .target_window = window,
 *                           .topology = PrimitiveTopology::POINT_LIST });
 * @endcode
 */
class MAYAFLUX_API NetworkTextureBuffer : public NetworkGeometryBuffer {
public:
    /**
     * @brief Create buffer from network with optional texture source
     * @param network          NodeNetwork providing geometry each frame
     * @param texture          Optional VKImage sampled in the UV compute pass
     * @param binding_name     Logical name for the geometry binding
     * @param over_allocate_factor Buffer size multiplier for dynamic growth
     */
    explicit NetworkTextureBuffer(
        std::shared_ptr<Nodes::Network::NodeNetwork> network,
        std::shared_ptr<Core::VKImage> texture = nullptr,
        const std::string& binding_name = "network_texture_geometry",
        float over_allocate_factor = 2.0F);

    ~NetworkTextureBuffer() override = default;

    /**
     * @brief Wire processors into the chain
     *
     * Calls NetworkGeometryBuffer::setup_processors, then adds
     * UVFieldProcessor as a postprocessor.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Get the UV compute processor for configuration
     */
    [[nodiscard]] std::shared_ptr<UVFieldProcessor> get_uv_processor() const
    {
        return m_uv_processor;
    }

    /**
     * @brief Replace the texture source at runtime
     * @param image New VKImage (nullptr reverts to UV-only)
     * @param config Sampler configuration
     */
    void set_texture(
        std::shared_ptr<Core::VKImage> image,
        const Portal::Graphics::SamplerConfig& config = {});

private:
    std::shared_ptr<UVFieldProcessor> m_uv_processor;
};

} // namespace MayaFlux::Buffers
