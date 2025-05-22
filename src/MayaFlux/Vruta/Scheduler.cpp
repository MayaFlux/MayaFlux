#include "Scheduler.hpp"

namespace MayaFlux::Vruta {

TaskScheduler::TaskScheduler(unsigned int sample_rate)
    : m_clock(sample_rate)
{
}

void TaskScheduler::add_task(std::shared_ptr<SoundRoutine> task, bool initialize)
{
    if (task) {
        if (initialize) {
            task->initialize_state(m_clock.current_sample());
        }
        m_tasks.push_back(task);
    }
}

void TaskScheduler::process_sample()
{
    u_int64_t current_sample = m_clock.current_sample();

    for (auto& task : m_tasks) {
        if (task)
            task->try_resume(current_sample);
    }

    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const std::shared_ptr<SoundRoutine>& task) {
                return !task || (task->get_handle() && task->get_handle().done());
            }),
        m_tasks.end());

    m_clock.tick();
}

void TaskScheduler::process_buffer(unsigned int buffer_size)
{
    for (unsigned int i = 0; i < buffer_size; ++i) {
        process_sample();
    }
}

bool TaskScheduler::cancel_task(std::shared_ptr<SoundRoutine> task)
{
    if (!task) {
        return false;
    }
    auto it = std::find(m_tasks.begin(), m_tasks.end(), task);
    if (it != m_tasks.end()) {

        if (auto& routine = *it; routine->is_active()) {
            auto& promise = routine->get_handle().promise();
            promise.should_terminate = true;
            promise.auto_resume = true;
            promise.next_sample = 0;
        }

        m_tasks.erase(it);
        return true;
    }
    return false;
}

const u_int64_t TaskScheduler::get_next_task_id() const
{
    if (m_tasks.size()) {
        return m_tasks.size() + 1;
    }
    return 1;
}
}
