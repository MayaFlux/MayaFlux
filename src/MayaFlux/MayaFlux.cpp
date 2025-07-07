#include "MayaFlux.hpp"

#include "Core/Engine.hpp"
#include "Kriya/Chain.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Component Access
//-------------------------------------------------------------------------

std::shared_ptr<Vruta::TaskScheduler> get_scheduler()
{
    return get_context().get_scheduler();
}

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

double get_uniform_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Utils::distribution::UNIFORM);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_gaussian_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Utils::distribution::NORMAL);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_exponential_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Utils::distribution::EXPONENTIAL);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_poisson_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Utils::distribution::POISSON);
    return get_context().get_random_engine()->random_sample(start, end);
}

//-------------------------------------------------------------------------
// Task Scheduling
//-------------------------------------------------------------------------

// template <typename... Args>
/* bool update_task_params(const std::string& name, Args... args)
{
    return get_context().update_task_params(name, args...);
} */

Vruta::SoundRoutine create_metro(double interval_seconds, std::function<void()> callback)
{
    return Kriya::metro(*get_scheduler(), interval_seconds, callback);
}

/* void schedule_metro(double interval_seconds, std::function<void()> callback, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "metro_" + std::to_string(scheduler->get_next_task_id());
    }
    auto metronome = Kriya::metro(*scheduler, interval_seconds, callback);

    get_context().schedule_task(name, std::move(metronome), false);
} */

Vruta::SoundRoutine create_sequence(std::vector<std::pair<double, std::function<void()>>> seq)
{
    return Kriya::sequence(*get_scheduler(), seq);
}

/* void schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "seq_" + std::to_string(scheduler->get_next_task_id());
    }
    auto tseq = Kriya::sequence(*scheduler, seq);
    get_context().schedule_task(name, std::move(tseq), false);
} */

Vruta::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop)
{
    return Kriya::line(*get_scheduler(), start_value, end_value, duration_seconds, step_duration, loop);
}

Vruta::SoundRoutine create_pattern(std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds)
{
    return Kriya::pattern(*get_scheduler(), pattern_func, callback, interval_seconds);
}

/* void schedule_pattern(std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "pattern_" + std::to_string(scheduler->get_next_task_id());
    }
    auto pattern = Kriya::pattern(*scheduler, pattern_func, callback, interval_seconds);
    get_context().schedule_task(name, std::move(pattern), false);
} */

/* float* get_line_value(const std::string& name)
{
    return get_context().get_line_value(name);
} */

/* std::function<float()> line_value(const std::string& name)
{
    return get_context().line_value(name);
} */

/* void schedule_task(std::string name, Vruta::SoundRoutine&& task, bool initialize)
{
    get_context().schedule_task(name, std::move(task), initialize);
} */

/* bool cancel_task(const std::string& name)
{
    return get_context().cancel_task(name);
} */

/* bool restart_task(const std::string& name)
{
    return get_context().restart_task(name);
} */

Kriya::ActionToken Play(std::shared_ptr<Nodes::Node> node)
{
    return Kriya::ActionToken(node);
}

Kriya::ActionToken Wait(double seconds)
{
    return Kriya::ActionToken(seconds);
}

Kriya::ActionToken Action(std::function<void()> func)
{
    return Kriya::ActionToken(func);
}

} // namespace MayaFlux
