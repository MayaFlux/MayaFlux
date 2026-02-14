#pragma once

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux {

namespace Core {
    class Window;
}

namespace Nodes {
    class Node;
}

namespace Vruta {
    class TaskScheduler;
    class EventManager;
    class SoundRoutine;
}

namespace Kriya {
    class BufferPipeline;
}

/**
 * @brief Gets the task scheduler from the default engine
 * @return Shared pointer to the centrally managed TaskScheduler
 *
 * Returns the scheduler that's managed by the default engine instance.
 * All scheduled tasks using the convenience functions will use this scheduler.
 */
MAYAFLUX_API std::shared_ptr<Vruta::TaskScheduler> get_scheduler();

/**
 * @brief Gets the event manager from the default engine
 * @return Shared pointer to the centrally managed EventManager
 *
 * Returns the event manager that's managed by the default engine instance.
 * Used for handling windowing and input events.
 */
MAYAFLUX_API std::shared_ptr<Vruta::EventManager> get_event_manager();

/**
 * @brief Creates a simple task that calls a function at a specified interval
 * @param interval_seconds Time between calls in seconds
 * @param callback Function to call on each tick
 * This is conceptually similar to Metronomes in PureData and MaxMSP
 */
MAYAFLUX_API Vruta::SoundRoutine create_metro(double interval_seconds, std::function<void()> callback);

/**
 * @brief Creates a metronome task and addes it to the default scheduler list for evaluation
 * @param interval_seconds Time between calls in seconds
 * @param callback Function to call on each tick
 * @param name Name of the metronome task (optional but recommended).
     If not provided, a default name will be generated.
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API void schedule_metro(double interval_seconds, std::function<void()> callback, std::string name = "");

/**
 * @brief Creates a sequence task that calls functions at specified times
 * @param sequence Vector of (time, function) pairs
 * @param name Name of the metronome task (optional but recommended).
     If not provided, a default name will be generated.
 * @return SoundRoutine object representing the scheduled task
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API Vruta::SoundRoutine create_sequence(std::vector<std::pair<double, std::function<void()>>> sequence);

/**
 * @brief Creates a sequence task that calls functions at specified times
 * and addes it to the default scheduler list for evaluation
 * @param sequence Vector of (time, function) pairs
 * @param name Name of the metronome task (optional but recommended).
     If not provided, a default name will be generated.
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API void schedule_sequence(std::vector<std::pair<double, std::function<void()>>> sequence, std::string name = "");

/**
 * @brief Creates a line generator that interpolates between values over time
 * @param start_value Starting value
 * @param end_value Ending value
 * @param duration_seconds Total duration in seconds
 * @param step_duration Time between steps in seconds
 * @param loop Whether to loop back to start after reaching end
 * @return SoundRoutine object representing the line generator
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API Vruta::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, uint32_t step_duration, bool loop);

/**
 * @brief Schedules a pattern generator that produces values based on a pattern function
 * @param pattern_func Function that generates pattern values based on step index
 * @param callback Function to call with each pattern value
 * @param interval_seconds Time between pattern steps
 * @return SoundRoutine object representing the scheduled task
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API Vruta::SoundRoutine create_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);

/**
 * @brief Schedules a pattern generator that produces values based on a pattern function
 * and addes it to the default scheduler list for evaluation
 * @param pattern_func Function that generates pattern values based on step index
 * @param callback Function to call with each pattern value
 * @param interval_seconds Time between pattern steps
 * @param name Name of the metronome task (optional but recommended).
     If not provided, a default name will be generated.
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API void schedule_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, std::string name = "");

/**
 * @brief Gets a pointer to a task's current value
 * @param name Name of the task
 * @return Pointer to the float value, or nullptr if not found
 *
 * Convenience wrapper for Engine::get_line_value() on the default engine.
 */
MAYAFLUX_API float* get_line_value(const std::string& name);

/**
 * @brief Schedules a new sound routine task
 * @param name Unique name for the task
 * @param task The sound routine to schedule
 * @param initialize Whether to initialize the task immediately
 *
 * Convenience wrapper for Engine::schedule_task() on the default engine.
 */
MAYAFLUX_API void schedule_task(const std::string& name, Vruta::SoundRoutine&& task, bool initialize = false);

/**
 * @brief Cancels a scheduled task
 * @param name Name of the task to cancel
 * @return true if task was found and canceled, false otherwise
 *
 * Convenience wrapper for Engine::cancel_task() on the default engine.
 */
MAYAFLUX_API bool cancel_task(const std::string& name);

/**
 * @brief Restarts a scheduled task
 * @param name Name of the task to restart
 * @return true if task was found and restarted, false otherwise
 *
 * Convenience wrapper for Engine::restart_task() on the default engine.
 */
MAYAFLUX_API bool restart_task(const std::string& name);

/**
 * @brief Updates parameters of a scheduled task
 * @tparam Args Parameter types
 * @param name Name of the task to update
 * @param args New parameter values
 * @return true if task was found and updated, false otherwise
 *
 * Convenience wrapper for Engine::update_task_params() on the default engine.
 */
template <typename... Args>
MAYAFLUX_API bool update_task_params(const std::string& name, Args... args);

/**
 * @brief Creates a new buffer pipeline instance
 * @return Shared pointer to the created BufferPipeline
 *
 * Uses the task scheduler from the default engine.
 */
MAYAFLUX_API std::shared_ptr<Kriya::BufferPipeline> create_buffer_pipeline();

/**
 * @brief Schedule a key press handler
 * @param window Window to listen to
 * @param key Key to wait for
 * @param callback Function to call on key press
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_key_pressed(window, MayaFlux::IO::Keys::Escape, []() {
 *     // Handle Escape key press
 * }, "escape_handler");
 * @endcode
 */
MAYAFLUX_API void on_key_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::Keys key,
    std::function<void()> callback,
    std::string name = "");

