#include "Tasks.hpp"
#include "Awaiters.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

Vruta::SoundRoutine metro(Vruta::TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback)
{
    uint64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);
    auto& promise = co_await Kriya::GetAudioPromise {};

    while (true) {
        if (promise.should_terminate) {
            break;
        }
        callback();
        co_await SampleDelay(interval_samples);
    }
}

Vruta::SoundRoutine sequence(Vruta::TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence)
{
    for (const auto& [time, callback] : sequence) {
        uint64_t delay_samples = scheduler.seconds_to_samples(time);
        co_await SampleDelay(delay_samples);
        callback();
    }
}

Vruta::SoundRoutine line(Vruta::TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, uint32_t step_duration, bool restartable)
{
    auto& promise_ref = co_await GetAudioPromise {};

    promise_ref.set_state("current_value", start_value);
    promise_ref.set_state("end_value", end_value);
    promise_ref.set_state("restart", false);

    const unsigned int sample_rate = scheduler.get_rate();
    if (step_duration < 1) {
        step_duration = 1;
    }

    uint64_t total_samples = duration_seconds * sample_rate;
    float per_sample_step = (end_value - start_value) / total_samples;
    float sample_step = per_sample_step * step_duration;

    promise_ref.set_state("step", sample_step);

    for (;;) {
        float* current_value = promise_ref.get_state<float>("current_value");
        float* last_value = promise_ref.get_state<float>("end_value");
        float* step = promise_ref.get_state<float>("step");

        if (!current_value || !last_value || !step) {
            std::cerr << "Error: line task state not properly initialized" << std::endl;
            co_return;
        }

        *current_value = start_value;

        uint64_t samples_elapsed = 0;

        co_await SampleDelay { 1 };

        while (samples_elapsed < total_samples) {
            *current_value += *step;

            if ((*step > 0 && *current_value >= *last_value) || (*step < 0 && *current_value <= *last_value)) {
                *current_value = *last_value;
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

Vruta::SoundRoutine pattern(Vruta::TaskScheduler& scheduler, std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds)
{
    uint64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);
    uint64_t step = 0;

    while (true) {
        std::any value = pattern_func(step++);
        callback(value);
        co_await SampleDelay(interval_samples);
    }
}

Vruta::SoundRoutine Gate(
    Vruta::TaskScheduler& scheduler,
    std::function<void()> callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node,
    bool open)
{
    auto& promise_ref = co_await GetAudioPromise {};

    if (!logic_node) {
        logic_node = std::make_shared<Nodes::Generator::Logic>(0.5);
    }

    if (open) {
        logic_node->while_true([callback](const Nodes::NodeContext& ctx) {
            callback();
        });
    } else {
        logic_node->while_false([callback](const Nodes::NodeContext& ctx) {
            callback();
        });
    }

    while (true) {
        if (promise_ref.should_terminate) {
            break;
        }

        logic_node->process_sample(0.0);

        co_await SampleDelay { 1 };
    }
}

Vruta::SoundRoutine Trigger(
    Vruta::TaskScheduler& scheduler,
    bool target_state,
    std::function<void()> callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node)
{
    auto& promise_ref = co_await GetAudioPromise {};

    if (!logic_node) {
        logic_node = std::make_shared<Nodes::Generator::Logic>(0.5);
    }

    logic_node->on_change_to([callback](const Nodes::NodeContext& ctx) {
        callback();
    },
        target_state);

    while (true) {
        if (promise_ref.should_terminate) {
            break;
        }

        logic_node->process_sample(0.0);

        co_await SampleDelay { 1 };
    }
}

Vruta::SoundRoutine Toggle(
    Vruta::TaskScheduler& scheduler,
    std::function<void()> callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node)
{
    auto& promise_ref = co_await GetAudioPromise {};

    if (!logic_node) {
        logic_node = std::make_shared<Nodes::Generator::Logic>(0.5);
    }

    logic_node->on_change([callback](const Nodes::NodeContext& ctx) {
        callback();
    });

    while (true) {
        if (promise_ref.should_terminate) {
            break;
        }

        logic_node->process_sample(0.0);

        co_await SampleDelay { 1 };
    }
}
}
