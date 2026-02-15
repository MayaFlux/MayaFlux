#include "Chronie.hpp"

#include "Core.hpp"

#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Kriya/BufferPipeline.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/Kriya/InputEvents.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

std::shared_ptr<Vruta::TaskScheduler> get_scheduler()
{
    return get_context().get_scheduler();
}

std::shared_ptr<Vruta::EventManager> get_event_manager()
{
    return get_context().get_event_manager();
}

template <typename... Args>
bool update_task_params(const std::string& name, Args... args)
{
    return get_scheduler()->update_task_params(name, args...);
}

Vruta::SoundRoutine create_metro(double interval_seconds, std::function<void()> callback)
{
    return Kriya::metro(*get_scheduler(), interval_seconds, std::move(callback));
}

void schedule_metro(double interval_seconds, std::function<void()> callback, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "metro_" + std::to_string(scheduler->get_next_task_id());
    }
    auto metronome = std::make_shared<Vruta::SoundRoutine>(create_metro(interval_seconds, std::move(callback)));

    get_scheduler()->add_task(std::move(metronome), name, false);
}

Vruta::SoundRoutine create_sequence(std::vector<std::pair<double, std::function<void()>>> seq)
{
    return Kriya::sequence(*get_scheduler(), std::move(seq));
}

void schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "seq_" + std::to_string(scheduler->get_next_task_id());
    }
    auto tseq = std::make_shared<Vruta::SoundRoutine>(create_sequence(std::move(seq)));
    get_scheduler()->add_task(std::move(tseq), name, false);
}

Vruta::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, uint32_t step_duration, bool loop)
{
    return Kriya::line(*get_scheduler(), start_value, end_value, duration_seconds, step_duration, loop);
}

Vruta::SoundRoutine create_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds)
{
    return Kriya::pattern(*get_scheduler(), std::move(pattern_func), std::move(callback), interval_seconds);
}

void schedule_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "pattern_" + std::to_string(scheduler->get_next_task_id());
    }
    auto pattern = std::make_shared<Vruta::SoundRoutine>(create_pattern(std::move(pattern_func), std::move(callback), interval_seconds));
    get_scheduler()->add_task(std::move(pattern), name, false);
}

float* get_line_value(const std::string& name)
{
    if (auto task = get_scheduler()->get_task(name)) {
        auto cur_val = task->get_state<float>("current_value");
        if (cur_val) {
            return cur_val;
        }

        MF_ERROR(Journal::Component::API, Journal::Context::CoroutineScheduling, "line value not returned from task. Verify that tasks has not returned");
        return nullptr;
    }
    MF_ERROR(Journal::Component::API, Journal::Context::CoroutineScheduling, "Task: {} not found. Verify task validity or if its been scheduled", name);
    return nullptr;
}

void schedule_task(const std::string& name, Vruta::SoundRoutine&& task, bool initialize)
{
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(task));
    get_scheduler()->add_task(std::move(task_ptr), name, initialize);
}

bool cancel_task(const std::string& name)
{
    return get_scheduler()->cancel_task(name);
}

bool restart_task(const std::string& name)
{
    return get_scheduler()->restart_task(name);
}

std::shared_ptr<Kriya::BufferPipeline> create_buffer_pipeline()
{
    return Kriya::BufferPipeline::create(*get_scheduler(), get_context().get_buffer_manager());
}

void on_key_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::Keys key,
    std::function<void()> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "key_press_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::key_pressed(window, key, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_key_released(
    const std::shared_ptr<Core::Window>& window,
    IO::Keys key,
    std::function<void()> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "key_release_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::key_released(window, key, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_any_key(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(IO::Keys)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "any_key_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::any_key(window, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_press_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_pressed(window, button, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_released(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_release_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_released(window, button, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_move(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_move_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_moved(window, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_scroll(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "scroll_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_scrolled(window, std::move(callback)));

    event_manager->add_event(event, name);
}

bool cancel_event_handler(const std::string& name)
{
    return get_event_manager()->cancel_event(name);
}

uint64_t seconds_to_samples(double seconds)
{
    uint64_t sample_rate = 48000;
    if (get_context().is_running()) {
        sample_rate = get_context().get_stream_info().sample_rate;
    }
    return static_cast<uint64_t>(seconds * sample_rate);
}

uint64_t seconds_to_blocks(double seconds)
{
    uint32_t sample_rate = 48000;
    uint32_t block_size = 512;

    if (get_context().is_running()) {
        sample_rate = get_context().get_stream_info().sample_rate;
        block_size = get_context().get_stream_info().buffer_size;
    }

    return Vruta::seconds_to_blocks(seconds, sample_rate, block_size);
}

uint64_t samples_to_blocks(uint64_t samples)
{
    uint32_t block_size = 512;

    if (get_context().is_running()) {
        block_size = get_context().get_stream_info().buffer_size;
    }

    return Vruta::samples_to_blocks(samples, block_size);
}

}
