#pragma once

#include "Pattern.hpp"

#include "Awaiters/ConditionAwaiter.hpp"
#include "Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"
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

    template <typename Step>
    bool guarded(Step&& step)
    {
        try {
            step();
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

    template <typename Generator, typename Callback, typename Index>
    bool evaluate(Generator& generator, Callback& callback, const Index& index)
    {
        return guarded([&] { callback(generator(index)); });
    }

    inline std::function<bool()> after_one_pass(std::function<bool()> condition)
    {
        return [condition = std::move(condition), first = true]() mutable {
            if (first) {
                first = false;
                return false;
            }
            return condition();
        };
    }

    inline std::function<bool()> after_one_pass()
    {
        return after_one_pass([] { return true; });
    }

    template <Vruta::ProcessingToken Token>
    struct PatternDomain;

    template <>
    struct PatternDomain<Vruta::ProcessingToken::SAMPLE_ACCURATE> {
        using Routine = Vruta::SoundRoutine;
        using GetPromise = GetAudioPromise;

        static SampleDelay pause() { return SampleDelay { 1 }; }

        static uint64_t units(double seconds)
        {
            return seconds > 0.0 ? std::max<uint64_t>(1, Vruta::seconds_to_samples(seconds)) : 1;
        }

        static SampleDelay delay(uint64_t count) { return SampleDelay { count }; }
    };

    template <>
    struct PatternDomain<Vruta::ProcessingToken::FRAME_ACCURATE> {
        using Routine = Vruta::GraphicsRoutine;
        using GetPromise = GetGraphicsPromise;

        static FrameDelay pause() { return FrameDelay { .frames_to_wait = 1 }; }

        static uint64_t units(double seconds)
        {
            return seconds > 0.0 ? std::max<uint64_t>(1, Vruta::seconds_to_frames(seconds)) : 1;
        }

        static FrameDelay delay(uint64_t count) { return FrameDelay { .frames_to_wait = count }; }
    };

    template <>
    struct PatternDomain<Vruta::ProcessingToken::MULTI_RATE> {
        using Routine = Vruta::CrossRoutine;
        using GetPromise = GetCrossPromise;

        static MultiRateDelay pause() { return MultiRateDelay { .samples_to_wait = 1, .frames_to_wait = 0 }; }

        static uint64_t units(double seconds)
        {
            return seconds > 0.0 ? std::max<uint64_t>(1, Vruta::seconds_to_samples(seconds)) : 1;
        }

        static MultiRateDelay delay(uint64_t count) { return MultiRateDelay { .samples_to_wait = count, .frames_to_wait = 0 }; }
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

    class ListDurations {
    public:
        explicit ListDurations(std::vector<double> values)
            : m_values(std::move(values))
        {
        }

        std::optional<double> next()
        {
            if (m_values.empty()) {
                return std::nullopt;
            }
            const double value = m_values[m_position];
            m_position = (m_position + 1) % m_values.size();
            return value;
        }

    private:
        std::vector<double> m_values;
        size_t m_position { 0 };
    };

    template <typename Source>
    class FunctionDurations {
    public:
        explicit FunctionDurations(Source source)
            : m_source(std::move(source))
        {
        }

        std::optional<double> next()
        {
            if (m_position >= m_current.size()) {
                m_current.clear();
                m_position = 0;
                try {
                    for (const auto& value : m_source(m_pass)) {
                        m_current.push_back(static_cast<double>(value));
                    }
                } catch (const std::exception& e) {
                    MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                        "Exception in pattern durations: {}", e.what());
                    return std::nullopt;
                } catch (...) {
                    MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                        "Unknown exception in pattern durations");
                    return std::nullopt;
                }
                ++m_pass;
                if (m_current.empty()) {
                    return std::nullopt;
                }
            }
            return m_current[m_position++];
        }

    private:
        Source m_source;
        std::vector<double> m_current;
        size_t m_position { 0 };
        uint64_t m_pass { 0 };
    };

    class TimeMapDurations {
    public:
        explicit TimeMapDurations(Kinesis::TimeMap map)
            : m_map(std::move(map))
        {
        }

        std::optional<double> next()
        {
            std::optional<double> gap;
            if (!guarded([&] { gap = m_map(m_elapsed); })) {
                return std::nullopt;
            }
            m_elapsed += *gap > 0.0 ? *gap : 0.0;
            return gap;
        }

    private:
        Kinesis::TimeMap m_map;
        double m_elapsed { 0.0 };
    };

    template <typename Durations>
    bool durations_missing(const Durations& durations)
    {
        if constexpr (std::same_as<Durations, std::vector<double>>) {
            return durations.empty();
        } else if constexpr (std::same_as<Durations, Kinesis::TimeMap>) {
            return !durations.fn;
        } else {
            return is_unset(durations);
        }
    }

    template <typename Durations>
    auto make_durations(Durations durations)
    {
        if constexpr (std::same_as<Durations, std::vector<double>>) {
            return ListDurations(std::move(durations));
        } else if constexpr (std::same_as<Durations, Kinesis::TimeMap>) {
            return TimeMapDurations(std::move(durations));
        } else {
            return FunctionDurations<Durations>(std::move(durations));
        }
    }

    template <typename Generator, typename Index>
    class GeneratorValues {
    public:
        explicit GeneratorValues(Generator generator)
            : m_generator(std::move(generator))
        {
        }

        template <typename Callback>
        bool deliver(Callback& callback)
        {
            return evaluate(m_generator, callback, m_index++);
        }

    private:
        Generator m_generator;
        Index m_index {};
    };

    template <typename Range>
    class RangeValues {
    public:
        explicit RangeValues(Range range)
            : m_range(std::move(range))
            , m_it(std::ranges::begin(m_range))
        {
        }

        RangeValues(RangeValues&&) = delete;

        template <typename Callback>
        bool deliver(Callback& callback)
        {
            if (m_it == std::ranges::end(m_range)) {
                return false;
            }
            return guarded([&] {
                callback(*m_it);
                ++m_it;
            });
        }

    private:
        Range m_range;
        std::ranges::iterator_t<Range> m_it;
    };

    template <typename Range>
    class CyclingValues {
    public:
        explicit CyclingValues(Recurring<Range> recurring)
            : m_range(std::move(recurring.range))
            , m_it(std::ranges::begin(m_range))
        {
        }

        CyclingValues(CyclingValues&&) = delete;

        template <typename Callback>
        bool deliver(Callback& callback)
        {
            if (m_it == std::ranges::end(m_range)) {
                m_it = std::ranges::begin(m_range);
                if (m_it == std::ranges::end(m_range)) {
                    return false;
                }
            }
            return guarded([&] {
                callback(*m_it);
                ++m_it;
            });
        }

    private:
        Range m_range;
        std::ranges::iterator_t<Range> m_it;
    };

    template <typename Values, typename Index>
    struct CursorFor {
        using type = RangeValues<Values>;
    };

    template <typename Values, typename Index>
        requires PatternGenerator<Values, Index>
    struct CursorFor<Values, Index> {
        using type = GeneratorValues<Values, Index>;
    };

    template <typename Range, typename Index>
    struct CursorFor<Recurring<Range>, Index> {
        using type = CyclingValues<Range>;
    };

    template <typename Domain, typename Index, typename Values, typename Callback, typename Cursor>
    typename Domain::Routine timed_pattern(Values values, Callback callback, Cursor durations)
    {
        auto& promise = co_await typename Domain::GetPromise {};
        typename CursorFor<Values, Index>::type steps(std::move(values));

        while (!promise.should_terminate) {
            if (!steps.deliver(callback)) {
                co_return;
            }

            const auto seconds = durations.next();
            if (!seconds) {
                co_return;
            }

            co_await Domain::delay(Domain::units(*seconds));
        }
    }

    template <typename Index, typename Values, typename Callback, typename Cursor>
    Vruta::FreeRoutine free_timed_pattern(Values values, Callback callback, Cursor durations)
    {
        co_await ConditionAwaiter { after_one_pass() };

        typename CursorFor<Values, Index>::type steps(std::move(values));

        for (;;) {
            if (!steps.deliver(callback)) {
                co_return;
            }

            const auto seconds = durations.next();
            if (!seconds) {
                co_return;
            }

            const double clamped = *seconds > 0.0 ? std::min(*seconds, 1.0e6) : 0.0;
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(clamped));

            co_await ConditionAwaiter { after_one_pass([deadline] {
                return std::chrono::steady_clock::now() >= deadline;
            }) };
        }
    }

    template <typename Index, typename Values, typename Callback, typename Cursor>
    std::shared_ptr<Vruta::Routine> launch_timed(Values values, Callback callback, Cursor durations, Vruta::ProcessingToken token)
    {
        using Vruta::ProcessingToken;

        switch (token) {
        case ProcessingToken::SAMPLE_ACCURATE:
            return std::make_shared<Vruta::SoundRoutine>(
                timed_pattern<PatternDomain<ProcessingToken::SAMPLE_ACCURATE>, Index>(
                    std::move(values), std::move(callback), std::move(durations)));
        case ProcessingToken::FRAME_ACCURATE:
            return std::make_shared<Vruta::GraphicsRoutine>(
                timed_pattern<PatternDomain<ProcessingToken::FRAME_ACCURATE>, Index>(
                    std::move(values), std::move(callback), std::move(durations)));
        case ProcessingToken::MULTI_RATE:
            return std::make_shared<Vruta::CrossRoutine>(
                timed_pattern<PatternDomain<ProcessingToken::MULTI_RATE>, Index>(
                    std::move(values), std::move(callback), std::move(durations)));
        case ProcessingToken::CONDITIONAL:
            return std::make_shared<Vruta::FreeRoutine>(
                free_timed_pattern<Index>(std::move(values), std::move(callback), std::move(durations)));
        default:
            MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "pattern supports SAMPLE_ACCURATE, FRAME_ACCURATE, MULTI_RATE and CONDITIONAL tokens");
            return nullptr;
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

template <std::incrementable Index, typename Values, typename Callback, typename Durations>
    requires PatternValues<Values, Index>
    && PatternCallback<Callback, pattern_value_t<Values, Index>>
    && PatternDurations<Durations>
std::shared_ptr<Vruta::Routine> pattern(Values values, Callback callback, Durations durations, Vruta::ProcessingToken token)
{
    if (Detail::is_unset(values) || Detail::is_unset(callback) || Detail::durations_missing(durations)) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "pattern requires values, a callback and usable durations");
        return nullptr;
    }

    return Detail::launch_timed<Index>(
        std::move(values), std::move(callback), Detail::make_durations(std::move(durations)), token);
}

} // namespace MayaFlux::Kriya
