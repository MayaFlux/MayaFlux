#include "Scheduler.hpp"

#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Vruta {

TaskScheduler::TaskScheduler(uint32_t default_sample_rate, uint32_t default_frame_rate)
    : m_clock(default_sample_rate)
    , m_cleanup_threshold(512)
{
    ensure_domain(ProcessingToken::SAMPLE_ACCURATE, default_sample_rate);
    ensure_domain(ProcessingToken::FRAME_ACCURATE, default_frame_rate);
    ensure_domain(ProcessingToken::MULTI_RATE, default_sample_rate);
    ensure_domain(ProcessingToken::ON_DEMAND, 1);
}

void TaskScheduler::add_task(std::shared_ptr<Routine> routine, const std::string& name, bool initialize)
{
    if (!routine) {
        std::cerr << "Failed to initiate routine\n;\t >> Exiting" << std::endl;
        return;
    }

    std::string task_name = name.empty() ? auto_generate_name(routine) : name;
    ProcessingToken token = routine->get_processing_token();

    {
        auto existing_it = find_task_by_name(task_name);
        if (existing_it != m_tasks.end()) {
            if (existing_it->routine && existing_it->routine->is_active()) {
                existing_it->routine->set_should_terminate(true);
            }
            m_tasks.erase(existing_it);
        }

        m_tasks.emplace_back(routine, task_name);
    }

    if (initialize) {
        ensure_domain(token);
        initialize_routine_state(routine, token);
    } else {
        ensure_domain(token);
    }
}

bool TaskScheduler::cancel_task(const std::string& name)
{
    auto it = find_task_by_name(name);
    if (it != m_tasks.end()) {
        if (it->routine && it->routine->is_active()) {
            it->routine->set_should_terminate(true);
        }
        m_tasks.erase(it);
        return true;
    }
    return false;
}

bool TaskScheduler::cancel_task(std::shared_ptr<Routine> routine)
{
    auto it = find_task_by_routine(routine);
    if (it != m_tasks.end()) {
        if (routine && routine->is_active()) {
            routine->set_should_terminate(true);
        }
        m_tasks.erase(it);
        return true;
    }
    return false;
}

bool TaskScheduler::restart_task(const std::string& name)
{
    auto it = find_task_by_name(name);
    if (it != m_tasks.end()) {
        if (it->routine && it->routine->is_active()) {
            it->routine->restart();
        }
    }
    return false;
}

std::shared_ptr<Routine> TaskScheduler::get_task(const std::string& name) const
{
    auto it = find_task_by_name(name);
    return (it != m_tasks.end()) ? it->routine : nullptr;
}

std::vector<std::shared_ptr<Routine>> TaskScheduler::get_tasks_for_token(ProcessingToken token) const
{
    std::vector<std::shared_ptr<Routine>> result;
    for (const auto& entry : m_tasks) {
        if (entry.routine && entry.routine->get_processing_token() == token) {
            result.push_back(entry.routine);
        }
    }
    return result;
}

void TaskScheduler::process_token(ProcessingToken token, uint64_t processing_units)
{
    auto processor_it = m_token_processors.find(token);
    if (processor_it != m_token_processors.end()) {
        auto tasks = get_tasks_for_token(token);
        processor_it->second(tasks, processing_units);
    } else {
        process_default(token, processing_units);
    }

    static uint64_t cleanup_counter = 0;
    if (++cleanup_counter % (m_cleanup_threshold * 2) == 0) {
        cleanup_completed_tasks();
    }
}

void TaskScheduler::process_all_tokens()
{
    for (const auto& token : m_token_clocks) {
        process_token(token.first, 0);
    }

    static uint64_t cleanup_counter = 0;
    if (++cleanup_counter % m_cleanup_threshold == 0) {
        cleanup_completed_tasks();
    }
}

void TaskScheduler::register_token_processor(ProcessingToken token, token_processing_func_t processor)
{
    ensure_domain(token);
    m_token_processors[token] = std::move(processor);
}

