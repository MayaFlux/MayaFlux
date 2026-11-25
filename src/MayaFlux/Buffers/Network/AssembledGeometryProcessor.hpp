#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Nodes::GpuSync {
class GeometryWriterNode;
}

namespace MayaFlux::Nodes::Network {
class AssemblyNetwork;
}

namespace MayaFlux::Buffers {

/**
 * @class AssembledGeometryProcessor
 * @brief Uploads the items of an AssemblyNetwork as one vertex stream and one index stream.
 *
 * Items are concatenated in insertion order. When any triangle item is indexed, every
 * triangle item is drawn through the index stream, its indices offset by the vertices
 * before it and an unindexed one numbered in order. Point and line items are always
 * drawn from their vertices. Uploads when an item changes, is added or removed, and
 * every cycle while an operator demands the vertices be restored.
 */
class MAYAFLUX_API AssembledGeometryProcessor : public VKBufferProcessor {
public:
    explicit AssembledGeometryProcessor(
        const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network);

    ~AssembledGeometryProcessor() override = default;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<Nodes::Network::AssemblyNetwork> m_network;

    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<VKBuffer> m_index_buffer;
    std::shared_ptr<VKBuffer> m_index_staging;

    std::vector<uint8_t> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Portal::Graphics::DrawRun> m_runs;
    std::vector<std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>> m_uploaded;

    [[nodiscard]] bool items_changed() const;
    [[nodiscard]] bool demands_vertex_restore() const;

    void aggregate();
    void upload(const std::shared_ptr<VKBuffer>& buffer);
    void upload_index_stream(const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
