#include "Timers.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Kriya/Awaiters.hpp"
#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

static MayaFlux::Nodes::ProcessingToken token = MayaFlux::Nodes::ProcessingToken::AUDIO_RATE;

namespace MayaFlux::Kriya {

Timer::Timer(Vruta::TaskScheduler& scheduler)
    : m_Scheduler(scheduler)
    , m_active(false)
{
}

void Timer::schedule(double delay_seconds, std::function<void()> callback)
{
    cancel();
    m_callback = callback;
    m_active = true;

    auto routine_func = [](Vruta::TaskScheduler& scheduler, u_int64_t delay_samples, Timer* timer_ptr) -> Vruta::SoundRoutine {
        auto& promise = co_await Kriya::GetPromise {};
        co_await SampleDelay { delay_samples };

        if (timer_ptr && timer_ptr->is_active()) {
            timer_ptr->m_callback();
            timer_ptr->m_active = false;
        }
    };

    m_routine = std::make_shared<Vruta::SoundRoutine>(
        routine_func(m_Scheduler, m_Scheduler.seconds_to_samples(delay_seconds), this));

    Vruta::ProcessingToken token = m_routine->get_processing_token();
    u_int64_t current_time = m_Scheduler.current_units(token);

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

void TimedAction::execute(std::function<void()> start_func, std::function<void()> end_func, double duration_seconds)
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

NodeTimer::NodeTimer(Vruta::TaskScheduler& scheduler)
    : m_scheduler(scheduler)
    , m_node_graph_manager(*MayaFlux::get_node_graph_manager())
    , m_timer(scheduler)
{
    auto num_channels = MayaFlux::Config::get_num_out_channels();
    m_max_channels = num_channels > 0 ? num_channels : MayaFlux::Config::get_node_config().channel_cache_size;
}

NodeTimer::NodeTimer(Vruta::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager)
    : m_scheduler(scheduler)
    , m_node_graph_manager(graph_manager)
    , m_timer(scheduler)
{
    auto num_channels = MayaFlux::Config::get_num_out_channels();
    m_max_channels = num_channels > 0 ? num_channels : MayaFlux::Config::get_node_config().channel_cache_size;
}

void NodeTimer::play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, u_int32_t channel)
{
    play_for(node, duration_seconds, std::vector<u_int32_t> { channel });
}

void NodeTimer::play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, std::vector<u_int32_t> channels)
{
    cancel();

    m_current_node = node;
    m_channels = channels;

    for (auto& channel : channels) {
        m_node_graph_manager.add_to_root(node, token, channel);
    }

    m_timer.schedule(duration_seconds, [this]() {
        cleanup_current_operation();
    });
}

void NodeTimer::play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds)
{
    auto source_mask = node->get_channel_mask().load(std::memory_order_relaxed);
    std::vector<u_int32_t> channels;

    if (source_mask == 0) {
        channels = { 0 };
    } else {
        for (u_int32_t channel = 0; channel < m_max_channels; ++channel) {
            if (node->is_used_by_channel(channel)) {
                channels.push_back(channel);
            }
        }
    }

    play_for(node, duration_seconds, channels);
}

void NodeTimer::play_with_processing(std::shared_ptr<Nodes::Node> node, std::function<void(std::shared_ptr<Nodes::Node>)> setup_func, std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func, double duration_seconds, u_int32_t channel)
{
    play_with_processing(node, setup_func, cleanup_func, duration_seconds, std::vector<u_int32_t> { channel });
}

void NodeTimer::play_with_processing(std::shared_ptr<Nodes::Node> node, std::function<void(std::shared_ptr<Nodes::Node>)> setup_func, std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func, double duration_seconds, std::vector<u_int32_t> channels)
{
    cancel();

    m_current_node = node;
    m_channels = channels;

    setup_func(node);

    for (auto channel : channels) {
        m_node_graph_manager.add_to_root(node, token, channel);
    }

    m_timer.schedule(duration_seconds, [this, node, cleanup_func]() {
        cleanup_func(node);
        cleanup_current_operation();
    });
}

void NodeTimer::play_with_processing(std::shared_ptr<Nodes::Node> node,
    std::function<void(std::shared_ptr<Nodes::Node>)> setup_func,
    std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func,
    double duration_seconds)
{
    auto source_mask = node->get_channel_mask().load(std::memory_order_relaxed);
    std::vector<u_int32_t> channels;

    if (source_mask == 0) {
        channels = { 0 };
    } else {
        for (u_int32_t channel = 0; channel < m_max_channels; ++channel) {
            if (node->is_used_by_channel(channel)) {
                channels.push_back(channel);
            }
        }
    }

    play_with_processing(node, setup_func, cleanup_func, duration_seconds, channels);
}

void NodeTimer::cleanup_current_operation()
{
    if (m_current_node) {
        for (auto channel : m_channels) {
            if (m_current_node->is_used_by_channel(channel)) {
                m_node_graph_manager.get_root_node(token, channel).unregister_node(m_current_node);
            }
        }
        m_current_node = nullptr;
        m_channels.clear();
    }
}

void NodeTimer::cancel()
{
    if (m_timer.is_active()) {
        cleanup_current_operation();
    }
    m_timer.cancel();
}

}
