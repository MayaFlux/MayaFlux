#include "Timers.hpp"
#include "MayaFlux/Core/Scheduler/Scheduler.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Tasks/Awaiters.hpp"

namespace MayaFlux::Tasks {

Timer::Timer(Core::Scheduler::TaskScheduler& scheduler)
    : m_Scheduler(scheduler)
    , m_active(false)
{
}

void Timer::schedule(double delay_seconds, std::function<void()> callback)
{
    cancel();

    m_callback = callback;

    m_active = true;

    auto routine_func = [](Core::Scheduler::TaskScheduler& scheduler, u_int64_t delay_samples, Timer* timer_ptr) -> Core::Scheduler::SoundRoutine {
        co_await SampleDelay { delay_samples };

        if (timer_ptr && timer_ptr->is_active()) {
            timer_ptr->m_callback();
            timer_ptr->m_active = false;
        }
    };

    m_routine = std::make_shared<Core::Scheduler::SoundRoutine>(
        routine_func(m_Scheduler, m_Scheduler.seconds_to_samples(delay_seconds), this));
    m_Scheduler.add_task(m_routine);
}

void Timer::cancel()
{
    if (m_active && m_routine) {
        m_routine->get_handle().destroy();
        m_active = false;
    }
}

TimedAction::TimedAction(Core::Scheduler::TaskScheduler& scheduler)
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

NodeTimer::NodeTimer(Core::Scheduler::TaskScheduler& scheduler)
    : m_scheduler(scheduler)
    , m_timer(scheduler)
{
}

void NodeTimer::play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, unsigned int channel)
{
    cancel();

    m_current_node = node;
    m_current_channel = channel;

    MayaFlux::add_node_to_root(node);

    m_timer.schedule(duration_seconds, [this, node, channel]() {
        MayaFlux::remove_node_from_root(node, channel);
        m_current_node = nullptr;
    });
}

void NodeTimer::play_with_processing(std::shared_ptr<Nodes::Node> node, std::function<void(std::shared_ptr<Nodes::Node>)> setup_func, std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func, double duration_seconds, unsigned int channel)
{
    cancel();

    m_current_node = node;
    m_current_channel = channel;

    setup_func(node);
    MayaFlux::add_node_to_root(node, channel);

    m_timer.schedule(duration_seconds, [this, node, cleanup_func, channel]() {
        cleanup_func(node);
        MayaFlux::remove_node_from_root(node, channel);
        m_current_node = nullptr;
    });
}

void NodeTimer::cancel()
{
    if (m_timer.is_active() && m_current_node) {
        MayaFlux::remove_node_from_root(m_current_node, m_current_channel);
        m_current_node = nullptr;
    }
    m_timer.cancel();
}

}
