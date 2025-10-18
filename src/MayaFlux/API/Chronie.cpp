#include "Chronie.hpp"

#include "Core.hpp"

#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux {

MAYAFLUX_API std::shared_ptr<Vruta::TaskScheduler> get_scheduler()
{
    return get_context().get_scheduler();
}

MAYAFLUX_API std::shared_ptr<Vruta::EventManager> get_event_manager()
{
    return get_context().get_event_manager();
}

template <typename... Args>
bool update_task_params(const std::string& name, Args... args)
{
    return get_scheduler()->update_task_params(name, args...);
}

MAYAFLUX_API Vruta::SoundRoutine create_metro(double interval_seconds, std::function<void()> callback)
{
    return Kriya::metro(*get_scheduler(), interval_seconds, std::move(callback));
}

MAYAFLUX_API void schedule_metro(double interval_seconds, std::function<void()> callback, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "metro_" + std::to_string(scheduler->get_next_task_id());
    }
    auto metronome = std::make_shared<Vruta::SoundRoutine>(create_metro(interval_seconds, std::move(callback)));

    get_scheduler()->add_task(std::move(metronome), name, false);
}

MAYAFLUX_API Vruta::SoundRoutine create_sequence(std::vector<std::pair<double, std::function<void()>>> seq)
{
    return Kriya::sequence(*get_scheduler(), std::move(seq));
}

MAYAFLUX_API void schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "seq_" + std::to_string(scheduler->get_next_task_id());
    }
    auto tseq = std::make_shared<Vruta::SoundRoutine>(create_sequence(std::move(seq)));
    get_scheduler()->add_task(std::move(tseq), name, false);
}

MAYAFLUX_API Vruta::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop)
{
    return Kriya::line(*get_scheduler(), start_value, end_value, duration_seconds, step_duration, loop);
}

MAYAFLUX_API Vruta::SoundRoutine create_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds)
{
    return Kriya::pattern(*get_scheduler(), std::move(pattern_func), std::move(callback), interval_seconds);
}

MAYAFLUX_API void schedule_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "pattern_" + std::to_string(scheduler->get_next_task_id());
    }
    auto pattern = std::make_shared<Vruta::SoundRoutine>(create_pattern(std::move(pattern_func), std::move(callback), interval_seconds));
    get_scheduler()->add_task(std::move(pattern), name, false);
}

MAYAFLUX_API float* get_line_value(const std::string& name)
{
    std::string err;
    if (auto task = get_scheduler()->get_task(name)) {
        auto cur_val = task->get_state<float>("current_value");
        if (cur_val) {
            return cur_val;
        }

        std::cerr << "line value not returned from task. Verify that tasks has not returned" << '\n';
        return nullptr;
    }
    std::cerr << "Task: " << name << " not found. Verify task validity or if its been scheduled" << '\n';
    return nullptr;
}

MAYAFLUX_API void schedule_task(std::string name, Vruta::SoundRoutine&& task, bool initialize)
{
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(task);
    get_scheduler()->add_task(std::move(task_ptr), name, initialize);
}

MAYAFLUX_API bool cancel_task(const std::string& name)
{
    return get_scheduler()->cancel_task(name);
}

MAYAFLUX_API bool restart_task(const std::string& name)
{
    return get_scheduler()->restart_task(name);
}

MAYAFLUX_API Kriya::ActionToken Play(std::shared_ptr<Nodes::Node> node)
{
    return { std::move(node) };
}

MAYAFLUX_API Kriya::ActionToken Wait(double seconds)
{
    return { seconds };
}

MAYAFLUX_API Kriya::ActionToken Action(std::function<void()> func)
{
    return { std::move(func) };
}

}
