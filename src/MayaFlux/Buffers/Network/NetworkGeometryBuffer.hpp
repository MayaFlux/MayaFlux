#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "NetworkGeometryProcessor.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class NetworkGeometryBuffer
 * @brief Specialized buffer for geometry from NodeNetwork instances
 *
 * Aggregates geometry from all nodes within a network into a single GPU buffer.
 * Designed for networks like ParticleNetwork (1000+ PointNodes), PointCloudNetwork,
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
 * // Create buffer that aggregates all 1000 PointNodes
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
    struct RenderConfig {
        std::shared_ptr<Core::Window> target_window;
        std::string vertex_shader = "point.vert.spv";
        std::string fragment_shader = "point.frag.spv";
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST;
        Portal::Graphics::PolygonMode polygon_mode = Portal::Graphics::PolygonMode::FILL;
        Portal::Graphics::CullMode cull_mode = Portal::Graphics::CullMode::NONE;
    };
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

    std::shared_ptr<RenderProcessor> get_render_processor() const
    {
        return m_render_processor;
    }

private:
    std::shared_ptr<Nodes::Network::NodeNetwork> m_network;
    std::shared_ptr<NetworkGeometryProcessor> m_processor;
    std::string m_binding_name;

    std::shared_ptr<RenderProcessor> m_render_processor;

    /**
     * @brief Calculate initial buffer size based on network node count
     */
    static size_t calculate_buffer_size(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
