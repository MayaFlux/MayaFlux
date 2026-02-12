#include "Operators.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Graph.hpp"

namespace MayaFlux::Kriya {

NodeTimeSpec::NodeTimeSpec(double seconds, std::optional<std::vector<uint32_t>> channels)
    : m_seconds(seconds)
    , m_channels(std::move(channels))
    , m_scheduler(*MayaFlux::get_scheduler())
    , m_graph_manager(*MayaFlux::get_node_graph_manager())
{
}

NodeTimeSpec::NodeTimeSpec(double seconds, Vruta::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager)
    : m_seconds(seconds)
    , m_scheduler(scheduler)
    , m_graph_manager(graph_manager)
{
}

NodeTimeSpec Time(double seconds)
{
    return { seconds };
}

NodeTimeSpec Time(double seconds, uint32_t channel)
{
    return NodeTimeSpec(seconds, std::vector<uint32_t> { channel });
}

NodeTimeSpec Time(double seconds, std::vector<uint32_t> channels)
{
    return { seconds, channels };
}

}
