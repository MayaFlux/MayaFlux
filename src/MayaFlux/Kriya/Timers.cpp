#include "Timers.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

Timer::Timer(Vruta::TaskScheduler& scheduler)
    : m_Scheduler(scheduler)
    , m_active(false)
{
}

void Timer::schedule(double delay_seconds, std::function<void()> callback)
{
    cancel();
    m_callback = std::move(callback);
    m_active = true;

    auto routine_func = [](Vruta::TaskScheduler& scheduler, uint64_t delay_samples, Timer* timer_ptr) -> Vruta::SoundRoutine {
        auto& promise = co_await Kriya::GetAudioPromise {};
        co_await SampleDelay { delay_samples };

        if (timer_ptr && timer_ptr->is_active()) {
            timer_ptr->m_callback();
            timer_ptr->m_active = false;
        }
    };

    m_routine = std::make_shared<Vruta::SoundRoutine>(
        routine_func(m_Scheduler, m_Scheduler.seconds_to_samples(delay_seconds), this));

    Vruta::ProcessingToken token = m_routine->get_processing_token();
    uint64_t current_time = m_Scheduler.current_units(token);

    m_Scheduler.add_task(m_routine, "", false);

    m_routine->initialize_state(current_time);
}

void Timer::cancel()
{
    if (m_active && m_routine) {
        m_Scheduler.cancel_task(m_routine);
        m_routine = nullptr;
        m_active = false;
    }
}

TimedAction::TimedAction(Vruta::TaskScheduler& scheduler)
    : m_Scheduler(scheduler)
    , m_timer(scheduler)
{
}

void TimedAction::execute(const std::function<void()>& start_func, const std::function<void()>& end_func, double duration_seconds)
{
    cancel();
    start_func();
    m_timer.schedule(duration_seconds, end_func);
}

void TimedAction::cancel()
{
    m_timer.cancel();
}

bool TimedAction::is_pending() const
{
    return m_timer.is_active();
}

TemporalActivation::TemporalActivation(Vruta::TaskScheduler& scheduler,
    Nodes::NodeGraphManager& node_graph_manager,
    Buffers::BufferManager& buffer_manager)
    : m_scheduler(scheduler)
    , m_node_graph_manager(node_graph_manager)
    , m_buffer_manager(buffer_manager)
    , m_timer(scheduler)
{
}

void TemporalActivation::activate_node(const std::shared_ptr<Nodes::Node>& node,
    double duration_seconds,
    Nodes::ProcessingToken token,
    const std::vector<uint32_t>& channels)
{
    cancel();

    m_current_node = node;
    m_node_token = token;
    m_channels = channels;
    m_active_type = ActiveType::NODE;

    for (auto channel : channels) {
        m_node_graph_manager.add_to_root(node, token, channel);
    }

    m_timer.schedule(duration_seconds, [this]() {
        cleanup_current_operation();
    });
}

void TemporalActivation::activate_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    double duration_seconds, Nodes::ProcessingToken token, const std::vector<uint32_t>& channels)
{
    cancel();

    m_current_network = network;
    m_node_token = token;
    m_active_type = ActiveType::NETWORK;
    m_channels = channels;

    for (const auto& ch : channels) {
        network->add_channel_usage(ch);
    }

    m_node_graph_manager.add_network(network, token);

    m_timer.schedule(duration_seconds, [this]() {
        cleanup_current_operation();
    });
}

void TemporalActivation::activate_buffer(const std::shared_ptr<Buffers::Buffer>& buffer,
    double duration_seconds,
    Buffers::ProcessingToken token,
    uint32_t channel)
{
    cancel();

    m_current_buffer = buffer;
    m_buffer_token = token;
    m_channels = { channel };
    m_active_type = ActiveType::BUFFER;

    m_buffer_manager.add_buffer(buffer, token, channel);

    m_timer.schedule(duration_seconds, [this]() {
        cleanup_current_operation();
    });
}

void TemporalActivation::cleanup_current_operation()
{
    switch (m_active_type) {
    case ActiveType::NODE:
        if (m_current_node) {
            if (m_channels.empty()) {
                m_node_graph_manager.get_root_node(m_node_token, 0).unregister_node(m_current_node);
            }
            for (auto channel : m_channels) {
                if (m_current_node->is_used_by_channel(channel)) {
                    m_node_graph_manager.get_root_node(m_node_token, channel).unregister_node(m_current_node);
                }
            }
            m_current_node = nullptr;
            m_channels.clear();
        }
        break;

    case ActiveType::NETWORK:
        if (m_current_network) {
            for (const auto& ch : m_channels) {
                m_current_network->remove_channel_usage(ch);
            }

            m_node_graph_manager.remove_network(m_current_network, m_node_token);
            m_current_network = nullptr;
        }
        break;

    case ActiveType::BUFFER:
        if (m_current_buffer) {
            for (auto channel : m_channels) {
                m_buffer_manager.remove_buffer(m_current_buffer, m_buffer_token, channel);
            }
            m_current_buffer = nullptr;
        }
        break;

    case ActiveType::NONE:
        break;
    }

    m_active_type = ActiveType::NONE;
}

void TemporalActivation::cancel()
{
    if (m_timer.is_active()) {
        cleanup_current_operation();
    }
    m_timer.cancel();
}

}
