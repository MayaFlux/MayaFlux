#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Nodes::GpuSync {
class GeometryWriterNode;
}

namespace MayaFlux::Buffers {

/**
 * @class CompositeGeometryProcessor
 * @brief Aggregates multiple geometry nodes with independent topologies
 *
 * Similar to GeometryBindingsProcessor but aggregates all nodes into
 * a single vertex buffer while tracking per-collection offsets and topologies.
 *
 * Each collection can have different primitive topology (LINE_LIST, LINE_STRIP,
 * POINT_LIST, etc.) and will be rendered with separate RenderProcessors.
 *
 * Architecture:
 * - Upload phase: Aggregate all vertices into single buffer (this processor)
 * - Render phase: Multiple RenderProcessors, one per topology
 *
 * Usage:
 *   auto processor = std::make_shared<CompositeGeometryProcessor>();
 *   processor->add_geometry("path", path_node, PrimitiveTopology::LINE_STRIP);
 *   processor->add_geometry("normals", normals_node, PrimitiveTopology::LINE_LIST);
 *
 *   buffer->set_default_processor(processor);
 *
 *   auto collection = processor->get_collection("path");
 *   render->set_vertex_range(collection.vertex_offset, collection.vertex_count);
 */
class MAYAFLUX_API CompositeGeometryProcessor : public VKBufferProcessor {
public:
    struct GeometryCollection {
        std::string name;
        std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> node;
        Portal::Graphics::PrimitiveTopology topology;

        uint32_t vertex_offset;
        uint32_t vertex_count;

        std::optional<Kakshya::VertexLayout> vertex_layout;
    };

    CompositeGeometryProcessor();

    /**
     * @brief Add a geometry collection
     * @param name Unique identifier for this collection
     * @param node GeometryWriterNode to aggregate
     * @param topology Primitive topology for rendering this collection
     */
    void add_geometry(
        const std::string& name,
        const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
        Portal::Graphics::PrimitiveTopology topology);

    /**
     * @brief Remove a geometry collection
     */
    void remove_geometry(const std::string& name);

    /**
     * @brief Get collection metadata
     * @return Optional collection if exists
     */
    [[nodiscard]] std::optional<GeometryCollection> get_collection(const std::string& name) const;

    /**
     * @brief Get all collections (for RenderProcessor creation)
     */
    [[nodiscard]] const std::vector<GeometryCollection>& get_collections() const { return m_collections; }

    /**
     * @brief Get collection count
     */
    [[nodiscard]] size_t get_collection_count() const { return m_collections.size(); }

    /**
     * @brief BufferProcessor interface - aggregates all geometry
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::vector<GeometryCollection> m_collections;
    std::vector<uint8_t> m_staging_aggregate;
};

} // namespace MayaFlux::Buffers
