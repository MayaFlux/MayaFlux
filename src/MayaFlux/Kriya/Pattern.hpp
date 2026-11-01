#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Vruta {
class Routine;
}

namespace MayaFlux::Kriya {

/**
 * @concept PatternGenerator
 * @brief A callable from a step index to a value
 */
template <typename Generator, typename Index>
concept PatternGenerator = std::move_constructible<std::decay_t<Generator>>
    && std::invocable<std::decay_t<Generator>&, Index>
    && !std::is_void_v<std::invoke_result_t<std::decay_t<Generator>&, Index>>;

/**
 * @struct PatternTraits
 * @brief Types a pattern derives from its generator and index
 */
template <typename Generator, typename Index>
struct PatternTraits {
    using index_type = Index;
    using generator_type = std::decay_t<Generator>;
    using value_type = std::invoke_result_t<generator_type&, Index>;
};

/**
 * @struct Recurring
 * @brief Marks a range whose values start again from the first when it ends
 */
template <typename Range>
struct Recurring {
    using range_type = Range;
    Range range;
};

/**
 * @brief Wraps a range so a pattern repeats it instead of ending with it
 * @param range Any forward range, copied or moved into the result
 * @return The range marked as recurring
 *
 * Its length is independent of the durations, so the two drift against each other.
 * An empty range ends the pattern at once.
 */
template <typename Range>
    requires std::ranges::forward_range<std::decay_t<Range>>
Recurring<std::decay_t<Range>> recurring(Range&& range)
{
    return { std::forward<Range>(range) };
}

template <typename Values>
inline constexpr bool is_recurring_v = false;

template <typename Range>
inline constexpr bool is_recurring_v<Recurring<Range>> = true;

/**
 * @concept PatternValues
 * @brief What a pattern draws its values from: an index generator, a range, or a recurring range
 */
template <typename Values, typename Index>
concept PatternValues = PatternGenerator<Values, Index>
    || std::ranges::input_range<std::decay_t<Values>>
    || (is_recurring_v<std::decay_t<Values>>
        && std::ranges::forward_range<typename std::decay_t<Values>::range_type>);

namespace Detail {

