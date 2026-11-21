#pragma once

#include "Clock.hpp"
#include "Routine.hpp"

namespace MayaFlux::Vruta {

/**
 * @struct TokenTraits
 * @brief What a ProcessingToken means at compile time
 *
 * `routine_type` is the routine class that carries the token, void when none does.
 * `clock_type` is the clock the scheduler creates for the token, void when it has none.
 * The primary template is the scheduler's default: a SampleClock and no routine class.
 */
template <ProcessingToken Token>
struct TokenTraits {
    using routine_type = void;
    using clock_type = SampleClock;
};

template <>
struct TokenTraits<ProcessingToken::SAMPLE_ACCURATE> {
    using routine_type = SoundRoutine;
    using clock_type = SampleClock;
};

template <>
struct TokenTraits<ProcessingToken::FRAME_ACCURATE> {
    using routine_type = GraphicsRoutine;
    using clock_type = FrameClock;
};

template <>
struct TokenTraits<ProcessingToken::MULTI_RATE> {
    using routine_type = CrossRoutine;
    using clock_type = SampleClock;
};

template <>
struct TokenTraits<ProcessingToken::CONDITIONAL> {
    using routine_type = FreeRoutine;
    using clock_type = void;
};

/**
 * @brief Turn a runtime token into a compile-time one
 * @param token The token to dispatch on
 * @param visitor Called with `std::integral_constant<ProcessingToken, token>`; every call must return the same type
 * @return What the visitor returns
 *
 * A token with no entry of its own reaches the visitor as ON_DEMAND, which uses the primary TokenTraits.
 */
template <typename Visitor>
auto visit_token(ProcessingToken token, Visitor&& visitor)
{
    using Constant = std::integral_constant<ProcessingToken, ProcessingToken::ON_DEMAND>;

    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
        return std::forward<Visitor>(visitor)(std::integral_constant<ProcessingToken, ProcessingToken::SAMPLE_ACCURATE> {});
    case ProcessingToken::FRAME_ACCURATE:
        return std::forward<Visitor>(visitor)(std::integral_constant<ProcessingToken, ProcessingToken::FRAME_ACCURATE> {});
    case ProcessingToken::MULTI_RATE:
        return std::forward<Visitor>(visitor)(std::integral_constant<ProcessingToken, ProcessingToken::MULTI_RATE> {});
    case ProcessingToken::CONDITIONAL:
        return std::forward<Visitor>(visitor)(std::integral_constant<ProcessingToken, ProcessingToken::CONDITIONAL> {});
    default:
        return std::forward<Visitor>(visitor)(Constant {});
    }
}

}
