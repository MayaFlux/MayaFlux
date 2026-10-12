#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

#include "Pattern.hpp"

namespace MayaFlux {
namespace Vruta {
    class TaskScheduler;
    class SoundRoutine;
    class GraphicsRoutine;
    class Routine;
}

namespace Nodes::Generator {
    class Logic;
}

namespace Kriya {

    /**
     * @brief Creates a periodic event generator that executes a callback at regular intervals
     * @param interval_seconds Time between callback executions in seconds
     * @param callback Function to execute on each interval
     * @param token Processing token to determine which scheduler rate to use (default: SAMPLE_ACCURATE)
     * @return A Routine shared_ptr of type determined by the processing token of the scheduler (SoundRoutine, GraphicsRoutine, etc.)
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
     * auto periodic_task = Kriya::metro(0.5, []() {
     *     trigger_event(); // Could affect audio, visuals, data, etc.
     * });
     * scheduler->add_task(periodic_task);
     * ```
     *
     * The metro task continues indefinitely until explicitly cancelled, creating
     * a persistent temporal structure within the computational system.
     */
    MAYAFLUX_API std::shared_ptr<Vruta::Routine> metro(double interval_seconds, std::function<void()> callback, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Creates a temporal sequence that executes callbacks at specified time offsets
     * @param scheduler The task scheduler that will manage this sequence
     * @param sequence Vector of (time_offset, callback) pairs to execute in order
     * @param token Processing token to determine which scheduler rate to use (default: SAMPLE_ACCURATE)
     * @return A Routine shared_ptr of type determined by the processing token of the scheduler (SoundRoutine, GraphicsRoutine, etc.)
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
     * auto event_sequence = Kriya::sequence({
     *     {0.0, []() { trigger_event_a(); }},  // Immediate
     *     {0.5, []() { trigger_event_b(); }},  // 0.5 seconds later
     *     {1.0, []() { trigger_event_c(); }},  // 1.0 seconds later
     *     {1.5, []() { trigger_event_d(); }}   // 1.5 seconds later
     * });
     * scheduler->add_task(event_sequence);
     * ```
     *
     * The sequence task completes after executing all events in the defined timeline.
     */
    MAYAFLUX_API std::shared_ptr<Vruta::Routine> sequence(std::vector<std::pair<double, std::function<void()>>> sequence, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Creates a continuous interpolation generator between two values over time
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
    MAYAFLUX_API Vruta::SoundRoutine line(float start_value, float end_value, float duration_seconds, uint32_t step_duration = 5, bool restartable = false);

    /**
     * @brief Coroutine that executes callback continuously while logic node outputs true
     * @param callback Function to execute while condition is true
     * @param logic_node Logic node to monitor (creates default threshold node if null)
     * @param open Whether to subscribe to gate open (true) or close (false)
     * @return SoundRoutine coroutine handle
     * @deprecated Register Logic::while_true (or while_false) on the node instead.
     * Scheduled for removal in the next release.
     */
    [[deprecated("Use Logic::while_true or Logic::while_false on the node; removal planned for the next release")]]
    MAYAFLUX_API Vruta::SoundRoutine Gate(
        std::function<void()> callback,
        std::shared_ptr<Nodes::Generator::Logic> logic_node, bool open = true);

    /**
     * @brief Coroutine that executes callback when logic node changes to specific state
     * @param logic_node Logic node to monitor (creates default threshold node if null)
     * @param target_state State to trigger on (true/false)
     * @param callback Function to execute on state change
     * @return SoundRoutine coroutine handle
     * @deprecated Register Logic::on_change_to on the node instead.
     * Scheduled for removal in the next release.
     */
    [[deprecated("Use Logic::on_change_to on the node; removal planned for the next release")]]
    MAYAFLUX_API Vruta::SoundRoutine Trigger(
        bool target_state,
        std::function<void()> callback,
        std::shared_ptr<Nodes::Generator::Logic> logic_node);

    /**
     * @brief Coroutine that executes callback on any logic node state change
     * @param logic_node Logic node to monitor (creates default threshold node if null)
     * @param callback Function to execute on any state flip
     * @return SoundRoutine coroutine handle
     * @deprecated Register Logic::on_change on the node instead.
     * Scheduled for removal in the next release.
     */
    [[deprecated("Use Logic::on_change on the node; removal planned for the next release")]]
    MAYAFLUX_API Vruta::SoundRoutine Toggle(
        std::function<void()> callback,
        std::shared_ptr<Nodes::Generator::Logic> logic_node);
}
}
