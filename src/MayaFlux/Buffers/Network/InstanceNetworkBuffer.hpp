#pragma once

#include "InstanceSSBOProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

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
 * Per-slot transforms are packed into an SSBO at binding 1 by InstanceSSBOProcessor.
 * The shader reads gl_InstanceIndex to fetch the mat4 and transform position,
 * normal, and tangent. instance_count on RenderProcessor is set to slot_count()
 * each cycle.
 *
 * Topology is taken from slot 0's node. All slots must carry geometry with the
 * same topology and vertex layout for the single draw call to be correct.
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

    [[nodiscard]] std::shared_ptr<InstanceSSBOProcessor> get_ssbo_processor() const
    {
        return m_ssbo_processor;
    }

private:
    std::shared_ptr<Nodes::Network::InstanceNetwork> m_network;
    std::shared_ptr<InstanceSSBOProcessor> m_ssbo_processor;

    static size_t estimate_vertex_bytes(
        const std::shared_ptr<Nodes::Network::InstanceNetwork>& network,
        float factor);
};

} // namespace MayaFlux::Buffers
