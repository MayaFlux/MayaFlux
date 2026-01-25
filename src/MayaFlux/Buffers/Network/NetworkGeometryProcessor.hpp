#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/PointNode.hpp"

namespace MayaFlux::Nodes::Network {
class NodeNetwork;
class ParticleNetwork;
} // namespace MayaFlux::Nodes

namespace MayaFlux::Buffers {

/**
 * @class NetworkGeometryProcessor
 * @brief BufferProcessor that aggregates geometry from NodeNetwork nodes
 *
 * Extracts geometry from all nodes within a network and uploads to GPU as
 * a single vertex buffer. Handles network-specific patterns like ParticleNetwork
 * (many PointNodes) and PointCloudNetwork.
 *
 * Key Differences from GeometryBindingsProcessor:
 * - Operates on NodeNetwork (not single GeometryWriterNode)
 * - Aggregates vertices from ALL internal nodes
 * - Type-aware: special handling for ParticleNetwork, PointCloudNetwork, etc.
 *
 * Behavior:
 * - Extracts ALL node geometry from bound networks
 * - Aggregates into single vertex buffer
 * - Uses staging buffer for device-local targets
 * - Supports multiple network bindings (different networks → different buffers)
 *
 * Usage:
 * ```cpp
 * auto particles = std::make_shared<ParticleNetwork>(1000);
 * auto vertex_buffer = std::make_shared<VKBuffer>(...);
 *
 * auto processor = std::make_shared<NetworkGeometryProcessor>();
 * processor->bind_network("particles", particles, vertex_buffer);
 *
 * vertex_buffer->set_default_processor(processor);
 * vertex_buffer->process_default();  // Aggregates all 1000 PointNodes → GPU
 * ```
 */
class MAYAFLUX_API NetworkGeometryProcessor : public VKBufferProcessor {
public:
    NetworkGeometryProcessor();

    /**
     * @brief Structure representing a network geometry binding
     */
    struct NetworkBinding {
        std::shared_ptr<Nodes::Network::NodeNetwork> network;
        std::shared_ptr<VKBuffer> gpu_vertex_buffer;
        std::shared_ptr<VKBuffer> staging_buffer;
    };

    /**
     * @brief Bind a network to a GPU vertex buffer
     * @param name Logical name for this binding
     * @param network NodeNetwork to aggregate geometry from
     * @param vertex_buffer GPU vertex buffer to upload to
     *
     * If vertex_buffer is device-local, a staging buffer is automatically created.
     */
    void bind_network(
        const std::string& name,
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        const std::shared_ptr<VKBuffer>& vertex_buffer);

    /**
     * @brief Remove a network binding
     * @param name Name of binding to remove
     */
    void unbind_network(const std::string& name);

    /**
     * @brief Check if a binding exists
     * @param name Binding name
     * @return True if binding exists
     */
    [[nodiscard]] bool has_binding(const std::string& name) const;

    /**
     * @brief Get all binding names
     * @return Vector of binding names
     */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

    /**
     * @brief Get number of active bindings
     * @return Binding count
     */
    [[nodiscard]] size_t get_binding_count() const;

    /**
     * @brief Get a specific binding
     * @param name Binding name
     * @return Optional containing binding if exists
     */
    [[nodiscard]] std::optional<NetworkBinding> get_binding(const std::string& name) const;

    /**
     * @brief BufferProcessor interface - aggregates and uploads network geometry
     * @param buffer The buffer this processor is attached to
     *
     * For each bound network:
     * 1. Extract vertices from all internal nodes
     * 2. Aggregate into single vertex array
     * 3. Upload to GPU vertex buffer
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::unordered_map<std::string, NetworkBinding> m_bindings;

    /**
     * @brief Extract vertices from ParticleNetwork
     * @return Vector of aggregated PointVertex data
     */
    std::vector<Nodes::GpuSync::PointVertex> extract_particle_vertices(
        const std::shared_ptr<Nodes::Network::ParticleNetwork>& network);

    /**
     * @brief Extract vertices from generic NodeNetwork (fallback)
     * @return Vector of aggregated vertex data
     *
     * Attempts to cast internal nodes to PointNode and extract geometry.
     * For custom network types, extend this with type-specific logic.
     */
    std::vector<Nodes::GpuSync::PointVertex> extract_network_vertices(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network);
};

} // namespace MayaFlux::Buffers
