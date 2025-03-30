#include "Tasks.hpp"

namespace MayaFlux::Core::Scheduler {

TaskScheduler::TaskScheduler(unsigned int sample_rate)
    : m_clock(sample_rate)
{
}

void TaskScheduler::add_task(SoundRoutine&& Task)
{
    m_tasks.push_back(std::move(Task));
}

void TaskScheduler::process_sample()
{
    u_int64_t current_sample = m_clock.current_sample();

    for (auto& task : m_tasks) {
        task.try_resume(current_sample);
    }

    m_tasks.erase(std::remove_if(
                      m_tasks.begin(), m_tasks.end(),
                      [](const SoundRoutine& task) { return !task.is_active(); }),
        m_tasks.end());

    m_clock.tick();
}

bool TaskScheduler::cancel_task(SoundRoutine* task)
{
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
        [task](const SoundRoutine& routine) {
            return &routine == task;
        });
    if (it != m_tasks.end()) {
        it->get_handle().destroy();
        m_tasks.erase(it);
        return true;
    }
    return false;
}

void TaskScheduler::process_buffer(unsigned int buffer_size)
{
    for (unsigned int i = 0; i < buffer_size; ++i) {
        process_sample();
    }
}

SoundRoutine metro(TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback)
{
    u_int64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);

    while (true) {
        callback();
        co_await SampleDelay(interval_samples);
    }
}

SoundRoutine sequence(TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence)
{
    for (const auto& [delay_seconds, callback] : sequence) {
        u_int64_t delay_samples = scheduler.seconds_to_samples(delay_seconds);
        co_await SampleDelay(delay_samples);
        callback();
    }
}
}
