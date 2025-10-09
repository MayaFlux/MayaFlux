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

void operator>>(std::shared_ptr<Nodes::Node> node, DAC& dac)
{
    MayaFlux::register_audio_node(node, dac.channel);
}

void operator>>(std::shared_ptr<Nodes::Node> node, const NodeTimeSpec& time_op)
{
    auto timer = std::make_shared<NodeTimer>(time_op.m_scheduler, time_op.m_graph_manager);

    if (time_op.has_explicit_channels()) {
        timer->play_for(node, time_op.get_seconds(), time_op.get_channels());
    } else {
        timer->play_for(node, time_op.get_seconds());
    }

    static std::vector<std::shared_ptr<NodeTimer>> active_timers;
    active_timers.push_back(timer);

    if (active_timers.size() > MayaFlux::Config::get_node_config().callback_cache_size) {
        active_timers.erase(
            std::remove_if(active_timers.begin(), active_timers.end(),
                [](const std::shared_ptr<NodeTimer>& t) {
                    return !t->is_active();
                }),
            active_timers.end());
    }
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
