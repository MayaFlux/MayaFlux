#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Nodes::Network {
class InstanceNetwork;
}

namespace MayaFlux::Buffers {

class InstaneceNetworkBuffer;

/**
 * @class InstanceSSBOProcessor
 * @brief Uploads template geometry once and packs per-slot mat4 transforms
 *        into an SSBO each frame for a single instanced draw call.
 *
 * Template vertex data is taken from slot 0's node. It is uploaded on
 * on_attach and on any cycle where slot 0 reports needs_gpu_update(). If
 * all slots share the same node this covers the shared template. If slots
 * carry distinct nodes the vertex buffer holds slot 0's geometry only;
 * callers are responsible for ensuring node topology is consistent across slots.
 *
 * Per-instance SSBO is written every cycle that any slot is dirty.
 * RenderProcessor instance_count is set to network->slot_count() after
 * each SSBO write.
 *
 * SSBO layout (one entry per slot, tightly packed):
 *   binding k_transform_ssbo_binding - mat4 transform[slot_count]
 *
 * The shader reads gl_InstanceIndex to index into the transform array.
 */
class MAYAFLUX_API InstanceSSBOProcessor : public VKBufferProcessor {
public:
    explicit InstanceSSBOProcessor(
        std::shared_ptr<Nodes::Network::InstanceNetwork> network);

    ~InstanceSSBOProcessor() override = default;

    [[nodiscard]] std::shared_ptr<VKBuffer> get_transform_ssbo() const
    {
        return m_transform_ssbo;
    }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<Nodes::Network::InstanceNetwork> m_network;

    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<VKBuffer> m_transform_ssbo;
    std::shared_ptr<VKBuffer> m_transform_staging;

    std::vector<glm::mat4> m_transform_scratch;

    void upload_template(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_transforms(const std::shared_ptr<VKBuffer>& vertex_buf);
    void push_ssbo_binding(const std::shared_ptr<VKBuffer>& vertex_buf);

    [[nodiscard]] bool any_slot_dirty() const;
    void clear_dirty_flags();

    static constexpr uint32_t k_transform_ssbo_binding = 1;

    friend class InstanceNetworkBuffer;
};

} // namespace MayaFlux::Buffers
