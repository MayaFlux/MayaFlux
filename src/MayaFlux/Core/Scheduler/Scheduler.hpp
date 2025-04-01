#pragma once

#include "config.h"

#include "Clock.hpp"
#include "Routine.hpp"

namespace MayaFlux::Core::Scheduler {

class TaskScheduler {
public:
    TaskScheduler(unsigned int sample_rate = 48000);

    void add_task(SoundRoutine&& Task);
    void process_sample();
    void process_buffer(unsigned int buffer_size);

    inline u_int64_t seconds_to_samples(double seconds) const
    {
        return static_cast<u_int64_t>(seconds * m_clock.sample_rate());
    }

    inline unsigned int task_sample_rate()
    {
        return m_clock.sample_rate();
    }

    const SampleClock& get_clock() const { return m_clock; }
    const std::vector<SoundRoutine>& get_tasks() const { return m_tasks; }
    std::vector<SoundRoutine>& get_tasks() { return m_tasks; }

    bool cancel_task(SoundRoutine* task);

private:
    SampleClock m_clock;
    std::vector<SoundRoutine> m_tasks;
};

}
