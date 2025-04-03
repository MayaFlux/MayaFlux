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

SoundRoutine line(TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, bool loop)
{
    auto& promise_ref = co_await GetPromise {};

    unsigned int sample_rate = scheduler.task_sample_rate();
    u_int64_t total_samples = duration_seconds * sample_rate;
    float step = (end_value - start_value) / total_samples;

    promise_ref.set_state("current_value", start_value);
    promise_ref.set_state("start_value", start_value);
    promise_ref.set_state("end_value", end_value);
    promise_ref.set_state("step", step);
    promise_ref.set_state("loop", loop);

    // co_await std::suspend_never {};

    u_int64_t samples_elapsed = 0;

    while (samples_elapsed < total_samples) {
        float* current_value = promise_ref.get_state<float>("current_value");
        float* end_value = promise_ref.get_state<float>("end_value");
        float* step = promise_ref.get_state<float>("step");

        if (current_value && end_value && step) {
            *current_value += *step;

            if ((*step > 0 && *current_value >= *end_value) || (*step < 0 && *current_value <= *end_value)) {
                *current_value = *end_value;
            }
        }

        samples_elapsed++;
        bool* should_loop = promise_ref.get_state<bool>("loop");

        if (!should_loop || !*should_loop) {
            break;
        }
        co_await SampleDelay { 1 };
    }
}

}
