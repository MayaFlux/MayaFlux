#pragma once

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "NetworkGeometryProcessor.hpp"

namespace MayaFlux::Buffers {
class UVFieldProcessor;
}

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class NetworkTextureBuffer
 * @brief Geometry buffer that adds a GPU UV projection and texture sampling pass
 *
 * Extends the NetworkGeometryBuffer pattern with a UVFieldProcessor compute
 * pass inserted between geometry upload and rendering.
 *
 * Processing chain order per frame (via process_complete):
 *   1. Default processor  — NetworkGeometryProcessor uploads vertex data to GPU
 *   2. Postprocessor      — UVFieldProcessor dispatches uv_field.comp:
 *                             writes UV at vertex offset 28, optionally writes
 *                             texture-sampled colour at offset 12
 *   3. Final processor    — RenderProcessor draws the geometry
 *
 * The texture source is optional. Without one the buffer behaves identically
 * to NetworkGeometryBuffer except that UV coordinates are computed on the GPU
 * each frame via the chosen projection mode.
 *
 * Usage:
 * @code
 * auto particles = std::make_shared<ParticleNetwork>(1000);
 *
 * auto noise_tex = ...; // any VKImage in eShaderReadOnlyOptimal layout
 *
 * auto buffer = std::make_shared<NetworkTextureBuffer>(particles, noise_tex);
 * buffer->get_uv_processor()->set_projection(UVProjectionMode::CYLINDRICAL);
 * buffer->get_uv_processor()->set_axis(glm::vec3(0.0F, 1.0F, 0.0F));
 * buffer->get_uv_processor()->set_aux(2.0F);
 * buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buffer->setup_rendering({ .target_window = window,
 *                           .topology = PrimitiveTopology::POINT_LIST });
 * @endcode
 *
 * Without texture (UV-only):
 * @code
 * auto buffer = std::make_shared<NetworkTextureBuffer>(particles);
 * buffer->get_uv_processor()->set_projection(UVProjectionMode::SPHERICAL);
 * buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buffer->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API NetworkTextureBuffer : public VKBuffer {
public:
    /**
     * @brief Create buffer from network with optional texture source
     * @param network          NodeNetwork providing geometry each frame
     * @param texture          Optional VKImage sampled in the UV compute pass
     * @param binding_name     Logical name for the geometry binding
     * @param over_allocate_factor Buffer size multiplier for dynamic growth
     *
     * The UVFieldProcessor is always created. Call get_uv_processor() to
     * configure projection mode, axis, scale, and aux parameters before
     * calling setup_processors().
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
     * Creates NetworkGeometryProcessor as default processor, adds
     * UVFieldProcessor as postprocessor.  Must be called before
     * setup_rendering().
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Configure rendering
     * @param config Render configuration (window, topology, shaders, etc.)
     *
     * Topology-driven shader defaults match NetworkGeometryBuffer exactly.
     * Adds RenderProcessor as final processor.
     */
    void setup_rendering(const RenderConfig& config);

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /**
     * @brief Get the UV compute processor
     *
     * Configure projection mode, axis, scale, aux, and texture source
     * through this handle before calling setup_processors().
     */
    [[nodiscard]] std::shared_ptr<UVFieldProcessor> get_uv_processor() const
    {
        return m_uv_processor;
    }

    [[nodiscard]] std::shared_ptr<Nodes::Network::NodeNetwork> get_network() const
    {
        return m_network;
    }

    [[nodiscard]] std::shared_ptr<NetworkGeometryProcessor> get_geometry_processor() const
    {
        return m_geometry_processor;
    }

    [[nodiscard]] std::shared_ptr<RenderProcessor> get_render_processor() const
    {
        return m_render_processor;
    }

    [[nodiscard]] const std::string& get_binding_name() const
    {
        return m_binding_name;
    }

    /**
     * @brief Get current vertex count from network operator
     */
    [[nodiscard]] uint32_t get_vertex_count() const;

    /**
     * @brief Replace the texture source at runtime
     * @param image New VKImage (nullptr clears, reverts to UV-only)
     * @param config Sampler configuration for the new texture
     */
    void set_texture(
        std::shared_ptr<Core::VKImage> image,
        const Portal::Graphics::SamplerConfig& config = {});

    /**
     * @brief Trigger network processing
     */
    void update_network(unsigned int num_samples = 1)
    {
        if (m_network && m_network->is_enabled())
            m_network->process_batch(num_samples);
    }

private:
    std::shared_ptr<Nodes::Network::NodeNetwork> m_network;
    std::shared_ptr<NetworkGeometryProcessor> m_geometry_processor;
    std::shared_ptr<UVFieldProcessor> m_uv_processor;
    std::shared_ptr<RenderProcessor> m_render_processor;
    std::string m_binding_name;

    static size_t calculate_buffer_size(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
