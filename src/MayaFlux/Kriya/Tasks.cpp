#include "Tasks.hpp"
#include "Awaiters/ConditionAwaiter.hpp"
#include "Awaiters/DelayAwaiters.hpp"
#include "RoutineTraits.hpp"

#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    template <typename Traits>
    typename Traits::Routine metro_routine(double interval, std::function<void()> cb)
    {
        uint64_t units = Traits::units(interval);
        if (units < 1) {
            MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "metro interval {}s is shorter than one clock unit, clamping to 1 unit", interval);
        }
        units = std::max<uint64_t>(1, units);
        auto& p = co_await typename Traits::GetPromise {};
        while (!p.should_terminate) {
            cb();
            co_await Traits::delay(units);
        }
    }

    Vruta::FreeRoutine free_metro_routine(double interval, std::function<void()> cb)
    {
        auto& p = co_await OnWorker { Vruta::WorkerId {} };
        while (!p.should_terminate) {
            cb();
            co_await SteadyDelay { interval };
        }
    }

    template <typename Traits>
    typename Traits::Routine sequence_routine(std::vector<std::pair<double, std::function<void()>>> seq)
    {
        for (const auto& [time, cb] : seq) {
            co_await Traits::delay(Traits::units(time));
            cb();
        }
    }

    Vruta::FreeRoutine free_sequence_routine(std::vector<std::pair<double, std::function<void()>>> seq)
    {
        auto& p = co_await OnWorker { Vruta::WorkerId {} };
        for (const auto& [time, cb] : seq) {
            co_await SteadyDelay { time };
            if (p.should_terminate) {
                co_return;
            }
            cb();
        }
    }

    template <typename Traits>
    typename Traits::Routine cycle_routine(std::function<void()> cb)
    {
        auto& p = co_await typename Traits::GetPromise {};
        while (!p.should_terminate) {
            cb();
            co_await Traits::pause();
        }
    }

    Vruta::FreeRoutine free_cycle_routine(std::function<void()> cb)
    {
        auto& p = co_await OnWorker { Vruta::WorkerId {} };
        while (!p.should_terminate) {
            cb();
            co_await SteadyDelay { 0.0 };
        }
    }

}

std::shared_ptr<Vruta::Routine> metro(double interval_seconds, std::function<void()> callback, Vruta::ProcessingToken token)
{
    using Vruta::ProcessingToken;

    switch (token) {
    case ProcessingToken::FRAME_ACCURATE:
        return std::make_shared<Vruta::GraphicsRoutine>(
            metro_routine<RoutineTraits<ProcessingToken::FRAME_ACCURATE>>(interval_seconds, std::move(callback)));
    case ProcessingToken::MULTI_RATE:
        return std::make_shared<Vruta::CrossRoutine>(
            metro_routine<RoutineTraits<ProcessingToken::MULTI_RATE>>(interval_seconds, std::move(callback)));
    case ProcessingToken::CONDITIONAL:
        return std::make_shared<Vruta::FreeRoutine>(
            free_metro_routine(interval_seconds, std::move(callback)));
    default:
        return std::make_shared<Vruta::SoundRoutine>(
            metro_routine<RoutineTraits<ProcessingToken::SAMPLE_ACCURATE>>(interval_seconds, std::move(callback)));
    }
}

std::shared_ptr<Vruta::Routine> sequence(std::vector<std::pair<double, std::function<void()>>> sequence, Vruta::ProcessingToken token)
{
    using Vruta::ProcessingToken;

    switch (token) {
    case ProcessingToken::FRAME_ACCURATE:
        return std::make_shared<Vruta::GraphicsRoutine>(
            sequence_routine<RoutineTraits<ProcessingToken::FRAME_ACCURATE>>(std::move(sequence)));
    case ProcessingToken::MULTI_RATE:
        return std::make_shared<Vruta::CrossRoutine>(
            sequence_routine<RoutineTraits<ProcessingToken::MULTI_RATE>>(std::move(sequence)));
    case ProcessingToken::CONDITIONAL:
        return std::make_shared<Vruta::FreeRoutine>(free_sequence_routine(std::move(sequence)));
    default:
        return std::make_shared<Vruta::SoundRoutine>(
            sequence_routine<RoutineTraits<ProcessingToken::SAMPLE_ACCURATE>>(std::move(sequence)));
    }
}

std::shared_ptr<Vruta::Routine> each_cycle(std::function<void()> callback, Vruta::ProcessingToken token)
{
    using Vruta::ProcessingToken;

    if (!callback) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "each_cycle requires a callback");
        return nullptr;
    }

    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
        return std::make_shared<Vruta::SoundRoutine>(
            cycle_routine<RoutineTraits<ProcessingToken::SAMPLE_ACCURATE>>(std::move(callback)));
    case ProcessingToken::FRAME_ACCURATE:
        return std::make_shared<Vruta::GraphicsRoutine>(
            cycle_routine<RoutineTraits<ProcessingToken::FRAME_ACCURATE>>(std::move(callback)));
    case ProcessingToken::MULTI_RATE:
        return std::make_shared<Vruta::CrossRoutine>(
            cycle_routine<RoutineTraits<ProcessingToken::MULTI_RATE>>(std::move(callback)));
    case ProcessingToken::CONDITIONAL:
        return std::make_shared<Vruta::FreeRoutine>(free_cycle_routine(std::move(callback)));
    default:
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "each_cycle supports SAMPLE_ACCURATE, FRAME_ACCURATE, MULTI_RATE and CONDITIONAL tokens");
        return nullptr;
    }
}

Vruta::SoundRoutine line(float start_value, float end_value, float duration_seconds, uint32_t step_duration, bool restartable)
{
    auto& promise_ref = co_await GetAudioPromise {};

    promise_ref.set_state("current_value", start_value);
    promise_ref.set_state("end_value", end_value);
    promise_ref.set_state("restart", false);

    const unsigned int sample_rate = Vruta::s_registered_sample_rate;
    if (step_duration < 1) {
        step_duration = 1;
    }

    uint64_t total_samples = static_cast<uint64_t>(static_cast<double>(duration_seconds) * sample_rate);
    float per_sample_step = (end_value - start_value) / (float)total_samples;
    float sample_step = per_sample_step * (float)step_duration;

    promise_ref.set_state("step", sample_step);

    for (;;) {
        auto current_value = promise_ref.get_state<float>("current_value");
        auto last_value = promise_ref.get_state<float>("end_value");
        auto step = promise_ref.get_state<float>("step");

        if (!current_value || !last_value || !step) {
            MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling, "Line task state not properly initialized");
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

        auto restart_requested = promise_ref.get_state<bool>("restart");
        if (restart_requested && *restart_requested) {
            *restart_requested = false;
            continue;
        }

        promise_ref.auto_resume = false;
        co_await std::suspend_always {};
    }
}
}
