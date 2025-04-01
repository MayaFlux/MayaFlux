#pragma once

#include "Scheduler.hpp"

namespace MayaFlux::Core::Scheduler {

SoundRoutine metro(TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback);

SoundRoutine sequence(TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence);

SoundRoutine line(TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, bool loop = false);

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
