#include "NodeNetwork.hpp"

namespace MayaFlux::Nodes {

void NodeNetwork::register_channel_usage(uint32_t channel_id)
{
    if (channel_id < 32) {
        m_channel_mask |= (1U << channel_id);
    }
}

void NodeNetwork::unregister_channel_usage(uint32_t channel_id)
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
