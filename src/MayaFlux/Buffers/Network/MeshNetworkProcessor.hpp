#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/SlotMachine.hpp"

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::Nodes::GpuSync {
class MeshWriterNode;
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
 *    If nothing is dirty the processor is a no-op. slot.dirty only says a slot
 *    should be looked at; it does not by itself ask for a geometry upload.
 * 2. Decides what changed. The geometry is uploaded in full when a node's
 *    geometry changed (node->needs_gpu_update()), when the nodes that supply
 *    geometry are no longer the ones last uploaded (a slot added, removed,
 *    reordered or given another node), or when the vertex layout changed
 *    (set_milling_slot_attribute). It is also done every cycle while an
 *    operator of the network demands the vertices be restored (a
 *    GpuFieldOperator with a bound field, which displaces them in place on the
 *    GPU). Otherwise only transforms can have changed.
 * 3. Full upload: concatenates vertex bytes from all slots in topological order,
 *    concatenates index data with each slot's indices rebased by the running
 *    vertex count so all slots share one index buffer and one draw call,
 *    uploads both streams through staging, links the index handle with
 *    set_index_resources() so RenderProcessor can issue an indexed draw, and
 *    rewrites the model matrices and per vertex slot indices.
 * 4. Transform only: composes the world matrices of the geometry slots, and
 *    when they differ from the matrices last uploaded writes just those into a
 *    free slot of the model arena and republishes the bindings. Vertex, index
 *    and slot index data are not touched, and nothing is written when the
 *    matrices are the same.
 * 5. Clears dirty flags on all slots and their nodes.
 *
 * The attached VKBuffer (Usage::VERTEX) is the combined vertex buffer.
 * A separate Usage::INDEX VKBuffer is owned by this processor and sized
 * to fit the total index count across all slots.
 */
class MAYAFLUX_API MeshNetworkProcessor : public VKBufferProcessor {
public:
    explicit MeshNetworkProcessor(
        std::shared_ptr<Nodes::Network::MeshNetwork> network);

    ~MeshNetworkProcessor() override = default;

    void set_milling_slot_attribute(bool enabled);

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

    /// @brief Per-slot world matrices in sorted_indices order. One mat4 per slot.
    std::shared_ptr<Portal::Graphics::SlotArena> m_model_arena;
    size_t m_model_slot {};
    /// @brief Per-vertex slot index. One uint32_t per concatenated vertex.
    std::shared_ptr<Portal::Graphics::SlotArena> m_slot_index_arena;
    size_t m_slot_index_slot {};

    /// @brief CPU-side scratch for model matrices, reused each cycle.
    std::vector<glm::mat4> m_model_aggregate;
    /// @brief CPU-side scratch for per-vertex slot indices, reused each cycle.
    std::vector<uint32_t> m_slot_index_aggregate;
    bool m_milling_slot_attribute {};

    /// @brief Geometry nodes of the last full upload, in sorted slot order.
    std::vector<std::shared_ptr<Nodes::GpuSync::MeshWriterNode>> m_uploaded_nodes;

    /// @brief CPU-side scratch for the matrices a transform only cycle composes.
    std::vector<glm::mat4> m_model_scratch;

    /// @brief Set when the next cycle must upload the geometry in full.
    bool m_force_full_upload { true };

    void allocate_gpu_buffers(const std::shared_ptr<VKBuffer>& vertex_buf);
    void upload_combined(const std::shared_ptr<VKBuffer>& vertex_buf);
    void link_index_resources(const std::shared_ptr<VKBuffer>& vertex_buf);

    [[nodiscard]] bool any_slot_dirty() const;
    [[nodiscard]] bool needs_full_upload() const;
    [[nodiscard]] bool demands_vertex_restore() const;
    void collect_model_matrices(std::vector<glm::mat4>& out) const;
    void upload_model_matrices();
    void clear_slot_dirty_flags();

    [[nodiscard]] size_t total_vertex_bytes() const;
    [[nodiscard]] size_t total_index_count() const;

    void allocate_ssbo_buffers();
    void upload_ssbos();
    void push_ssbo_bindings(const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
