#pragma once

#include "CompositeGeometryProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

struct RenderData {
    std::shared_ptr<RenderProcessor> render_processor;
    uint32_t vertex_offset;
    uint32_t vertex_count;
};

/**
 * @class CompositeGeometryBuffer
 * @brief Buffer for aggregating multiple geometry nodes with independent topologies
 *
 * Allows manual composition of multiple GeometryWriterNodes, each rendered with
 * its own primitive topology. All geometry is aggregated into a single GPU buffer
 * for efficient upload, but rendered with separate draw calls per topology.
 *
 * Philosophy:
 * - Manual composition for full control
 * - Each geometry can have different topology (LINE_STRIP, LINE_LIST, POINT_LIST, etc.)
 * - Single buffer upload, multiple render passes
 * - Efficient batching without topology constraints
 *
 * Key Differences from GeometryBuffer:
 * - Accepts multiple nodes (not single GeometryWriterNode)
 * - Each node can have different topology
 * - Automatically creates multiple RenderProcessors
 *
 * Key Differences from NetworkGeometryBuffer:
 * - Manual node registration (not NodeNetwork-driven)
 * - Explicit topology specification per node
 * - Not tied to network operators or topology inference
 *
 * Usage:
 * ```cpp
 * auto path_node = vega.PathGeneratorNode(...);
 * auto normals_node = vega.PointCollectionNode();
 * // ... populate normals
 *
 * auto composite = std::make_shared<CompositeGeometryBuffer>();
 * composite->add_geometry("path", path_node, PrimitiveTopology::LINE_STRIP);
 * composite->add_geometry("normals", normals_node, PrimitiveTopology::LINE_LIST);
 * composite->add_geometry("control_points", points_node, PrimitiveTopology::POINT_LIST);
 *
 * composite->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * composite->setup_rendering({.target_window = window});
 * ```
 *
 * Each frame:
 * 1. CompositeGeometryProcessor aggregates all nodes → single GPU upload
 * 2. Multiple RenderProcessors draw subsets with different topologies
 */
class MAYAFLUX_API CompositeGeometryBuffer : public VKBuffer {
public:
    /**
     * @brief Create empty composite buffer
     * @param initial_capacity Initial buffer size in bytes (default: 1MB)
     * @param over_allocate_factor Growth multiplier for dynamic resizing (default: 1.5x)
     */
    explicit CompositeGeometryBuffer(
        size_t initial_capacity = 1024 * 1024,
        float over_allocate_factor = 1.5F);

    ~CompositeGeometryBuffer() override = default;

    /**
     * @brief Add a geometry collection
     * @param name Unique identifier for this geometry
     * @param node GeometryWriterNode to render
     * @param topology Primitive topology for this geometry
     * @param target_window Window to render to (used for shader config)
     *
     * The node's vertex data will be aggregated with other geometries during upload,
     * but rendered independently with the specified topology.
     */
    void add_geometry(
        const std::string& name,
        const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
        Portal::Graphics::PrimitiveTopology topology,
        const std::shared_ptr<Core::Window>& target_window);

    /**
     * @brief Add a geometry collection with explicit render config
     * @param name Unique identifier for this geometry
     * @param node GeometryWriterNode to render
     * @param topology Primitive topology for this geometry
     * @param config Render configuration (shaders, render states)
     */
    void add_geometry(
        const std::string& name,
        const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
        Portal::Graphics::PrimitiveTopology topology,
        const RenderConfig& config);

    /**
     * @brief Remove a geometry collection
     * @param name Geometry identifier
     */
    void remove_geometry(const std::string& name);

    /**
     * @brief Get geometry collection metadata
     * @param name Geometry identifier
     * @return Optional collection if exists
     */
    [[nodiscard]] std::optional<CompositeGeometryProcessor::GeometryCollection>
    get_collection(const std::string& name) const;

    /**
     * @brief Get number of geometry collections
     */
    [[nodiscard]] size_t get_collection_count() const;

    /**
     * @brief Initialize buffer processors
     *
     * Creates CompositeGeometryProcessor as default processor.
     * Must be called before setup_rendering().
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Setup rendering (DEPRECATED for CompositeGeometryBuffer)
     *
     * For CompositeGeometryBuffer, use add_geometry() with RenderConfig instead.
     * This method exists for interface compatibility but does nothing.
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Get the composite processor managing uploads
     */
    [[nodiscard]] std::shared_ptr<CompositeGeometryProcessor> get_composite_processor() const
    {
        return m_processor;
    }

    /**
     * @brief Get all render processors (one per collection)
     */
    [[nodiscard]] std::vector<std::shared_ptr<RenderProcessor>> get_render_processors() const
    {
        std::vector<std::shared_ptr<RenderProcessor>> renders;
        renders.reserve(m_render_data.size());
        for (const auto& [_, data] : m_render_data) {
            renders.push_back(data.render_processor);
        }
        return renders;
    }

    /**
     * @brief Update the vertex range for a specific geometry collection's render processor
     * @param name Geometry identifier
     * @param vertex_offset Starting vertex offset in the buffer
     * @param vertex_count Number of vertices to render
     *
     * This should be called after processing to ensure each RenderProcessor draws the correct subset.
     */
    void update_collection_render_range(
        const std::string& name,
        uint32_t vertex_offset,
        uint32_t vertex_count);

    /**
     * @brief Push a topology-specific vertex layout to the matching RenderProcessor
     * @param name Geometry identifier
     * @param layout VertexLayout belonging exclusively to this collection
     *
     * Must be called after update_collection_render_range() so the
     * RenderProcessor compiles its Vulkan pipeline with the correct
     * vertex-input stride and attribute offsets for this topology.
     */
    void update_collection_vertex_layout(
        const std::string& name,
        const Kakshya::VertexLayout& layout);

private:
    std::shared_ptr<CompositeGeometryProcessor> m_processor;
    std::unordered_map<std::string, RenderData> m_render_data;
    float m_over_allocate_factor;

    /**
     * @brief Calculate initial buffer size
     */
    static size_t calculate_initial_size(size_t requested_capacity);
};

} // namespace MayaFlux::Buffers
