#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/SlotMachine.hpp"

namespace MayaFlux::Nodes::Network {
class InstanceNetwork;
}

namespace MayaFlux::Buffers {

class InstaneceNetworkBuffer;

/**
 * @struct InstancePayload
 * @brief One instance's payload as the shaders read it: a std430 struct of 32 bytes.
 *
 * Mirrors GeometrySlot::tint, scalar and texture. The padding keeps the struct at the
 * 16 byte alignment a vec3 member gives it in std430.
 */
struct InstancePayload {
    glm::vec3 tint { 1.0F };
    float scalar { 1.0F };
    uint32_t texture {};
    uint32_t joint_count {}; ///< Matrices of this instance in the palettes, at instance * joint_count.
    uint32_t target_count {}; ///< Weights of this instance, at instance * target_count.
    uint32_t vertex_count {}; ///< Template vertices the influences and morph differences cover.
};

static_assert(sizeof(InstancePayload) == 32);

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
 * Per-instance SSBOs are written every cycle that any slot is dirty.
 * RenderProcessor instance_count is set to network->slot_count() after
 * each write.
 *
 * SSBO layout (one entry per slot, tightly packed):
 *   binding k_transform_ssbo_binding - mat4 transform[slot_count]
 *   binding k_payload_ssbo_binding   - InstancePayload payload[slot_count]
 *
 *   binding k_influence_ssbo_binding - joints and weights of each template vertex
 *   binding k_delta_ssbo_binding     - what each morph target adds, per template vertex
 *   binding k_palette_ssbo_binding   - mat4 palette[slot_count * joint_count]
 *   binding k_weight_ssbo_binding    - float morph_weight[slot_count * target_count]
 *
 * The last four exist for a template with a skin or morph targets (see GeometrySlot).
 *
 * The shader reads gl_InstanceIndex to index into the per instance arrays.
 */
class MAYAFLUX_API InstanceSSBOProcessor : public VKBufferProcessor {
public:
    explicit InstanceSSBOProcessor(
        std::shared_ptr<Nodes::Network::InstanceNetwork> network);

    ~InstanceSSBOProcessor() override = default;

    /**
     * @brief Buffer holding the transform slots, which changes if the arena grows
     *
     * The live transforms sit at the offset of the slot last pushed, which can
     * move whenever transforms are rewritten while a frame is still reading.
     */
    [[nodiscard]] std::shared_ptr<VKBuffer> get_transform_ssbo() const
    {
        return m_transform_arena ? m_transform_arena->buffer() : nullptr;
    }

    /**
     * @brief Buffer holding the payload slots, which changes if the arena grows
     *
     * The live payload sits at the offset of the slot last pushed, as with the transforms.
     */
    [[nodiscard]] std::shared_ptr<VKBuffer> get_payload_ssbo() const
    {
        return m_payload_arena ? m_payload_arena->buffer() : nullptr;
    }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<Nodes::Network::InstanceNetwork> m_network;

    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<Portal::Graphics::SlotArena> m_transform_arena;
    size_t m_transform_slot {};

    std::shared_ptr<Portal::Graphics::SlotArena> m_payload_arena;
    size_t m_payload_slot {};
    std::vector<InstancePayload> m_payload_scratch;
    std::shared_ptr<VKBuffer> m_index_buffer;
    std::shared_ptr<VKBuffer> m_index_staging;

    std::vector<glm::mat4> m_transform_scratch;

    struct Stream {
        std::shared_ptr<Portal::Graphics::SlotArena> arena;
        size_t slot {};
    };
    Stream m_influences;
    Stream m_deltas;
    Stream m_palettes;
    Stream m_weights;
    uint32_t m_joint_count {};
    uint32_t m_target_count {};
    uint32_t m_deformed_vertices {};
    bool m_deformation_fits {};

    void upload_deformation_template();
    void upload_deformation();
    static void write_stream(Stream& stream, const void* data, size_t bytes);

    void upload_template(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_indices(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_transforms(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_payload();
    void push_ssbo_binding(const std::shared_ptr<VKBuffer>& vertex_buf);

    [[nodiscard]] bool any_slot_dirty() const;
    void clear_dirty_flags();

    static constexpr uint32_t k_transform_ssbo_binding = 1;
    static constexpr uint32_t k_payload_ssbo_binding = 8;
    static constexpr uint32_t k_influence_ssbo_binding = 9;
    static constexpr uint32_t k_delta_ssbo_binding = 10;
    static constexpr uint32_t k_palette_ssbo_binding = 11;
    static constexpr uint32_t k_weight_ssbo_binding = 12;

    friend class InstanceNetworkBuffer;
};

} // namespace MayaFlux::Buffers
