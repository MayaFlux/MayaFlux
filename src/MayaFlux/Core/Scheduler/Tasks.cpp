#include "Tasks.hpp"

namespace MayaFlux::Core::Scheduler {

SoundRoutine metro(TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback)
{
    u_int64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);
    u_int64_t next_sample = 0;

    while (true) {
        callback();
        next_sample += interval_samples;
        co_await SampleDelay(interval_samples);
    }
}

SoundRoutine sequence(TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence)
{
    for (const auto& [time, callback] : sequence) {
        u_int64_t delay_samples = scheduler.seconds_to_samples(time);
        callback();
        co_await SampleDelay(delay_samples);
    }
}

SoundRoutine line(TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, u_int32_t step_duration, bool restartable)
{
    auto& promise_ref = co_await GetPromise {};
    const unsigned int sample_rate = scheduler.task_sample_rate();
    if (step_duration < 1) {
        step_duration = 1;
    }

    for (;;) {
        uint64_t total_samples = duration_seconds * sample_rate;
        float per_sample_step = (end_value - start_value) / total_samples;
        float sample_step = per_sample_step * step_duration;

        promise_ref.set_state("current_value", start_value);
        promise_ref.set_state("start_value", start_value);
        promise_ref.set_state("end_value", end_value);
        promise_ref.set_state("step", sample_step);
        promise_ref.set_state("restart", false);

        float* current_value = promise_ref.get_state<float>("current_value");
        float* last_value = promise_ref.get_state<float>("end_value");
        float* step = promise_ref.get_state<float>("step");

        uint64_t samples_elapsed = 0;

        while (samples_elapsed < total_samples) {
            if (!current_value || !last_value || !step) {
                current_value = promise_ref.get_state<float>("current_value");
                last_value = promise_ref.get_state<float>("end_value");
                step = promise_ref.get_state<float>("step");
            }

            if (current_value && last_value && step) {
                *current_value += *step;

                if ((*step > 0 && *current_value >= *last_value) || (*step < 0 && *current_value <= *last_value)) {
                    *current_value = *last_value;
                }
            }

            samples_elapsed += step_duration;
            co_await SampleDelay { step_duration };
        }

        if (!restartable)
            break;

        bool* restart_requested = promise_ref.get_state<bool>("restart");
        if (restart_requested && *restart_requested) {
            *restart_requested = false;
            continue;
        }

        promise_ref.auto_resume = false;
        co_await std::suspend_always {};
    }
}

}
