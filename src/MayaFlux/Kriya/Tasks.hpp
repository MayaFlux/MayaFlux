#pragma once

#include "config.h"

namespace MayaFlux::Vruta {
class TaskScheduler;
class SoundRoutine;
}

namespace MayaFlux::Kriya {

/**
 * @brief Creates a periodic event generator that executes a callback at regular intervals
 * @param scheduler The task scheduler that will manage this event generator
 * @param interval_seconds Time between callback executions in seconds
 * @param callback Function to execute on each interval
 * @return A SoundRoutine that implements the periodic behavior
 *
 * The metro task provides a fundamental temporal mechanism for creating
 * time-based structures in computational systems. It executes the provided
 * callback function at precise, regular intervals with sample-accurate timing,
 * enabling the creation of rhythmic patterns, event sequences, and temporal
 * frameworks that can synchronize across different domains.
 *
 * Unlike system timers which can drift due to processing load, this implementation
 * maintains precise timing by synchronizing with the sample-rate clock, making it
 * suitable for both audio and cross-domain applications where timing accuracy
 * is critical.
 *
 * Example usage:
 * ```cpp
 * // Create a periodic event generator (2Hz)
 * auto periodic_task = Kriya::metro(*scheduler, 0.5, []() {
 *     trigger_event(); // Could affect audio, visuals, data, etc.
 * });
 * scheduler->add_task(std::make_shared<SoundRoutine>(std::move(periodic_task)));
 * ```
 *
 * The metro task continues indefinitely until explicitly cancelled, creating
 * a persistent temporal structure within the computational system.
 */
Vruta::SoundRoutine metro(Vruta::TaskScheduler& scheduler, double interval_seconds, std::function<void()> callback);

/**
 * @brief Creates a temporal sequence that executes callbacks at specified time offsets
 * @param scheduler The task scheduler that will manage this sequence
 * @param sequence Vector of (time_offset, callback) pairs to execute in order
 * @return A SoundRoutine that implements the sequence behavior
 *
 * The sequence task enables the creation of precisely timed event chains with
 * specific temporal relationships. Each event consists of a time offset (in seconds)
 * and a callback function to execute at that precise moment.
 *
 * This mechanism is valuable for creating structured temporal progressions,
 * algorithmic sequences, or any series of time-based events that require
 * specific timing relationships. The sequence can coordinate events across
 * multiple domains (audio, visual, data) with sample-accurate precision.
 *
 * Example usage:
 * ```cpp
 * // Create a temporal sequence of events
 * auto event_sequence = Kriya::sequence(*scheduler, {
 *     {0.0, []() { trigger_event_a(); }},  // Immediate
 *     {0.5, []() { trigger_event_b(); }},  // 0.5 seconds later
 *     {1.0, []() { trigger_event_c(); }},  // 1.0 seconds later
 *     {1.5, []() { trigger_event_d(); }}   // 1.5 seconds later
 * });
 * scheduler->add_task(std::make_shared<SoundRoutine>(std::move(event_sequence)));
 * ```
 *
 * The sequence task completes after executing all events in the defined timeline.
 */
Vruta::SoundRoutine sequence(Vruta::TaskScheduler& scheduler, std::vector<std::pair<double, std::function<void()>>> sequence);

/**
 * @brief Creates a continuous interpolation generator between two values over time
 * @param scheduler The task scheduler that will manage this generator
 * @param start_value Initial value of the interpolation
 * @param end_value Final value of the interpolation
 * @param duration_seconds Total duration of the interpolation in seconds
 * @param step_duration Number of samples between value updates (default: 5)
 * @param restartable Whether the interpolation can be restarted after completion (default: false)
 * @return A SoundRoutine that implements the interpolation behavior
 *
 * The line task generates a linear interpolation between two numerical values
 * over a specified duration. This creates continuous, gradual transitions that
 * can be applied to any parameter in a computational system - from audio
 * parameters to visual properties, physical simulation values, or data
 * transformation coefficients.
 *
 * The current value of the interpolation is stored in the task's state under the key
 * "current_value" and can be accessed by external code using get_state<float>.
 *
 * Example usage:
 * ```cpp
 * // Create a 2-second interpolation from 0.0 to 1.0
 * auto transition = Kriya::line(*scheduler, 0.0f, 1.0f, 2.0f);
 * auto task_ptr = std::make_shared<SoundRoutine>(std::move(transition));
 * scheduler->add_task(task_ptr);
 *
 * // In the processing loop:
 * float* value = task_ptr->get_state<float>("current_value");
 * if (value) {
 *     // Apply to any parameter in any domain
 *     apply_parameter(*value);
 * }
 * ```
 *
 * If restartable is true, the interpolation task will remain active after reaching the
 * end value and can be restarted by calling restart() on the SoundRoutine.
 */
Vruta::SoundRoutine line(Vruta::TaskScheduler& scheduler, float start_value, float end_value, float duration_seconds, u_int32_t step_duration = 5, bool restartable = false);

/**
 * @brief Creates a generative algorithm that produces values based on a pattern function
 * @param scheduler The task scheduler that will manage this generator
 * @param pattern_func Function that generates values based on a step index
 * @param callback Function to execute with each generated value
 * @param interval_seconds Time between pattern steps in seconds
 * @return A SoundRoutine that implements the generative behavior
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
 * auto generator = Kriya::pattern(*scheduler,
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
 * scheduler->add_task(std::make_shared<SoundRoutine>(std::move(generator)));
 * ```
 *
 * The pattern task continues indefinitely until explicitly cancelled, creating
 * an ongoing generative process within the computational system.
 */
Vruta::SoundRoutine pattern(Vruta::TaskScheduler& scheduler, std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);
}
