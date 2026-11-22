#pragma once

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "NetworkGeometryProcessor.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class NetworkGeometryBuffer
 * @brief Specialized buffer for geometry from NodeNetwork instances
 *
 * Aggregates geometry from all nodes within a network into a single GPU buffer.
 * Designed for networks like ParticleNetwork (1000+ points in PointCollectionNode), PointCloudNetwork,
 * and other multi-node generative systems.
 *
 * Philosophy:
 * - Networks are collections of MANY nodes with relationships
 * - This buffer aggregates all node geometry → single draw call
 * - Supports dynamic growth as networks evolve
 *
 * Key Differences from GeometryBuffer:
 * - Accepts NodeNetwork (not single GeometryWriterNode)
 * - Aggregates vertices from ALL internal nodes
 * - Handles network-specific processing patterns
 *
 * Usage:
 * ```cpp
 * // Create particle network with 1000 particles
 * auto particles = std::make_shared<ParticleNetwork>(1000);
 * particles->set_topology(Topology::SPATIAL);
 * particles->set_output_mode(OutputMode::GRAPHICS_BIND);
 *
 * // Create buffer that aggregates all 1000 Points in PointCollectionNode inside the ParticleNetwork
 * auto buffer = std::make_shared<NetworkGeometryBuffer>(particles);
 * buffer->setup_processors(ProcessingToken::VISUAL_RATE);
 *
 * // Render all particles in one draw call
 * auto render = std::make_shared<RenderProcessor>(config);
 * render->set_target_window(window);
 * buffer->add_processor(render);
 * ```
 */
class MAYAFLUX_API NetworkGeometryBuffer : public StateFieldBuffer {
public:
    /**
     * @brief Create geometry buffer from network
     * @param network NodeNetwork containing geometry nodes (e.g., ParticleNetwork)
     * @param binding_name Logical name for this geometry binding (default: "network_geometry")
     * @param over_allocate_factor Buffer size multiplier for dynamic growth (default: 2.0x)
     *
     * Buffer size is calculated based on network node count and estimated vertex size.
     * Higher over_allocate_factor recommended for networks that may grow dynamically.
     */
    explicit NetworkGeometryBuffer(
        std::shared_ptr<Nodes::Network::NodeNetwork> network,
        const std::string& binding_name = "network_geometry",
        float over_allocate_factor = 2.0F);

    ~NetworkGeometryBuffer() override = default;

    /**
     * @brief Initialize the buffer and its processors
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Get the network driving this buffer
     */
    [[nodiscard]] std::shared_ptr<Nodes::Network::NodeNetwork> get_network() const
    {
        return m_network;
    }

    /**
     * @brief Get the processor managing uploads
     */
    [[nodiscard]] std::shared_ptr<NetworkGeometryProcessor> get_processor() const
    {
        return m_processor;
    }

    /**
     * @brief Get the logical binding name
     */
    [[nodiscard]] const std::string& get_binding_name() const
    {
        return m_binding_name;
    }

    /**
     * @brief Get current vertex count (aggregated from all network nodes)
     */
    [[nodiscard]] uint32_t get_vertex_count() const;

    /**
     * @brief Trigger network processing
     *
     * Calls network->process_batch() to update physics/state.
     * Geometry aggregation happens automatically in processor.
     */
    void update_network(unsigned int num_samples = 1)
    {
        if (m_network && m_network->is_enabled()) {
            m_network->process_batch(num_samples);
        }
    }

    /**
     * @brief Setup rendering with RenderProcessor
     * @param config Rendering configuration
     *
     * Registers in the main process phase, after everything setup_processors
     * wired, since the chain runs that phase in insertion order and
     * setup_processors must precede this call for the chain to exist at all.
     * Ordering matters more here than for a plain draw: a triangulating
     * processor mills the vertex buffer into a copy at process time rather than
     * recording a binding read at present time, so every pass that writes those
     * vertices has to come first.
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Add a RenderProcessor for a specific operator chain index
     * @param config Rendering configuration
     *
     * This allows rendering different subsets of the network geometry with different pipelines.
     * Each chain index corresponds to a specific node/operator in the network.
     *
     * Each processor added here gets its own spans through
     * update_chain_render_range, so one that triangulates mills only its own
     * operator's slice.
     */
    void add_chain_operator_rendering(const RenderConfig& config);

    /**
     * @brief Get RenderProcessor for a specific operator chain index
     * @param index Operator chain index
     * @return Optional containing RenderProcessor if exists
     *
     * Each chain index corresponds to a specific node/operator in the network.
     */
    [[nodiscard]] std::shared_ptr<RenderProcessor> get_chain_render_processor(size_t index) const;

