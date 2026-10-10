#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Vruta {
class Routine;
}

namespace MayaFlux::Kriya {

/**
 * @brief Creates a generative algorithm that produces values based on a pattern function
 * @param pattern_func Function that generates values based on a step index
 * @param callback Function to execute with each generated value
 * @param interval_seconds Time between pattern steps in seconds
 * @return A Routine shared_ptr of type determined by the processing token of the scheduler (SoundRoutine, GraphicsRoutine, etc.)
 *
 * The pattern task provides a powerful framework for algorithmic generation
 * of values according to any computational pattern or rule system. At regular
 * intervals, it calls the pattern_func with the current step index, then passes
 * the returned value to the callback function.
 *
 * This mechanism enables the creation of generative algorithms, procedural
 * sequences, emergent behaviors, and rule-based systems that can influence
 * any aspect of a computational environment - from audio parameters to
 * visual elements, data transformations, or cross-domain mappings.
 *
 * Example usage:
 * ```cpp
 * // Create a generative algorithm based on a mathematical sequence
 * std::vector<int> fibonacci = {0, 1, 1, 2, 3, 5, 8, 13, 21};
 * auto generator = Kriya::pattern(
 *     // Pattern function - apply algorithmic rules
 *     [&fibonacci](uint64_t step) -> std::any {
 *         return fibonacci[step % fibonacci.size()];
 *     },
 *     // Callback - apply the generated value
 *     [](std::any value) {
 *         int result = std::any_cast<int>(value);
 *         // Can be applied to any domain - audio, visual, data, etc.
 *         apply_generated_value(result);
 *     },
 *     0.125 // Generate 8 values per second
 * );
 * scheduler->add_task(generator);
 * ```
 *
 * The pattern task continues indefinitely until explicitly cancelled, creating
 * an ongoing generative process within the computational system.
 */
MAYAFLUX_API std::shared_ptr<Vruta::Routine> pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

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
