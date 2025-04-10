#include "Operators.hpp"
#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Tasks {

void operator>>(std::shared_ptr<Nodes::Node> node, DAC& dac)
{
    MayaFlux::add_node_to_root(node, dac.channel);
}

void operator>>(std::shared_ptr<Nodes::Node> node, const TimeOperation& time_op)
{
    auto timer = std::make_shared<NodeTimer>(*MayaFlux::get_scheduler());
    timer->play_for(node, time_op.get_seconds());

    static std::vector<std::shared_ptr<NodeTimer>> active_timers;
    active_timers.push_back(timer);

    if (active_timers.size() > 20) {
        active_timers.erase(
            std::remove_if(active_timers.begin(), active_timers.end(),
                [](const std::shared_ptr<NodeTimer>& t) { return !t->is_active(); }),
            active_timers.end());
    }
}

TimeOperation Time(double seconds)
{
    return TimeOperation(seconds);
}

}
