#include "Scheduler.hpp"

namespace MayaFlux::Core::Scheduler {

TaskScheduler::TaskScheduler(unsigned int sample_rate)
    : m_clock(sample_rate)
{
}

void TaskScheduler::add_task(SoundRoutine&& task)
{
    m_tasks.push_back(std::move(task));
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

void TaskScheduler::process_buffer(unsigned int buffer_size)
{
    for (unsigned int i = 0; i < buffer_size; ++i) {
        process_sample();
    }
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
}
