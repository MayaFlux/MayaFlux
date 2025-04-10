#pragma once

#include "config.h"

namespace MayaFlux::Core::Scheduler {
class TaskScheduler;
class SoundRoutine;
}

namespace MayaFlux::Tasks {

Core::Scheduler::SoundRoutine metro(Core::Scheduler::TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback);

Core::Scheduler::SoundRoutine sequence(Core::Scheduler::TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence);

Core::Scheduler::SoundRoutine line(Core::Scheduler::TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, u_int32_t step_duration = 5, bool restartable = false);

Core::Scheduler::SoundRoutine pattern(Core::Scheduler::TaskScheduler& scheduler, std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);
}