    /**
     * @brief Update vertex range for a specific operator chain index
     * @param index Operator chain index
     * @param vertex_offset Starting vertex offset for this chain
     * @param vertex_count Number of vertices for this chain
     * @param layout Optional vertex layout for this chain (if different from primary)
     * @param runs This operator's spans, in this buffer's vertex coordinates.
     *        Consumed only by a triangulating processor; empty leaves it to
     *        treat its whole range as one span.
     *
     * This allows the processor to push per-chain vertex ranges to the RenderProcessor,
     * enabling it to issue draw calls for specific subsets of the geometry.
     *
     * Offsets in @p runs must already be rebased off this operator's slice base:
     * GraphicsOperator::topology_runs() reports operator-local offsets, and only
     * whoever concatenated the slices knows where each one landed.
     */
    void update_chain_render_range(
        size_t index,
        uint32_t vertex_offset,
        uint32_t vertex_count,
        const std::optional<Kakshya::VertexLayout>& layout,
        std::vector<Portal::Graphics::DrawRun> runs = {});

    /**
     * @brief Hand the primary processor its spans, without touching its range.
     * @param runs Spans in this buffer's vertex coordinates.
     *
     * For a network with no operator chain, where one producer owns the whole
     * buffer and there is no slice table to rebase against. Inert on a processor
     * that does not triangulate.
     */
    void update_render_runs(std::vector<Portal::Graphics::DrawRun> runs);

    //-------------------------------------------------------------------------
    // Auxiliary state: declare_state, has_state and the read/write/swap
    // calls are StateFieldBuffer's.
    //-------------------------------------------------------------------------

    /**
     * @brief Ensure the hash_cluster_id state field exists, declaring and
     *        populating it from the network's own primary GraphicsOperator
     *        if nothing has already done so.
     * @return True if hash_cluster_id exists by the time this returns
     *         (already did, or was just created); false if there is no
     *         primary GraphicsOperator to size it from, or it reports zero
     *         vertices.
     *
     * Sized to get_vertex_count(), not get_point_count(): a GPU consumer
     * indexes this field in lockstep with the vertex buffer itself, and for
     * an operator whose rendered vertex count differs from its source point
     * count (TopologyOperator/PathOperator after interpolation) those are
     * two different numbers. Coincide for PhysicsOperator's point-sprite
     * geometry, so this changes nothing for the particle path.
     *
     * has_state-guarded, so whichever caller reaches this first (a
     * this buffer's own field-operator wiring, or a VertexFieldProcessor
     * attaching with a cluster-scoped GpuFieldOperator binding) does the
     * real work and the other finds it already done. Values come from
     * GraphicsOperator::build_cluster_ids(): every entry 0 for any operator
     * that has never overridden it, which is every one except PhysicsOperator
     * today, so calling this against a PointCloudNetwork or plain
     * FieldOperator-driven network is safe and simply declares a field
     * whose only value is 0.
     */
    bool ensure_cluster_ids() override;

protected:
    std::shared_ptr<Nodes::Network::NodeNetwork> m_network;
    std::shared_ptr<NetworkGeometryProcessor> m_processor;
    std::string m_binding_name;

private:
    struct ChainRenderEntry {
        std::shared_ptr<RenderProcessor> render_processor;
        uint32_t vertex_offset {};
        uint32_t vertex_count {};
    };
    std::vector<ChainRenderEntry> m_chain_render_processors;

    /** @brief Spans last handed to the primary processor, before any point-mode rewrite. */
    std::vector<Portal::Graphics::DrawRun> m_applied_runs;

    /**
     * @brief Calculate initial buffer size based on network node count
     */
    static size_t calculate_buffer_size(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        float over_allocate_factor);

    /**
     * @brief Find the first GpuFieldOperator in the network's operator chain
     *        and wire its compute stages: a VertexFieldProcessor when it
     *        carries bindings, plus whichever spatial-hash / claim / density /
     *        population stages its SpatialFieldConfig calls for.
     *
     * No-op unless the network is one of the field-compatible types
     * (ParticleNetwork, PointCloudNetwork); mesh and instance buffers never
     * reach the scan. Called at the end of setup_processors, so a scene that
     * puts a GpuFieldOperator in the chain needs no hand-wired
     * VertexFieldProcessor and must not add one itself. Implemented in
     * NetworkGeometryFieldWiring.cpp to keep the compute-processor headers out
     * of the base translation unit.
     */
    void wire_field_operators();
};

} // namespace MayaFlux::Buffers
