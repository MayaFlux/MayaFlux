#pragma once

#include "Scheduler.hpp"

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

    const SampleClock& get_clock() const { return m_clock; }

    const std::vector<SoundRoutine>& get_tasks() const { return m_tasks; }

    std::vector<SoundRoutine>& get_tasks() { return m_tasks; }

    bool cancel_task(SoundRoutine* task);

private:
    SampleClock m_clock;
    std::vector<SoundRoutine> m_tasks;
};

SoundRoutine metro(TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback);

SoundRoutine sequence(TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence);

template <typename T>
SoundRoutine pattern(TaskScheduler& scheduler, std::function<T(u_int64_t)> pattern_func, std::function<void(T)> callback, double interval_seconds)
{
    u_int64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);
    u_int64_t step = 0;

    while (true) {
        T value = pattern_func(step++);
        callback(value);
        co_await SampleDelay(interval_samples);
    }
}
}
