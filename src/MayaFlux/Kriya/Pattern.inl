#pragma once

#include "Pattern.hpp"

#include "Awaiters/ConditionAwaiter.hpp"
#include "Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/Routine.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace Detail {

    template <typename T>
    struct is_std_function : std::false_type { };

    template <typename Signature>
    struct is_std_function<std::function<Signature>> : std::true_type { };

    template <typename F>
    bool is_unset(const F& function)
    {
        if constexpr (is_std_function<F>::value) {
            return !function;
        } else {
            return false;
        }
    }

    template <typename Condition, typename Index>
    bool satisfied(Condition& ready, const Index& index, bool& failed)
    {
        try {
            return static_cast<bool>(ready(index));
        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Exception in pattern condition: {}", e.what());
        } catch (...) {
            MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Unknown exception in pattern condition");
        }
        failed = true;
        return true;
    }

    template <typename Generator, typename Callback, typename Index>
    bool evaluate(Generator& generator, Callback& callback, const Index& index)
    {
        try {
            callback(generator(index));
            return true;
        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Exception in pattern step: {}", e.what());
        } catch (...) {
            MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Unknown exception in pattern step");
        }
        return false;
    }

    inline std::function<bool()> after_one_pass()
    {
        return [first = true]() mutable {
            const bool hold = first;
            first = false;
            return !hold;
        };
    }

    template <Vruta::ProcessingToken Token>
    struct PatternDomain;

    template <>
    struct PatternDomain<Vruta::ProcessingToken::SAMPLE_ACCURATE> {
        using Routine = Vruta::SoundRoutine;
        using GetPromise = GetAudioPromise;

        static SampleDelay pause() { return SampleDelay { 1 }; }
    };

    template <>
    struct PatternDomain<Vruta::ProcessingToken::FRAME_ACCURATE> {
        using Routine = Vruta::GraphicsRoutine;
        using GetPromise = GetGraphicsPromise;

        static FrameDelay pause() { return FrameDelay { .frames_to_wait = 1 }; }
    };

    template <>
    struct PatternDomain<Vruta::ProcessingToken::MULTI_RATE> {
        using Routine = Vruta::CrossRoutine;
        using GetPromise = GetCrossPromise;

        static MultiRateDelay pause() { return MultiRateDelay { .samples_to_wait = 1, .frames_to_wait = 0 }; }
    };

    template <typename Domain, typename Index, typename Generator, typename Callback, typename Condition>
    typename Domain::Routine checked_pattern(Generator generator, Callback callback, Condition ready)
    {
        auto& promise = co_await typename Domain::GetPromise {};
        bool failed = false;
        Index index {};

        for (; !promise.should_terminate; ++index) {
            while (!promise.should_terminate && !satisfied(ready, index, failed)) {
                co_await Domain::pause();
            }

            if (promise.should_terminate || failed || !evaluate(generator, callback, index)) {
                co_return;
            }

            co_await Domain::pause();
        }
    }

    template <typename Index, typename Generator, typename Callback, typename Condition>
    Vruta::FreeRoutine free_pattern(Generator generator, Callback callback, Condition ready)
    {
        co_await ConditionAwaiter { after_one_pass() };

        bool failed = false;
        Index index {};

        for (;; ++index) {
            co_await ConditionAwaiter { [&] { return satisfied(ready, index, failed); } };

            if (failed || !evaluate(generator, callback, index)) {
                co_return;
            }

            co_await ConditionAwaiter { after_one_pass() };
        }
    }

} // namespace Detail

template <std::incrementable Index, typename Generator, typename Callback, typename Condition>
    requires PatternGenerator<Generator, Index>
    && PatternCallback<Callback, typename PatternTraits<Generator, Index>::value_type>
    && PatternCondition<Condition, Index>
std::shared_ptr<Vruta::Routine> pattern(Generator generator, Callback callback, Condition ready, Vruta::ProcessingToken token)
{
    using Vruta::ProcessingToken;

    if (Detail::is_unset(generator) || Detail::is_unset(callback) || Detail::is_unset(ready)) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "pattern requires a generator, a callback and a condition");
        return nullptr;
    }

    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
        return std::make_shared<Vruta::SoundRoutine>(
            Detail::checked_pattern<Detail::PatternDomain<ProcessingToken::SAMPLE_ACCURATE>, Index>(
                std::move(generator), std::move(callback), std::move(ready)));
    case ProcessingToken::FRAME_ACCURATE:
        return std::make_shared<Vruta::GraphicsRoutine>(
            Detail::checked_pattern<Detail::PatternDomain<ProcessingToken::FRAME_ACCURATE>, Index>(
                std::move(generator), std::move(callback), std::move(ready)));
    case ProcessingToken::MULTI_RATE:
        return std::make_shared<Vruta::CrossRoutine>(
            Detail::checked_pattern<Detail::PatternDomain<ProcessingToken::MULTI_RATE>, Index>(
                std::move(generator), std::move(callback), std::move(ready)));
    case ProcessingToken::CONDITIONAL:
        return std::make_shared<Vruta::FreeRoutine>(
            Detail::free_pattern<Index>(std::move(generator), std::move(callback), std::move(ready)));
    default:
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "pattern supports SAMPLE_ACCURATE, FRAME_ACCURATE, MULTI_RATE and CONDITIONAL tokens");
        return nullptr;
    }
}

} // namespace MayaFlux::Kriya
