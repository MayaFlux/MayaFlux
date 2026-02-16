#include "AggregateBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {

void AggregateBindingsProcessor::add_node(
    const std::string& aggregate_name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::shared_ptr<VKBuffer>& target)
{
    if (!node) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to add null node to aggregate '{}'", aggregate_name);
        return;
    }

    if (!target) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot add node to aggregate '{}' with null target buffer", aggregate_name);
    }

    auto& aggregate = m_aggregates[aggregate_name];

    if (aggregate.nodes.empty()) {
        aggregate.target_buffer = target;
    } else if (aggregate.target_buffer != target) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Aggregate '{}' already has a different target buffer. Ignoring new target.",
            aggregate_name);
    }

    aggregate.nodes.push_back(node);
    aggregate.staging_data.resize(aggregate.nodes.size());

    size_t required_size = aggregate.nodes.size() * sizeof(float);
    if (aggregate.target_buffer->get_size_bytes() < required_size) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target buffer for aggregate '{}' may be too small: {} nodes require {} bytes, buffer has {} bytes",
            aggregate_name, aggregate.nodes.size(), required_size,
            aggregate.target_buffer->get_size_bytes());
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Added node to aggregate '{}' (total: {})", aggregate_name, aggregate.nodes.size());
}

void AggregateBindingsProcessor::remove_node(
    const std::string& aggregate_name,
    const std::shared_ptr<Nodes::Node>& node)
{
    auto agg_it = m_aggregates.find(aggregate_name);
    if (agg_it == m_aggregates.end()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to remove node from non-existent aggregate '{}'", aggregate_name);
        return;
    }

    auto& nodes = agg_it->second.nodes;
    auto node_it = std::ranges::find(nodes, node);

    if (node_it != nodes.end()) {
        nodes.erase(node_it);
        agg_it->second.staging_data.resize(nodes.size());

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Removed node from aggregate '{}' (remaining: {})",
            aggregate_name, nodes.size());

        if (nodes.empty()) {
            m_aggregates.erase(agg_it);
            MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Removed empty aggregate '{}'", aggregate_name);
        }
    } else {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to remove node not in aggregate '{}'", aggregate_name);
    }
}

void AggregateBindingsProcessor::clear_aggregate(const std::string& aggregate_name)
{
    if (m_aggregates.erase(aggregate_name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to clear non-existent aggregate '{}'", aggregate_name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cleared aggregate '{}'", aggregate_name);
    }
}

void AggregateBindingsProcessor::clear_all_aggregates()
{
    size_t count = m_aggregates.size();
    m_aggregates.clear();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Cleared all aggregates ({})", count);
}

size_t AggregateBindingsProcessor::get_node_count(const std::string& aggregate_name) const
{
    auto it = m_aggregates.find(aggregate_name);
    return it != m_aggregates.end() ? it->second.nodes.size() : 0;
}

size_t AggregateBindingsProcessor::get_total_node_count() const
{
    size_t total = 0;
    for (const auto& [name, aggregate] : m_aggregates) {
        total += aggregate.nodes.size();
    }
    return total;
}

std::vector<std::string> AggregateBindingsProcessor::get_aggregate_names() const
{
    std::vector<std::string> names;
    names.reserve(m_aggregates.size());
    for (const auto& [name, _] : m_aggregates) {
        names.push_back(name);
    }
    return names;
}

size_t AggregateBindingsProcessor::get_aggregate_count() const
{
    return m_aggregates.size();
}

void AggregateBindingsProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_aggregates.empty()) {
        return;
    }

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AggregateBindingsProcessor requires VKBuffer, got different buffer type");
        return;
    }

    for (auto& [aggregate_name, aggregate] : m_aggregates) {
        if (aggregate.nodes.empty()) {
            continue;
        }

        for (size_t i = 0; i < aggregate.nodes.size(); i++) {
            aggregate.staging_data[i] = static_cast<float>(
                aggregate.nodes[i]->get_last_output());
        }

        upload_to_gpu(
            aggregate.staging_data.data(),
            aggregate.staging_data.size() * sizeof(float),
            aggregate.target_buffer);
    }

    bool attached_is_target = false;
    for (const auto& [name, aggregate] : m_aggregates) {
        if (aggregate.target_buffer == vk_buffer) {
            attached_is_target = true;
            break;
        }
    }

    if (!attached_is_target && !m_aggregates.empty()) {
        auto& first_aggregate = m_aggregates.begin()->second;

        if (!first_aggregate.nodes.empty()) {
            upload_to_gpu(
                first_aggregate.staging_data.data(),
                first_aggregate.staging_data.size() * sizeof(float),
                vk_buffer);
        }
    }
}

} // namespace MayaFlux::Buffers
