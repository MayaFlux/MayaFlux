#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
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
class MAYAFLUX_API NetworkGeometryBuffer : public VKBuffer {
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
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Add a RenderProcessor for a specific operator chain index
     * @param config Rendering configuration
     *
     * This allows rendering different subsets of the network geometry with different pipelines.
     * Each chain index corresponds to a specific node/operator in the network.
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
     *
     * This allows the processor to push per-chain vertex ranges to the RenderProcessor,
     * enabling it to issue draw calls for specific subsets of the geometry.
     */
    void update_chain_render_range(
        size_t index,
        uint32_t vertex_offset,
        uint32_t vertex_count,
        const std::optional<Kakshya::VertexLayout>& layout);

    //-------------------------------------------------------------------------
    // Auxiliary state
    //-------------------------------------------------------------------------

    /**
     * @brief Declare a named auxiliary state field alongside the vertex data.
     * @param name Lookup key, unique within this buffer.
     * @param element_count Number of elements the field holds. Fixed at
     *        declaration: it does not track later growth of the vertex
     *        buffer itself, since NetworkGeometryProcessor's resize path
     *        (1.5x headroom on overflow) has no knowledge of declared state
     *        fields. Declare at the network's expected particle count.
     * @param stride_bytes Bytes per element.
     * @param double_buffered False resolves read and write to one slot,
     *        which suits a value recomputed from other state each cycle.
     * @return True if the field was registered.
     *
     * Mirrors VolumeGridBuffer::allocate_field: storage lives as raw handle
     * pairs in the base VKBuffer's back_buffers, addressed by name rather
     * than wrapped in a VKBuffer of its own. A processor resolves the
     * handle it needs through read_state_handle/write_state_handle and
     * writes its own descriptor directly against ShaderFoundry, the way
     * VolumeFieldProcessor does for volume fields, rather than going
     * through bind_buffer or the buffer's shared pipeline_context.
     */
    bool declare_state(
        const std::string& name,
        size_t element_count,
        size_t stride_bytes,
        bool double_buffered = true);

    /** @brief Whether a state field of this name was declared. */
    [[nodiscard]] bool has_state(const std::string& name) const;

    /** @brief Byte size of one slot of the named state field, or 0 if undeclared. */
    [[nodiscard]] size_t get_state_bytes(const std::string& name) const;

    /**
     * @brief Handle a stage should read the named state field from.
     * @return Vulkan buffer handle, or nullptr if undeclared.
     */
    [[nodiscard]] vk::Buffer read_state_handle(const std::string& name) const;

    /**
     * @brief Full back_buffers slot a stage should read the named state
     *        field from.
     * @return The slot, or a default-constructed (null-handle) slot if
     *         undeclared.
     *
     * Same resolution as read_state_handle, without narrowing to just the
     * Vulkan handle: a CPU-side readback through StagingUtils::download_back_buffer
     * needs the whole slot (it checks mapped_ptr for the host-visible fast
     * path), not only .buffer. Returned by value since GenerationSlot is
     * three handles wide.
     */
    [[nodiscard]] VKBufferResources::GenerationSlot read_state_slot(const std::string& name) const;

    /**
     * @brief Handle a stage should write the named state field to.
     * @return Vulkan buffer handle, or nullptr if undeclared. Equals
     *         read_state_handle() for single-slot fields.
     */
    [[nodiscard]] vk::Buffer write_state_handle(const std::string& name) const;

    /**
     * @brief Full back_buffers slot a stage should write the named state
     *        field to.
     * @return The slot, or a default-constructed (null-handle) slot if
     *         undeclared. Equals read_state_slot() for single-slot fields.
     *
     * Same resolution as write_state_handle, without narrowing to just the
     * Vulkan handle: a CPU-side upload through StagingUtils::upload_back_buffer
     * needs the whole slot (it checks mapped_ptr for the host-visible fast
     * path), not only .buffer.
     */
    [[nodiscard]] VKBufferResources::GenerationSlot write_state_slot(const std::string& name) const;

    /**
     * @brief Exchange read and write slots for the named state field.
     * @param name Field name. No effect on single-slot fields.
     *
     * Called by whichever stage last wrote the field, after its dispatch,
     * so the next stage reading it observes the new values.
     */
    void swap_state(const std::string& name);

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
    bool ensure_cluster_ids();

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

    struct StateField {
        size_t element_count;
        size_t stride_bytes;
        uint32_t slot_a;
        uint32_t slot_b;
        bool read_is_a;
    };
    std::unordered_map<std::string, StateField> m_state_fields;

    /**
     * @brief Resolve a declared state field by name.
     * @param context Caller identifier used in the error path.
     * @return Pointer to the field, or nullptr with an error logged.
     */
    [[nodiscard]] const StateField* find_state_field(const std::string& name, const char* context) const;

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