    template <typename Values, typename Index>
    auto pattern_value_identity()
    {
        using Decayed = std::decay_t<Values>;
        if constexpr (PatternGenerator<Decayed, Index>) {
            return std::type_identity<std::invoke_result_t<Decayed&, Index>> {};
        } else if constexpr (is_recurring_v<Decayed>) {
            return std::type_identity<std::ranges::range_reference_t<typename Decayed::range_type>> {};
        } else {
            return std::type_identity<std::ranges::range_reference_t<Decayed>> {};
        }
    }

} // namespace Detail

/**
 * @brief What the callback receives: the generator's result, or a reference to the range element
 */
template <typename Values, typename Index>
using pattern_value_t = typename decltype(Detail::pattern_value_identity<Values, Index>())::type;

/**
 * @concept PatternCallback
 * @brief A callable that accepts what the values produce
 */
template <typename Callback, typename Value>
concept PatternCallback = std::move_constructible<std::decay_t<Callback>>
    && std::invocable<std::decay_t<Callback>&, Value>;

/**
 * @concept PatternCondition
 * @brief A predicate on the step index
 */
template <typename Condition, typename Index>
concept PatternCondition = std::move_constructible<std::decay_t<Condition>>
    && std::predicate<std::decay_t<Condition>&, Index>;

/**
 * @concept PatternDurationSource
 * @brief A callable from a pass number to the array of durations for that pass
 */
template <typename Source>
concept PatternDurationSource = std::move_constructible<std::decay_t<Source>>
    && std::invocable<std::decay_t<Source>&, uint64_t>
    && std::ranges::input_range<std::invoke_result_t<std::decay_t<Source>&, uint64_t>>
    && std::convertible_to<std::ranges::range_value_t<std::invoke_result_t<std::decay_t<Source>&, uint64_t>>, double>;

/**
 * @brief Creates a pattern whose next step runs when a condition holds
 * @tparam Index Step index type, default uint64_t
 * @param generator Callable from the step index to any value
 * @param callback Callable that takes the generated value
 * @param ready Condition for the step index, checked each time the routine resumes
 * @param token Selects the routine type that runs the pattern
 * @return The routine, or nullptr for an empty function or an unsupported token
 *
 * The routine suspends and checks `ready(index)` again until it holds, then runs
 * `callback(generator(index))` and advances the index. The value type is
 * whatever the generator returns, so no cast is involved. The condition can be
 * anything that returns a bool: a Logic node's output, a flag, another routine's
 * state. A condition that stays true runs one step per check.
 *
 * Tokens:
 * - SAMPLE_ACCURATE: SoundRoutine
 * - FRAME_ACCURATE: GraphicsRoutine
 * - MULTI_RATE: CrossRoutine
 * - CONDITIONAL: FreeRoutine on the condition thread
 *
 * A function that throws is logged and ends the pattern.
 *
 * ```cpp
 * auto routine = Kriya::pattern(
 *     [](uint64_t step) { return glm::vec2(step, step * 2); },
 *     [](glm::vec2 point) { use(point); },
 *     [gate](uint64_t) { return gate->get_last_output() > 0.5; });
 * scheduler->add_task(routine);
 * ```
 */
template <std::incrementable Index = uint64_t, typename Generator, typename Callback, typename Condition>
    requires PatternGenerator<Generator, Index>
    && PatternCallback<Callback, typename PatternTraits<Generator, Index>::value_type>
    && PatternCondition<Condition, Index>
std::shared_ptr<Vruta::Routine> pattern(
    Generator generator,
    Callback callback,
    Condition ready,
    Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

/**
 * @brief Creates a pattern whose steps are spaced by a list of durations
 * @tparam Index Step index type, default uint64_t
 * @param values Callable from the step index to any value, a range played once, or `recurring(range)`
 * @param callback Callable that takes the generated value, or the range element by reference
 * @param durations Seconds to wait after each step, stepped through in order and repeated
 * @param token Selects the routine type that runs the pattern
 * @return The routine, or nullptr for an empty function, an empty list or an unsupported token
 *
 * The first step runs at once. After step `i` the routine waits `durations[i % size]`, rounded
 * to the routine's own units and never less than one unit. A single interval is a list of one:
 * `pattern(gen, cb, { 0.125 })`. A range ends the pattern after its last element and that
 * step's wait; a recurring range starts again, so its length and the list's drift apart. A throw
 * from the generator, the range or the callback is logged and ends the pattern.
 *
 * ```cpp
 * auto routine = Kriya::pattern(generate, apply, { 0.25, 0.25, 0.5 });
 * auto phrase = Kriya::pattern(std::vector { 60, 62, 64, 67 }, play, { 0.25, 0.5 });
 * auto drift = Kriya::pattern(Kriya::recurring(std::vector { 1, 2, 3, 4, 5 }), play, { 0.25, 0.25, 0.5 });
 * scheduler->add_task(routine);
 * ```
 */
template <std::incrementable Index = uint64_t, typename Values, typename Callback>
    requires PatternValues<Values, Index>
    && PatternCallback<Callback, pattern_value_t<Values, Index>>
std::shared_ptr<Vruta::Routine> pattern(
    Values values,
    Callback callback,
    std::vector<double> durations,
    Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

/**
 * @brief Creates a pattern whose durations come from a function that returns an array per pass
 * @tparam Index Step index type, default uint64_t
 * @param values Callable from the step index to any value, a range played once, or `recurring(range)`
 * @param callback Callable that takes the generated value, or the range element by reference
 * @param durations Callable from a pass number to a range of seconds
 * @param token Selects the routine type that runs the pattern
 * @return The routine, or nullptr for an empty function or an unsupported token
 *
 * The function is called with 0 for the first pass and again, with the next number, each time
 * the array it returned has been used up. Durations are stepped through one per step. An empty
 * array, or a throw from the function, ends the pattern after the step that asked for it. This
 * lets the gaps change from pass to pass.
 *
 * ```cpp
 * auto routine = Kriya::pattern(generate, apply, [](uint64_t pass) -> std::vector<double> {
 *     if (pass > 3) return {};
 *     return { 0.25, 0.25 * (pass + 1) };
 * });
 * ```
 */
template <std::incrementable Index = uint64_t, typename Values, typename Callback, typename Source>
    requires PatternValues<Values, Index>
    && PatternCallback<Callback, pattern_value_t<Values, Index>>
    && PatternDurationSource<Source>
std::shared_ptr<Vruta::Routine> pattern(
    Values values,
    Callback callback,
    Source durations,
    Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

/**
 * @brief The std::any, uint64_t form of the condition driven pattern
 *
 * Compiled once in the library. Chosen when the three arguments are
 * std::function objects of exactly these types; lambdas select the template.
 */
MAYAFLUX_API std::shared_ptr<Vruta::Routine> pattern(
    std::function<std::any(uint64_t)> pattern_func,
    std::function<void(std::any)> callback,
    std::function<bool(uint64_t)> ready,
    Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

} // namespace MayaFlux::Kriya

#include "Pattern.inl"
