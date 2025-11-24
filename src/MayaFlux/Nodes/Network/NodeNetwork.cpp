#include "NodeNetwork.hpp"

namespace MayaFlux::Nodes {

void NodeNetwork::ensure_initialized()
{
    if (!m_initialized) {
        initialize();
        m_initialized = true;
    }
}

std::optional<std::vector<double>> NodeNetwork::get_audio_buffer() const
{
    if (m_output_mode == OutputMode::AUDIO_SINK && !m_last_audio_buffer.empty()) {
        return m_last_audio_buffer;
    }
    return std::nullopt;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
NodeNetwork::get_metadata() const
{
    return { { "topology", topology_to_string(m_topology) },
        { "output_mode", output_mode_to_string(m_output_mode) },
        { "node_count", std::to_string(get_node_count()) },
        { "enabled", m_enabled ? "true" : "false" } };
}

void NodeNetwork::map_parameter(const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    // Default: store mapping, subclass handles in process_batch()
    m_parameter_mappings.push_back({ param_name, mode, source, nullptr });
}

void NodeNetwork::map_parameter(const std::string& param_name,
    const std::shared_ptr<NodeNetwork>& source_network)
{
    m_parameter_mappings.push_back(
        { param_name, MappingMode::ONE_TO_ONE, nullptr, source_network });
}

void NodeNetwork::unmap_parameter(const std::string& param_name)
{
    m_parameter_mappings.erase(
        std::remove_if(
            m_parameter_mappings.begin(), m_parameter_mappings.end(),
            [&](const auto& m) { return m.param_name == param_name; }),
        m_parameter_mappings.end());
}

bool NodeNetwork::is_processing() const
{
    return m_processing_state.load(std::memory_order_acquire);
}

void NodeNetwork::add_channel_usage(uint32_t channel_id)
{
    if (channel_id < 32) {
        m_channel_mask |= (1U << channel_id);
    }
}

void NodeNetwork::remove_channel_usage(uint32_t channel_id)
{
    if (channel_id < 32) {
        m_channel_mask &= ~(1U << channel_id);
    }
}

bool NodeNetwork::is_registered_on_channel(uint32_t channel_id) const
{
    if (channel_id < 32) {
        return (m_channel_mask & (1U << channel_id)) != 0;
    }
    return false;
}

std::vector<uint32_t> NodeNetwork::get_registered_channels() const
{
    std::vector<uint32_t> channels;
    for (uint32_t i = 0; i < 32; ++i) {
        if (m_channel_mask & (1U << i)) {
            channels.push_back(i);
        }
    }
    return channels;
}

void NodeNetwork::mark_processing(bool processing)
{
    m_processing_state.store(processing, std::memory_order_release);
}

bool NodeNetwork::is_processed_this_cycle() const
{
    return m_processed_this_cycle.load(std::memory_order_acquire);
}

void NodeNetwork::mark_processed(bool processed)
{
    m_processed_this_cycle.store(processed, std::memory_order_release);
}

std::string NodeNetwork::topology_to_string(Topology topo)
{
    switch (topo) {
    case Topology::INDEPENDENT:
        return "INDEPENDENT";
    case Topology::CHAIN:
        return "CHAIN";
    case Topology::RING:
        return "RING";
    case Topology::GRID_2D:
        return "GRID_2D";
    case Topology::GRID_3D:
        return "GRID_3D";
    case Topology::SPATIAL:
        return "SPATIAL";
    case Topology::CUSTOM:
        return "CUSTOM";
    default:
        return "UNKNOWN";
    }
}

std::string NodeNetwork::output_mode_to_string(OutputMode mode)
{
    switch (mode) {
    case OutputMode::NONE:
        return "NONE";
    case OutputMode::AUDIO_SINK:
        return "AUDIO_SINK";
    case OutputMode::GRAPHICS_BIND:
        return "GRAPHICS_BIND";
    case OutputMode::CUSTOM:
        return "CUSTOM";
    default:
        return "UNKNOWN";
    }
}

} // namespace MayaFlux::Nodes