const IClock& TaskScheduler::get_clock(ProcessingToken token) const
{
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

uint64_t TaskScheduler::seconds_to_units(double seconds, ProcessingToken token) const
{
    unsigned int rate = get_rate(token);
    return Utils::seconds_to_units(seconds, rate);
}

uint64_t TaskScheduler::seconds_to_samples(double seconds) const
{
    return Utils::seconds_to_samples(seconds, get_rate(ProcessingToken::SAMPLE_ACCURATE));
}

uint64_t TaskScheduler::current_units(ProcessingToken token) const
{
    const auto& clock = get_clock(token);
    return clock.current_position();
}

unsigned int TaskScheduler::get_rate(ProcessingToken token) const
{
    auto clock_it = m_token_clocks.find(token);
    if (clock_it != m_token_clocks.end()) {
        return clock_it->second->rate();
    }

    return get_default_rate(token);
}

bool TaskScheduler::has_active_tasks(ProcessingToken token) const
{
    return std::any_of(m_tasks.begin(), m_tasks.end(),
        [token](const TaskEntry& entry) {
            return entry.routine && entry.routine->is_active() && entry.routine->get_processing_token() == token;
        });
}

uint64_t TaskScheduler::get_next_task_id() const
{
    return m_next_task_id.fetch_add(1);
}

std::string TaskScheduler::auto_generate_name(std::shared_ptr<Routine> routine) const
{
    return "task_" + std::to_string(get_next_task_id());
}

std::vector<TaskEntry>::iterator TaskScheduler::find_task_by_name(const std::string& name)
{
    return std::find_if(m_tasks.begin(), m_tasks.end(),
        [&name](const TaskEntry& entry) { return entry.name == name; });
}

std::vector<TaskEntry>::const_iterator TaskScheduler::find_task_by_name(const std::string& name) const
{
    return std::find_if(m_tasks.begin(), m_tasks.end(),
        [&name](const TaskEntry& entry) { return entry.name == name; });
}

std::vector<TaskEntry>::iterator TaskScheduler::find_task_by_routine(std::shared_ptr<Routine> routine)
{
    return std::find_if(m_tasks.begin(), m_tasks.end(),
        [&routine](const TaskEntry& entry) { return entry.routine == routine; });
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

void TaskScheduler::ensure_domain(ProcessingToken token, unsigned int rate)
{
    auto clock_it = m_token_clocks.find(token);
    if (clock_it == m_token_clocks.end()) {
        unsigned int domain_rate = (rate > 0) ? rate : get_default_rate(token);

        switch (token) {
        case ProcessingToken::SAMPLE_ACCURATE:
            m_token_clocks[token] = std::make_unique<SampleClock>(domain_rate);
            break;
        case ProcessingToken::FRAME_ACCURATE:
            m_token_clocks[token] = std::make_unique<SampleClock>(domain_rate);
            break;
        case ProcessingToken::ON_DEMAND:
            m_token_clocks[token] = std::make_unique<SampleClock>(domain_rate);
            break;
        default:
            m_token_clocks[token] = std::make_unique<SampleClock>(domain_rate);
            break;
        }
    }
}

void TaskScheduler::process_default(ProcessingToken token, uint64_t processing_units)
{
    auto clock_it = m_token_clocks.find(token);
    if (clock_it == m_token_clocks.end()) {
        return;
    }

    auto tasks = get_tasks_for_token(token);
    if (tasks.empty()) {
        auto& clock = *clock_it->second;
        clock.tick(processing_units);
        return;
    }

    auto& clock = *clock_it->second;

    for (uint64_t i = 0; i < processing_units; i++) {
        uint64_t current_context = clock.current_position();

        for (auto& routine : tasks) {
            if (routine && routine->is_active()) {
                if (routine->requires_clock_sync()) {
                    if (current_context >= routine->next_execution()) {
                        // routine->try_resume(current_context);
                        routine->try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
                    }
                } else {
                    // routine->try_resume(current_context);
                    routine->try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
                }
            }
        }

        clock.tick(1);
    }
}

void TaskScheduler::cleanup_completed_tasks()
{
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const TaskEntry& entry) {
                return !entry.routine || !entry.routine->is_active();
            }),
        m_tasks.end());
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

    uint64_t current_context = clock_it->second->current_position();
    return routine->initialize_state(current_context);
}

void TaskScheduler::pause_all_tasks()
{
    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            bool current_auto_resume = entry.routine->get_auto_resume();
            entry.routine->set_state<bool>("was_auto_resume", current_auto_resume);
            entry.routine->set_auto_resume(false);
        }
    }
}

void TaskScheduler::resume_all_tasks()
{
    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            bool* was_auto_resume = entry.routine->get_state<bool>("was_auto_resume");
            if (was_auto_resume) {
                entry.routine->set_auto_resume(*was_auto_resume);
            } else {
                entry.routine->set_auto_resume(true);
            }
        }
    }
}

void TaskScheduler::terminate_all_tasks()
{
    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            entry.routine->set_should_terminate(true);
            entry.routine->set_auto_resume(true);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    m_tasks.clear();
}

void TaskScheduler::process_buffer_cycle_tasks()
{
    m_current_buffer_cycle++;
    auto tasks = get_tasks_for_token(ProcessingToken::SAMPLE_ACCURATE);

    for (auto& task : tasks) {
        if (task && task->is_active()) {
            task->try_resume_with_context(m_current_buffer_cycle, DelayContext::BUFFER_BASED);
        }
    }
}

}