/**
 * @brief Schedule a key release handler
 * @param window Window to listen to
 * @param key Key to wait for
 * @param callback Function to call on key release
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_key_released(window, MayaFlux::IO::Keys::Enter, []() {
 *     // Handle Enter key release
 * }, "enter_release_handler");
 * @endcode
 */
MAYAFLUX_API void on_key_released(
    const std::shared_ptr<Core::Window>& window,
    IO::Keys key,
    std::function<void()> callback,
    std::string name = "");

/**
 * @brief Schedule a handler for any key press
 * @param window Window to listen to
 * @param callback Function to call with key code when any key is pressed
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_any_key(window, [](MayaFlux::IO::Keys key) {
 *     // Handle any key press, key code in 'key'
 * }, "any_key_handler");
 * @endcode
 */
MAYAFLUX_API void on_any_key(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(IO::Keys)> callback,
    std::string name = "");

/**
 * @brief Schedule a mouse button press handler
 * @param window Window to listen to
 * @param button Mouse button to wait for
 * @param callback Function to call on button press (x, y)
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_mouse_pressed(window, MayaFlux::IO::MouseButtons::Left, [](double x, double y) {
 *     // Handle left mouse button press at (x, y)
 * }, "mouse_left_press_handler");
 * @endcode
 */
MAYAFLUX_API void on_mouse_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name = "");

/**
 * @brief Schedule a mouse button release handler
 * @param window Window to listen to
 * @param button Mouse button to wait for
 * @param callback Function to call on button release (x, y)
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_mouse_released(window, MayaFlux::IO::MouseButtons::Right, [](double x, double y) {
 *     // Handle right mouse button release at (x, y)
 * }, "mouse_right_release_handler");
 * @endcode
 */
MAYAFLUX_API void on_mouse_released(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name = "");

/**
 * @brief Schedule a mouse movement handler
 * @param window Window to listen to
 * @param callback Function to call on mouse move (x, y)
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_mouse_move(window, [](double x, double y) {
 *     // Handle mouse move at (x, y)
 * }, "mouse_move_handler");
 * @endcode
 */
MAYAFLUX_API void on_mouse_move(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name = "");

/**
 * @brief Schedule a mouse scroll handler
 * @param window Window to listen to
 * @param callback Function to call on scroll (xoffset, yoffset)
 * @param name Optional name for the event handler
 *
 * Example:
 * @code
 * MayaFlux::on_scroll(window, [](double xoffset, double yoffset) {
 *     // Handle mouse scroll with offsets
 * }, "mouse_scroll_handler");
 * @endcode
 */
MAYAFLUX_API void on_scroll(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name = "");

/**
 * @brief Cancel an event handler by name
 * @param name Event handler name
 * @return True if cancelled successfully
 */
MAYAFLUX_API bool cancel_event_handler(const std::string& name);

/**
 * @brief Converts a time duration in seconds to the equivalent number of audio samples
 * @param seconds Time duration in seconds
 * @return Equivalent number of audio samples based on the current sample rate
 *
 * This function uses the sample rate from the default engine to calculate how many
 * audio samples correspond to the given time duration in seconds. It's useful for
 * scheduling tasks or processing buffers based on time intervals.
 */
MAYAFLUX_API uint64_t seconds_to_samples(double seconds);

}
