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
 * @concept PatternCallback
 * @brief A callable that accepts what the generator returns
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
 * @param generator Callable from the step index to any value
 * @param callback Callable that takes the generated value
 * @param durations Seconds to wait after each step, stepped through in order and repeated
 * @param token Selects the routine type that runs the pattern
 * @return The routine, or nullptr for an empty function, an empty list or an unsupported token
 *
 * The first step runs at once. After step `i` the routine waits `durations[i % size]`, rounded
 * to the routine's own units and never less than one unit. A single interval is a list of one:
 * `pattern(gen, cb, { 0.125 })`. A throw from the generator or callback is logged and ends the
 * pattern.
 *
 * ```cpp
 * auto routine = Kriya::pattern(generate, apply, { 0.25, 0.25, 0.5 });
 * scheduler->add_task(routine);
 * ```
 */
template <std::incrementable Index = uint64_t, typename Generator, typename Callback>
    requires PatternGenerator<Generator, Index>
    && PatternCallback<Callback, typename PatternTraits<Generator, Index>::value_type>
std::shared_ptr<Vruta::Routine> pattern(
    Generator generator,
    Callback callback,
    std::vector<double> durations,
    Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

/**
 * @brief Creates a pattern whose durations come from a function that returns an array per pass
 * @tparam Index Step index type, default uint64_t
 * @param generator Callable from the step index to any value
 * @param callback Callable that takes the generated value
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
template <std::incrementable Index = uint64_t, typename Generator, typename Callback, typename Source>
    requires PatternGenerator<Generator, Index>
    && PatternCallback<Callback, typename PatternTraits<Generator, Index>::value_type>
    && PatternDurationSource<Source>
std::shared_ptr<Vruta::Routine> pattern(
    Generator generator,
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
