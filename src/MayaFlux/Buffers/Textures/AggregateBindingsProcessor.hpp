#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @class AggregateBindingsProcessor
 * @brief BufferProcessor that aggregates multiple node outputs into GPU buffers
 *
 * Collects outputs from multiple nodes and uploads them as contiguous arrays
 * to GPU buffers. Supports multiple independent target buffers, each receiving
 * aggregated data from a subset of nodes.
 *
 * Behavior:
 * - Uploads ALL registered aggregates to their target buffers
 * - If attached buffer is one of the targets, it receives its aggregate
 * - If attached buffer is NOT a target, it receives the first aggregate
 *
 * Usage:
 *   auto velocity_buffer = std::make_shared<VKBuffer>(1000 * sizeof(float), ...);
 *   auto aggregate = std::make_shared<AggregateBindingsProcessor>();
 *
 *   // Add nodes to "velocities" aggregate
 *   for (int i = 0; i < 1000; i++) {
 *       aggregate->add_node("velocities", velocity_node, velocity_buffer);
 *   }
 *
 *   velocity_buffer->set_default_processor(aggregate);
 *   velocity_buffer->process_default();  // Uploads all aggregates
 */
class MAYAFLUX_API AggregateBindingsProcessor : public VKBufferProcessor {
public:
    struct AggregateBinding {
        std::vector<std::shared_ptr<Nodes::Node>> nodes;
        std::shared_ptr<VKBuffer> target_buffer;
        std::vector<float> staging_data;
    };

    /**
     * @brief Default constructor
     */
    AggregateBindingsProcessor() = default;

    /**
     * @brief Add a node to a named aggregate
     * @param aggregate_name Name of the aggregate group
     * @param node Node to add
     * @param target Target buffer for this aggregate
     *
     * Nodes with the same aggregate_name are grouped together and uploaded
     * to the same target buffer. Nodes are ordered by insertion.
     */
    void add_node(
        const std::string& aggregate_name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::shared_ptr<VKBuffer>& target);

    /**
     * @brief Remove a node from an aggregate
     * @param aggregate_name Name of the aggregate group
     * @param node Node to remove
     */
    void remove_node(
        const std::string& aggregate_name,
        const std::shared_ptr<Nodes::Node>& node);

    /**
     * @brief Clear all nodes from an aggregate
     * @param aggregate_name Name of the aggregate group
     */
    void clear_aggregate(const std::string& aggregate_name);

    /**
     * @brief Clear all aggregates
     */
    void clear_all_aggregates();

    /**
     * @brief Get number of nodes in an aggregate
     * @param aggregate_name Name of the aggregate group
     * @return Node count (0 if aggregate doesn't exist)
     */
    [[nodiscard]] size_t get_node_count(const std::string& aggregate_name) const;

    /**
     * @brief Get total number of nodes across all aggregates
     * @return Total node count
     */
    [[nodiscard]] size_t get_total_node_count() const;

    /**
     * @brief Get all aggregate names
     * @return Vector of aggregate names
     */
    [[nodiscard]] std::vector<std::string> get_aggregate_names() const;

    /**
     * @brief Get number of aggregates
     * @return Aggregate count
     */
    [[nodiscard]] size_t get_aggregate_count() const;

    /**
     * @brief BufferProcessor interface - uploads all aggregates
     * @param buffer The buffer this processor is attached to
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

private:
    std::unordered_map<std::string, AggregateBinding> m_aggregates;
};

} // namespace MayaFlux::Buffers
