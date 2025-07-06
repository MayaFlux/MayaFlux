#include "Scheduler.hpp"

#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Vruta {

TaskScheduler::TaskScheduler(u_int32_t default_sample_rate, u_int32_t default_frame_rate)
    : m_clock(default_sample_rate)
{
    ensure_token_domain(ProcessingToken::SAMPLE_ACCURATE, default_sample_rate);
    ensure_token_domain(ProcessingToken::FRAME_ACCURATE, default_frame_rate);
    ensure_token_domain(ProcessingToken::MULTI_RATE, default_sample_rate);
    ensure_token_domain(ProcessingToken::ON_DEMAND, 1);
}

void TaskScheduler::add_task(std::shared_ptr<Routine> routine, bool initialize)
{
    if (!routine) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    ProcessingToken token = routine->get_processing_token();

    ensure_token_domain(token);

    m_token_tasks[token].push_back(routine);

    if (initialize) {
        initialize_routine_state(routine, token);
    }
}

void TaskScheduler::add_task(std::shared_ptr<SoundRoutine> task, bool initialize)
{
    add_task(std::static_pointer_cast<Routine>(task), initialize);
}

bool TaskScheduler::cancel_task(std::shared_ptr<Routine> routine)
{
    if (!routine) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    ProcessingToken token = routine->get_processing_token();
    auto token_it = m_token_tasks.find(token);

    if (token_it == m_token_tasks.end()) {
        return false;
    }

    auto& tasks = token_it->second;
    auto it = std::find(tasks.begin(), tasks.end(), routine);

    if (it != tasks.end()) {
        if (routine->is_active()) {
            routine->set_state("should_terminate", true);
            routine->set_state("auto_resume", true);
        }

        tasks.erase(it);
        return true;
    }

    return false;
}

bool TaskScheduler::cancel_task(std::shared_ptr<SoundRoutine> task)
{
    return cancel_task(std::static_pointer_cast<Routine>(task));
}

void TaskScheduler::process_token(ProcessingToken token, u_int64_t processing_units)
{
    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    auto token_it = m_token_tasks.find(token);
    if (token_it == m_token_tasks.end() || token_it->second.empty()) {
        return;
    }

    auto processor_it = m_token_processors.find(token);
    if (processor_it != m_token_processors.end()) {
        processor_it->second(token_it->second, processing_units);
    } else {
        process_token_default(token, processing_units);
    }

    cleanup_completed_tasks(token);
}

void TaskScheduler::process_all_tokens()
{
    for (const auto& [token, tasks] : m_token_tasks) {
        if (!tasks.empty()) {
            process_token(token, 1);
        }
    }
}

void TaskScheduler::register_token_processor(
    ProcessingToken token,
    std::function<void(const std::vector<std::shared_ptr<Routine>>&, u_int64_t)> processor)
{
    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    ensure_token_domain(token);
    m_token_processors[token] = std::move(processor);
}

void TaskScheduler::process_sample()
{
    process_token(ProcessingToken::SAMPLE_ACCURATE, 1);
}

void TaskScheduler::process_buffer(unsigned int buffer_size)
{
    process_token(ProcessingToken::SAMPLE_ACCURATE, buffer_size);
}

const IClock& TaskScheduler::get_clock(ProcessingToken token) const
{
    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    auto clock_it = m_token_clocks.find(token);
    if (clock_it != m_token_clocks.end()) {
        return *clock_it->second;
    }

    auto audio_clock_it = m_token_clocks.find(ProcessingToken::SAMPLE_ACCURATE);
    if (audio_clock_it != m_token_clocks.end()) {
        return *audio_clock_it->second;
    }

    throw std::runtime_error("No clocks available in scheduler");
}

u_int64_t TaskScheduler::seconds_to_units(double seconds, ProcessingToken token) const
{
    unsigned int rate = get_rate(token);
    return Utils::seconds_to_units(seconds, rate);
}

u_int64_t TaskScheduler::seconds_to_samples(double seconds) const
{
    return Utils::seconds_to_samples(seconds, get_rate(ProcessingToken::SAMPLE_ACCURATE));
}

u_int64_t TaskScheduler::current_units(ProcessingToken token) const
{
    const auto& clock = get_clock(token);
    return clock.current_position();
}

unsigned int TaskScheduler::get_rate(ProcessingToken token) const
{
    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    auto clock_it = m_token_clocks.find(token);
    if (clock_it != m_token_clocks.end()) {
        return clock_it->second->rate();
    }

    return get_default_rate(token);
}

