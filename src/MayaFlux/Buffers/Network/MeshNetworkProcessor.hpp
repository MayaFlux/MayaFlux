#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::Buffers {

/**
 * @class MeshNetworkProcessor
 * @brief Uploads concatenated vertex and index data from all MeshNetwork slots.
 *
 * Mirrors MeshProcessor but operates over the full slot list of a MeshNetwork
 * rather than a single MeshBuffer. Each cycle:
 *
 * 1. Checks whether any slot is dirty (slot.dirty || node->needs_gpu_update()).
 *    If nothing is dirty the processor is a no-op.
 * 2. Concatenates vertex bytes from all slots in topological order into a
 *    single aggregate vertex staging buffer.
 * 3. Concatenates index data with each slot's indices rebased by the running
 *    vertex count so all slots share one index buffer and one draw call.
 * 4. Uploads both streams to their respective GPU buffers via staging.
 * 5. Calls set_index_resources() on the vertex VKBuffer to link the index
 *    handle so RenderProcessor can issue an indexed draw.
 * 6. Clears dirty flags on all uploaded slots and their nodes.
 *
 * The attached VKBuffer (Usage::VERTEX) is the combined vertex buffer.
 * A separate Usage::INDEX VKBuffer is owned by this processor and sized
 * to fit the total index count across all slots.
 *
 * Partial-range re-upload (dirty slots only) is deferred. Any dirty slot
 * triggers a full re-concatenation, which is correct and cheap for moderate
 * slot counts.
 */
class MAYAFLUX_API MeshNetworkProcessor : public VKBufferProcessor {
public:
    explicit MeshNetworkProcessor(
        std::shared_ptr<Nodes::Network::MeshNetwork> network);

    ~MeshNetworkProcessor() override = default;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<Nodes::Network::MeshNetwork> m_network;

    std::shared_ptr<VKBuffer> m_gpu_index_buffer;
    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<VKBuffer> m_index_staging;

    /// @brief Aggregate scratch buffers reused each cycle to avoid allocation.
    std::vector<uint8_t> m_vertex_aggregate;
    std::vector<uint32_t> m_index_aggregate;

    void allocate_gpu_buffers(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_combined(const std::shared_ptr<VKBuffer>& vertex_buf);
    void link_index_resources(const std::shared_ptr<VKBuffer>& vertex_buf);

    [[nodiscard]] bool any_slot_dirty() const;
    void clear_slot_dirty_flags();

    [[nodiscard]] size_t total_vertex_bytes() const;
    [[nodiscard]] size_t total_index_count() const;
};

} // namespace MayaFlux::Buffers
