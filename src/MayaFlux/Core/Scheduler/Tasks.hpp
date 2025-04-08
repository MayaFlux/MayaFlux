#pragma once

#include "Scheduler.hpp"

namespace MayaFlux::Core::Scheduler {

SoundRoutine metro(TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback);

SoundRoutine sequence(TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence);

SoundRoutine line(TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, u_int32_t step_duration = 5, bool restartable = false);

SoundRoutine pattern(TaskScheduler& scheduler, std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);
}
