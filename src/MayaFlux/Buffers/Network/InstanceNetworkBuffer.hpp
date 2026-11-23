#pragma once

#include "InstanceSSBOProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::Network {
class InstanceNetwork;
}

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class InstanceNetworkBuffer
 * @brief VKBuffer that renders an InstanceNetwork as a single instanced draw call.
 *
 * Template geometry (from slot 0's node) is uploaded once into the vertex buffer.
 * Per-slot transforms are packed into an SSBO at binding 1 by InstanceSSBOProcessor,
 * and per-slot payloads (tint, texture index, free scalar) into one at binding 8.
 * The shader reads gl_InstanceIndex to fetch the mat4 and transform position,
 * normal, and tangent, and the payload to tint the colour. instance_count on
 * RenderProcessor is set to slot_count() each cycle.
 *
 * bind_instance_textures() gives the instances a texture array: each slot's
 * texture index picks the image its instance samples, with instance_textured.frag
 * as the default fragment shader for a triangle template.
 *
 * Topology is taken from slot 0's node. All slots must carry geometry with the
 * same topology and vertex layout for the single draw call to be correct.
 *
 * When the render config triangulates, the template is milled once and the
 * vertex shader applies each instance transform to the milled position, then
 * expands point quads and ribbons toward the viewpoint, so every instance
 * faces the view whatever its rotation. This needs a tangent attribute in the
 * template layout. Ribbon::WorldPlane in the mill spec instead holds the
 * expansion in the template plane and the instance transform rotates it.
 *
 * Usage:
 * @code
 * auto net = std::make_shared<InstanceNetwork>();
 * // ... add slots ...
 *
 * auto buf = std::make_shared<InstanceNetworkBuffer>(net);
 * buf->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API InstanceNetworkBuffer : public VKBuffer {
public:
    explicit InstanceNetworkBuffer(
        std::shared_ptr<Nodes::Network::InstanceNetwork> network,
        float over_allocate_factor = 1.5F);

    ~InstanceNetworkBuffer() override = default;

    void setup_processors(ProcessingToken token) override;
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Give the instances a texture array, one image per GeometrySlot::texture index.
     * @param images Images in index order. Must be set before setup_rendering() for the
     *        pipeline to include the array; a template that is not triangles ignores it.
     *
     * The array occupies descriptor bindings from 2 up, in front of any additional
     * textures, which are bound after it.
     */
    void bind_instance_textures(std::vector<std::shared_ptr<Core::VKImage>> images);

    [[nodiscard]] bool has_instance_textures() const noexcept { return !m_instance_textures.empty(); }

    [[nodiscard]] std::shared_ptr<InstanceSSBOProcessor> get_ssbo_processor() const
    {
        return m_ssbo_processor;
    }

    /** @brief Return the live instance network driving this buffer. */
    [[nodiscard]] std::shared_ptr<Nodes::Network::InstanceNetwork> get_network() const
    {
        return m_network;
    }

private:
    std::shared_ptr<Nodes::Network::InstanceNetwork> m_network;
    std::shared_ptr<InstanceSSBOProcessor> m_ssbo_processor;
    std::vector<std::shared_ptr<Core::VKImage>> m_instance_textures;

    static size_t estimate_vertex_bytes(
        const std::shared_ptr<Nodes::Network::InstanceNetwork>& network,
        float factor);
};

} // namespace MayaFlux::Buffers
