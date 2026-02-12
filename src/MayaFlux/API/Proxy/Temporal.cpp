#include "Temporal.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Kriya/Timers.hpp"

namespace MayaFlux {

TimeSpec Time(double seconds, uint32_t channel)
{
    return { seconds, channel };
}

TimeSpec Time(double seconds, std::vector<uint32_t> channels)
{
    return { seconds, std::move(channels) };
}

std::shared_ptr<Nodes::Node> operator|(const TemporalWrapper<Nodes::Node>& wrapper, Domain domain)
{
    auto node = wrapper.entity();
    const auto& spec = wrapper.spec();
    auto node_token = get_node_token(domain);

    auto activation = std::make_shared<Kriya::TemporalActivation>(*get_scheduler(), *get_node_graph_manager(), *get_buffer_manager());

    if (spec.channels.has_value()) {
        activation->activate_node(node, spec.seconds, node_token, spec.channels.value());
    } else {
        activation->activate_node(node, spec.seconds, node_token);
    }

    static std::vector<std::shared_ptr<Kriya::TemporalActivation>> active_timers;
    active_timers.push_back(activation);

    if (active_timers.size() > 100) {
        std::erase_if(active_timers, [](const std::shared_ptr<Kriya::TemporalActivation>& t) {
            return !t->is_active();
        });
    }

    return node;
}

std::shared_ptr<Buffers::Buffer> operator|(const TemporalWrapper<Buffers::Buffer>& wrapper, Domain domain)
{
    auto buffer = wrapper.entity();
    const auto& spec = wrapper.spec();
    auto buffer_token = get_buffer_token(domain);

    auto activation = std::make_shared<Kriya::TemporalActivation>(*get_scheduler(), *get_node_graph_manager(), *get_buffer_manager());

    if (spec.channels.has_value() && !spec.channels.value().empty()) {
        activation->activate_buffer(buffer, spec.seconds, buffer_token, spec.channels.value()[0]);
    } else {
        activation->activate_buffer(buffer, spec.seconds, buffer_token);
    }

    static std::vector<std::shared_ptr<Kriya::TemporalActivation>> active_timers;
    active_timers.push_back(activation);

    if (active_timers.size() > 100) {
        std::erase_if(active_timers, [](const std::shared_ptr<Kriya::TemporalActivation>& t) {
            return !t->is_active();
        });
    }

    return buffer;
}

std::shared_ptr<Nodes::Network::NodeNetwork> operator|(const TemporalWrapper<Nodes::Network::NodeNetwork>& wrapper, Domain domain)
{
    auto network = wrapper.entity();
    const auto& spec = wrapper.spec();
    auto node_token = get_node_token(domain);

    auto activation = std::make_shared<Kriya::TemporalActivation>(*get_scheduler(), *get_node_graph_manager(), *get_buffer_manager());
    activation->activate_network(network, spec.seconds, node_token);

    static std::vector<std::shared_ptr<Kriya::TemporalActivation>> active_timers;
    active_timers.push_back(activation);

    if (active_timers.size() > 100) {
        std::erase_if(active_timers, [](const std::shared_ptr<Kriya::TemporalActivation>& t) {
            return !t->is_active();
        });
    }

    return network;
}

}
