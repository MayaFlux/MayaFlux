#pragma once

#include "MayaFlux/Vruta/ChronUtils.hpp"
#include "MayaFlux/Vruta/TokenTraits.hpp"

#include "Awaiters/DelayAwaiters.hpp"

namespace MayaFlux::Kriya {

/**
 * @struct DelayFor
 * @brief The awaiter that waits on a promise's clock, built from a unit count
 */
template <typename Promise>
struct DelayFor;

template <>
struct DelayFor<Vruta::audio_promise> {
    static SampleDelay make(uint64_t count) { return SampleDelay { count }; }
};

template <>
struct DelayFor<Vruta::graphics_promise> {
    static FrameDelay make(uint64_t count) { return FrameDelay { .frames_to_wait = count }; }
};

template <>
struct DelayFor<Vruta::cross_promise> {
    static MultiRateDelay make(uint64_t count) { return MultiRateDelay { .samples_to_wait = count, .frames_to_wait = 0 }; }
};

/**
 * @struct RoutineTraits
 * @brief What a clocked token means for a coroutine: routine, promise accessor, delays and unit conversion
 *
 * Built from Vruta::TokenTraits, so it adds only what Vruta cannot know: the
 * awaiter per promise. A coroutine written once over RoutineTraits runs on any
 * clocked token. Seconds convert at the registered rate; a caller holding a
 * scheduler converts with its seconds_to_units instead.
 */
template <Vruta::ProcessingToken Token>
struct RoutineTraits {
    using Routine = typename Vruta::TokenTraits<Token>::routine_type;
    using Promise = typename Routine::promise_type;
    using GetPromise = GetPromiseBase<Promise>;

    static auto delay(uint64_t count) { return DelayFor<Promise>::make(count); }
    static auto pause() { return delay(1); }

    static uint64_t units(double seconds)
    {
        if constexpr (std::same_as<typename Vruta::TokenTraits<Token>::clock_type, Vruta::FrameClock>) {
            return Vruta::seconds_to_frames(seconds);
        } else {
            return Vruta::seconds_to_samples(seconds);
        }
    }
};

}
