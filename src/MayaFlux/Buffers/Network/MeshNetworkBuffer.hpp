#pragma once

#include "MeshNetworkProcessor.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class MeshNetworkBuffer
 * @brief VKBuffer that renders a MeshNetwork as a single indexed draw call.
 *
 * Analogous to MeshBuffer but driven by a MeshNetwork rather than a single
 * MeshData. All slots are concatenated by MeshNetworkProcessor into one
 * combined vertex buffer and one combined index buffer, with per-slot index
 * rebasing so the topology is correct across slot boundaries.
 *
 * Texture support follows MeshBuffer's model exactly:
 * - bind_diffuse_texture() binds one shared diffuse texture for all slots.
 * - RenderConfig::additional_textures carries any further bindings.
 * - Per-slot distinct textures (texture array path) are deferred.
 *
 * Usage:
 * @code
 * auto net = std::make_shared<MeshNetwork>();
 * // ... add slots ...
 *
 * auto buf = std::make_shared<MeshNetworkBuffer>(net);
 * buf->bind_diffuse_texture(image);
 * buf->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API MeshNetworkBuffer : public VKBuffer {
public:
    /**
     * @brief Construct from an existing MeshNetwork.
     * @param network Must be non-null and have at least one slot before
     *                setup_processors() is called.
     * @param over_allocate_factor Initial buffer size multiplier. Applied to
     *                             the estimated total vertex byte count.
     */
    explicit MeshNetworkBuffer(
        std::shared_ptr<Nodes::Network::MeshNetwork> network,
        float over_allocate_factor = 1.5F);

    ~MeshNetworkBuffer() override = default;

    /**
     * @brief Create and attach MeshNetworkProcessor as the default processor.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor with optional texture support.
     *
     * Mirrors MeshBuffer::setup_rendering:
     * - Topology is always TRIANGLE_LIST.
     * - Defaults to triangle.vert.spv / triangle.frag.spv (untextured)
     *   or mesh_textured.frag.spv when a diffuse texture is bound.
     * - RenderConfig::additional_textures registers extra texture slots.
     */
    void setup_rendering(const RenderConfig& config);

    // -------------------------------------------------------------------------
    // Texture binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a single shared diffuse texture for all slots.
     * @param image       GPU image to bind. Pass nullptr to unbind.
     * @param binding_name Shader descriptor name (default: "diffuseTex").
     *
     * Must be called before setup_rendering() for the ShaderConfig to include
     * the descriptor slot. Calling after setup_rendering() updates the binding
     * on the existing RenderProcessor but will warn if the pipeline was
     * compiled without the binding.
     */
    void bind_diffuse_texture(
        std::shared_ptr<Core::VKImage> image,
        std::string_view binding_name = "diffuseTex");

    [[nodiscard]] bool has_diffuse_texture() const noexcept
    {
        return m_diffuse_texture != nullptr;
    }

    [[nodiscard]] std::shared_ptr<Core::VKImage> get_diffuse_texture() const noexcept
    {
        return m_diffuse_texture;
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    [[nodiscard]] std::shared_ptr<Nodes::Network::MeshNetwork> get_network() const
    {
        return m_network;
    }

    [[nodiscard]] std::shared_ptr<MeshNetworkProcessor> get_mesh_processor() const
    {
        return m_processor;
    }

    [[nodiscard]] std::shared_ptr<RenderProcessor> get_render_processor() const override
    {
        return m_render_processor;
    }

private:
    std::shared_ptr<Nodes::Network::MeshNetwork> m_network;
    std::shared_ptr<MeshNetworkProcessor> m_processor;
    std::shared_ptr<RenderProcessor> m_render_processor;

    std::shared_ptr<Core::VKImage> m_diffuse_texture;
    std::string m_diffuse_binding { "diffuseTex" };

    RenderConfig m_render_config;

    static size_t estimate_vertex_bytes(
        const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
