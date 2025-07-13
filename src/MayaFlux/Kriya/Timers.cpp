#include "Timers.hpp"
#include "MayaFlux/Kriya/Awaiters.hpp"
#include "MayaFlux/MayaFlux.hpp"
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
        co_await SampleDelay { delay_samples };

        if (timer_ptr && timer_ptr->is_active()) {
            timer_ptr->m_callback();
            timer_ptr->m_active = false;
        }
    };

    m_routine = std::make_shared<Vruta::SoundRoutine>(
        routine_func(m_Scheduler, m_Scheduler.seconds_to_samples(delay_seconds), this));
    m_Scheduler.add_task(m_routine);
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
}

NodeTimer::NodeTimer(Vruta::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager)
    : m_scheduler(scheduler)
    , m_node_graph_manager(graph_manager)
    , m_timer(scheduler)
{
}

void NodeTimer::play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, unsigned int channel)
{
    cancel();

    m_current_node = node;
    m_current_channel = channel;

    m_node_graph_manager.add_to_root(node, token, channel);

    m_timer.schedule(duration_seconds, [this, node, channel]() {
        m_node_graph_manager.get_token_root(token, channel).unregister_node(node);
        m_current_node = nullptr;
    });
}

void NodeTimer::play_with_processing(std::shared_ptr<Nodes::Node> node, std::function<void(std::shared_ptr<Nodes::Node>)> setup_func, std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func, double duration_seconds, unsigned int channel)
{
    cancel();

    m_current_node = node;
    m_current_channel = channel;

    setup_func(node);
    m_node_graph_manager.add_to_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);

    m_timer.schedule(duration_seconds, [this, node, cleanup_func, channel]() {
        cleanup_func(node);
        m_node_graph_manager.get_token_root(Nodes::ProcessingToken::AUDIO_RATE, channel).unregister_node(node);
        m_current_node = nullptr;
    });
}

void NodeTimer::cancel()
{
    if (m_timer.is_active() && m_current_node) {
        m_node_graph_manager.get_token_root(token, m_current_channel).unregister_node(m_current_node);
        m_current_node = nullptr;
    }
    m_timer.cancel();
}

}
