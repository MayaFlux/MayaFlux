#include "Tasks.hpp"
#include "Awaiters/DelayAwaiters.hpp"
#include "RoutineDomain.hpp"

#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    template <typename Domain>
    typename Domain::Routine metro_routine(double interval, std::function<void()> cb)
    {
        uint64_t units = Domain::units(interval);
        if (units < 1) {
            MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "metro interval {}s is shorter than one clock unit, clamping to 1 unit", interval);
        }
        units = std::max<uint64_t>(1, units);
        auto& p = co_await typename Domain::GetPromise {};
        while (!p.should_terminate) {
            cb();
            co_await Domain::delay(units);
        }
    }

    template <typename Domain>
    typename Domain::Routine sequence_routine(std::vector<std::pair<double, std::function<void()>>> seq)
    {
        for (const auto& [time, cb] : seq) {
            co_await Domain::delay(Domain::units(time));
            cb();
        }
    }

}

std::shared_ptr<Vruta::Routine> metro(double interval_seconds, std::function<void()> callback, Vruta::ProcessingToken token)
{
    if (token == Vruta::ProcessingToken::FRAME_ACCURATE) {
        return std::make_shared<Vruta::GraphicsRoutine>(
            metro_routine<RoutineDomain<Vruta::ProcessingToken::FRAME_ACCURATE>>(interval_seconds, std::move(callback)));
    }
    return std::make_shared<Vruta::SoundRoutine>(
        metro_routine<RoutineDomain<Vruta::ProcessingToken::SAMPLE_ACCURATE>>(interval_seconds, std::move(callback)));
}

std::shared_ptr<Vruta::Routine> sequence(std::vector<std::pair<double, std::function<void()>>> sequence, Vruta::ProcessingToken token)
{
    if (token == Vruta::ProcessingToken::FRAME_ACCURATE) {
        return std::make_shared<Vruta::GraphicsRoutine>(
            sequence_routine<RoutineDomain<Vruta::ProcessingToken::FRAME_ACCURATE>>(std::move(sequence)));
    }
    return std::make_shared<Vruta::SoundRoutine>(
        sequence_routine<RoutineDomain<Vruta::ProcessingToken::SAMPLE_ACCURATE>>(std::move(sequence)));
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