const std::vector<std::shared_ptr<Routine>>& TaskScheduler::get_tasks(ProcessingToken token) const
{
    std::lock_guard<std::mutex> lock(m_scheduler_mutex);

    auto token_it = m_token_tasks.find(token);
    if (token_it != m_token_tasks.end()) {
        return token_it->second;
    }

    // Return empty vector if token not found
    static const std::vector<std::shared_ptr<Routine>> empty_tasks;
    return empty_tasks;
}

std::vector<std::shared_ptr<SoundRoutine>> TaskScheduler::get_audio_tasks() const
{
    std::vector<std::shared_ptr<SoundRoutine>> audio_tasks;

    const auto& routines = get_tasks(ProcessingToken::SAMPLE_ACCURATE);
    for (const auto& routine : routines) {
        if (auto sound_routine = std::dynamic_pointer_cast<SoundRoutine>(routine)) {
            audio_tasks.push_back(sound_routine);
        }
    }

    return audio_tasks;
}

bool TaskScheduler::has_active_tasks(ProcessingToken token) const
{
    const auto& tasks = get_tasks(token);
    return std::any_of(tasks.begin(), tasks.end(),
        [](const std::shared_ptr<Routine>& routine) {
            return routine && routine->is_active();
        });
}

const u_int64_t TaskScheduler::get_next_task_id() const
{
    if (m_tasks.size()) {
        return m_tasks.size() + 1;
    }
    return 1;
}

void TaskScheduler::ensure_token_domain(ProcessingToken token, unsigned int rate)
{
    if (m_token_clocks.find(token) != m_token_clocks.end()) {
        return;
    }

    if (rate == 0) {
        rate = get_default_rate(token);
    }

    std::unique_ptr<IClock> clock;
    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
    case ProcessingToken::MULTI_RATE: // Use sample clock as primary for multi-rate
        clock = std::make_unique<SampleClock>(rate);
        break;

    case ProcessingToken::FRAME_ACCURATE:
        clock = std::make_unique<FrameClock>(rate);
        break;

    case ProcessingToken::ON_DEMAND:
    case ProcessingToken::CUSTOM:
    default:
        clock = std::make_unique<CustomClock>(rate, "units");
        break;
    }

    m_token_clocks[token] = std::move(clock);
    m_token_rates[token] = rate;

    m_token_tasks[token] = std::vector<std::shared_ptr<Routine>>();
}

unsigned int TaskScheduler::get_default_rate(ProcessingToken token) const
{
    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
        return 48000;
    case ProcessingToken::FRAME_ACCURATE:
        return 60;
    case ProcessingToken::MULTI_RATE:
        return 48000;
    case ProcessingToken::ON_DEMAND:
        return 1;
    case ProcessingToken::CUSTOM:
        return 1000;
    default:
        return 48000;
    }
}

void TaskScheduler::process_token_default(ProcessingToken token, u_int64_t processing_units)
{
    auto clock_it = m_token_clocks.find(token);
    auto tasks_it = m_token_tasks.find(token);

    if (clock_it == m_token_clocks.end() || tasks_it == m_token_tasks.end()) {
        return;
    }

    auto& clock = *clock_it->second;
    auto& tasks = tasks_it->second;

    clock.tick(processing_units);
    u_int64_t current_context = clock.current_position();

    for (auto& routine : tasks) {
        if (routine && routine->is_active()) {
            if (routine->requires_clock_sync()) {
                if (current_context >= routine->next_execution()) {
                    routine->try_resume(current_context);
                }
            } else {
                routine->try_resume(current_context);
            }
        }
    }
}

void TaskScheduler::cleanup_completed_tasks(ProcessingToken token)
{
    auto tasks_it = m_token_tasks.find(token);
    if (tasks_it == m_token_tasks.end()) {
        return;
    }

    auto& tasks = tasks_it->second;

    tasks.erase(
        std::remove_if(tasks.begin(), tasks.end(),
            [](const std::shared_ptr<Routine>& routine) {
                return !routine || !routine->is_active();
            }),
        tasks.end());
}

bool TaskScheduler::initialize_routine_state(std::shared_ptr<Routine> routine, ProcessingToken token)
{
    if (!routine) {
        return false;
    }

    auto clock_it = m_token_clocks.find(token);
    if (clock_it == m_token_clocks.end()) {
        return false;
    }

    u_int64_t current_context = clock_it->second->current_position();
    return routine->initialize_state(current_context);
}

}
