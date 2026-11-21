#include "Scheduler.hpp"

#include "ChronUtils.hpp"
#include "TokenTraits.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <stop_token>

namespace MayaFlux::Vruta {

namespace {

    /**
     * @brief Counts a submission call as in flight and reports whether it may proceed
     *
     * The count is entered before the terminating flag is read, so
     * terminate_all_tasks can wait for every accepted call to finish.
     */
    struct SubmissionGuard {
        SubmissionGuard(std::atomic<uint32_t>& inflight, const std::atomic<bool>& terminating)
            : m_inflight(inflight)
        {
            m_inflight.fetch_add(1);
            accepted = !terminating.load();
        }

        ~SubmissionGuard() { m_inflight.fetch_sub(1); }

        SubmissionGuard(const SubmissionGuard&) = delete;
        SubmissionGuard& operator=(const SubmissionGuard&) = delete;

        bool accepted;

    private:
        std::atomic<uint32_t>& m_inflight;
    };

    /**
     * @brief Scheduler whose queue drain is running on this thread, if any
     *
     * Completion callbacks fire inside the drains while they hold iterators
     * into the task lists, so terminate_all_tasks must not run from them.
     */
    thread_local const TaskScheduler* t_draining = nullptr;

    struct DrainScope {
        explicit DrainScope(const TaskScheduler* scheduler)
            : m_previous(t_draining)
        {
            t_draining = scheduler;
        }

        ~DrainScope() { t_draining = m_previous; }

        DrainScope(const DrainScope&) = delete;
        DrainScope& operator=(const DrainScope&) = delete;

    private:
        const TaskScheduler* m_previous;
    };

    /**
     * @brief Clears the terminating flag on every exit, including exceptions
     */
    struct TerminationScope {
        explicit TerminationScope(std::atomic<bool>& flag)
            : m_flag(flag)
        {
        }

        ~TerminationScope() { m_flag.store(false); }

        TerminationScope(const TerminationScope&) = delete;
        TerminationScope& operator=(const TerminationScope&) = delete;

    private:
        std::atomic<bool>& m_flag;
    };

}

TaskScheduler::TaskScheduler(uint32_t default_sample_rate, uint32_t default_frame_rate)
    : m_clock(default_sample_rate)
    , m_cleanup_threshold(512)
    , m_registered_sample_rate(default_sample_rate)
    , m_registered_frame_rate(default_frame_rate)
{
    s_registered_sample_rate = default_sample_rate;
    s_registered_frame_rate = default_frame_rate;
    ensure_domain(ProcessingToken::SAMPLE_ACCURATE, default_sample_rate);
    ensure_domain(ProcessingToken::FRAME_ACCURATE, default_frame_rate);
    ensure_domain(ProcessingToken::MULTI_RATE, default_sample_rate);
    ensure_domain(ProcessingToken::ON_DEMAND, 1);
    m_workers[0] = std::make_unique<ConditionalWorker>("default");
}

void TaskScheduler::add_task(const std::shared_ptr<Routine>& routine, const std::string& name, bool initialize)
{
    if (!routine) {
        MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling, "Failed to initiate routine; routine is null. Exiting add_task.");
        return;
    }

    SubmissionGuard guard(m_inflight_submissions, m_terminating);
    if (!guard.accepted) {
        MF_WARN(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "Rejected task '{}': scheduler is terminating", name);
        return;
    }

    std::string task_name = name.empty() ? auto_generate_name(routine) : name;
    ProcessingToken token = routine->get_processing_token();

    ensure_domain(token);

    if (initialize)
        initialize_routine_state(routine, token);

    if (token == ProcessingToken::CONDITIONAL) {
        routine->set_auto_resume(true);

        WorkerId requested;
        if (const auto* free_routine = dynamic_cast<const FreeRoutine*>(routine.get())) {
            requested = free_routine->worker();
        }

        auto* worker = worker_at(requested.index);
        if (!worker) {
            MF_WARN(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
                "Task '{}' asked for worker {} which does not exist; using the default worker",
                task_name, requested.index);
            worker = worker_at(0);
        }

        if (worker->pending.push({ TaskEntry { routine, task_name }, true })) {
            start_conditional_thread(*worker);
            worker->wake.notify();
            return;
        }
        MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "Conditional pending queue full, could not add task '{}'", task_name);
        return;
    }

    if (m_pending_ops.push({ TaskEntry { routine, task_name }, true })) {
        return;
    }

    MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
        "Pending task queue full, could not add task '{}'", task_name);
}

bool TaskScheduler::cancel_task(const std::string& name)
{
    SubmissionGuard guard(m_inflight_submissions, m_terminating);
    if (!guard.accepted)
        return false;

    bool queued = false;

    for_each_worker([&](ConditionalWorker& worker) {
        if (worker.thread.joinable() && worker.pending.push({ TaskEntry { nullptr, name }, false })) {
            worker.wake.notify();
            queued = true;
        }
    });

    if (m_pending_ops.push({ TaskEntry { nullptr, name }, false })) {
        queued = true;
    }

    if (!queued) {
        MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "Pending task queue full, could not cancel task '{}'", name);
    }

    return queued;
}

bool TaskScheduler::cancel_task(const std::shared_ptr<Routine>& routine)
{
    if (!routine)
        return false;

    SubmissionGuard guard(m_inflight_submissions, m_terminating);
    if (!guard.accepted)
        return false;

    if (routine->get_processing_token() == ProcessingToken::CONDITIONAL) {
        std::string name;
        for_each_worker([&](ConditionalWorker& worker) {
            const auto it = std::ranges::find_if(worker.tasks,
                [&routine](const TaskEntry& e) { return e.routine == routine; });
            if (it != worker.tasks.end()) {
                name = it->name;
            }
        });
        return cancel_task(name);
    }

    auto it = find_task_by_routine(routine);
    const std::string name = (it != m_tasks.end()) ? it->name : "";

    if (m_pending_ops.push({ TaskEntry { routine, name }, false })) {
        return true;
    }

    MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
        "Pending task queue full, could not cancel task '{}'", name);
    return false;
}

bool TaskScheduler::restart_task(const std::string& name)
{
    SubmissionGuard guard(m_inflight_submissions, m_terminating);
    if (!guard.accepted)
        return false;

    bool restarted = false;
    for_each_worker([&](ConditionalWorker& worker) {
        const auto cit = std::ranges::find_if(worker.tasks,
            [&name](const TaskEntry& e) { return e.name == name; });
        if (cit != worker.tasks.end()) {
            if (cit->routine && cit->routine->is_active())
                cit->routine->restart();
            restarted = true;
        }
    });
    if (restarted) {
        return true;
    }

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
    std::shared_ptr<Routine> found;
    for_each_worker([&](ConditionalWorker& worker) {
        const auto cit = std::ranges::find_if(worker.tasks,
            [&name](const TaskEntry& e) { return e.name == name; });
        if (cit != worker.tasks.end() && !found)
            found = cit->routine;
    });
    if (found)
        return found;

    auto it = find_task_by_name(name);
    return (it != m_tasks.end()) ? it->routine : nullptr;
}

std::vector<std::shared_ptr<Routine>> TaskScheduler::get_tasks_for_token(ProcessingToken token) const
{
    if (token == ProcessingToken::CONDITIONAL) {
        std::vector<std::shared_ptr<Routine>> result;
        for_each_worker([&](ConditionalWorker& worker) {
            for (const auto& e : worker.tasks) {
                if (e.routine)
                    result.push_back(e.routine);
            }
        });
        return result;
    }

    std::vector<std::shared_ptr<Routine>> result;
    for (const auto& entry : m_tasks) {
        if (entry.routine && entry.routine->get_processing_token() == token) {
            result.push_back(entry.routine);
        }
    }
    return result;
}

std::vector<std::string> TaskScheduler::get_task_names() const
{
    std::vector<std::string> names;
    for (const auto& entry : m_tasks) {
        if (entry.routine)
            names.push_back(entry.name);
    }
    return names;
}

std::vector<std::string> TaskScheduler::get_task_names(ProcessingToken token) const
{
    if (token == ProcessingToken::CONDITIONAL) {
        std::vector<std::string> names;
        for_each_worker([&](ConditionalWorker& worker) {
            for (const auto& e : worker.tasks) {
                if (e.routine)
                    names.push_back(e.name);
            }
        });
        return names;
    }

    std::vector<std::string> names;
    for (const auto& entry : m_tasks) {
        if (entry.routine && entry.routine->get_processing_token() == token)
            names.push_back(entry.name);
    }
    return names;
}

const std::vector<TaskEntry>& TaskScheduler::get_all_tasks()
{
    drain_pending_tasks();
    return m_tasks;
}

void TaskScheduler::process_token(ProcessingToken token, uint64_t processing_units)
{
    drain_pending_tasks();

    auto processor_it = m_token_processors.find(token);
    if (processor_it != m_token_processors.end()) {
        auto tasks = get_tasks_for_token(token);
        processor_it->second(tasks, processing_units);
    } else {
        process_default(token, processing_units);
    }

    if (token == ProcessingToken::SAMPLE_ACCURATE) {
        pump_cross(DelayContext::SAMPLE_BASED, ProcessingToken::SAMPLE_ACCURATE, processing_units);
    } else if (token == ProcessingToken::FRAME_ACCURATE) {
        pump_cross(DelayContext::FRAME_BASED, ProcessingToken::FRAME_ACCURATE, processing_units);
    }

    static uint64_t cleanup_counter = 0;
    if (++cleanup_counter % (static_cast<uint64_t>(m_cleanup_threshold * 2)) == 0) {
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

void TaskScheduler::register_clock(ProcessingToken token, std::shared_ptr<IClock> clock)
{
    ensure_domain(token);
    m_token_clocks[token] = std::move(clock);
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

    error<std::runtime_error>(Journal::Component::Vruta, Journal::Context::CoroutineScheduling, std::source_location::current(),
        "No clock found for token {}, and no sample-accurate clock available as fallback", static_cast<int>(token));
}

uint64_t TaskScheduler::seconds_to_units(double seconds, ProcessingToken token) const
{
    return Vruta::seconds_to_units(seconds, get_rate(token));
}

uint64_t TaskScheduler::seconds_to_samples(double seconds) const
{
    return Vruta::seconds_to_units(seconds, get_rate(ProcessingToken::SAMPLE_ACCURATE));
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
    return std::ranges::any_of(m_tasks,
        [token](const TaskEntry& entry) {
            return entry.routine && entry.routine->is_active() && entry.routine->get_processing_token() == token;
        });
}

uint64_t TaskScheduler::get_next_task_id() const
{
    return m_next_task_id.fetch_add(1);
}

std::string TaskScheduler::auto_generate_name(const std::shared_ptr<Routine>& /*routine*/) const
{
    return "task_" + std::to_string(get_next_task_id());
}

std::vector<TaskEntry>::iterator TaskScheduler::find_task_by_name(const std::string& name)
{
    return std::ranges::find_if(m_tasks,
        [&name](const TaskEntry& entry) { return entry.name == name; });
}

std::vector<TaskEntry>::const_iterator TaskScheduler::find_task_by_name(const std::string& name) const
{
    return std::ranges::find_if(m_tasks,
        [&name](const TaskEntry& entry) { return entry.name == name; });
}

std::vector<TaskEntry>::iterator TaskScheduler::find_task_by_routine(const std::shared_ptr<Routine>& routine)
{
    return std::ranges::find_if(m_tasks,
        [&routine](const TaskEntry& entry) { return entry.routine == routine; });
}

unsigned int TaskScheduler::get_default_rate(ProcessingToken token) const
{
    switch (token) {
    case ProcessingToken::SAMPLE_ACCURATE:
        return m_registered_sample_rate;
    case ProcessingToken::FRAME_ACCURATE:
        return m_registered_frame_rate;
    case ProcessingToken::MULTI_RATE:
        return m_registered_sample_rate;
    case ProcessingToken::ON_DEMAND:
        return 1;
    case ProcessingToken::CUSTOM:
        return 1000;
    case ProcessingToken::CONDITIONAL:
        return 0;
    default:
        return m_registered_sample_rate;
    }
}

void TaskScheduler::ensure_domain(ProcessingToken token, unsigned int rate)
{
    if (token == ProcessingToken::CONDITIONAL)
        return;

    auto clock_it = m_token_clocks.find(token);
    if (clock_it == m_token_clocks.end()) {
        unsigned int domain_rate = (rate > 0) ? rate : get_default_rate(token);

        m_token_clocks[token] = visit_token(token, [domain_rate](auto tag) -> std::shared_ptr<IClock> {
            using Clock = typename TokenTraits<decltype(tag)::value>::clock_type;
            if constexpr (std::is_void_v<Clock>) {
                return nullptr;
            } else {
                return std::make_shared<Clock>(domain_rate);
            }
        });
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
                        routine->try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
                    }
                } else {
                    routine->try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
                }
            }
        }

        clock.tick(1);
    }
}

void TaskScheduler::cleanup_completed_tasks()
{
    std::erase_if(m_tasks, [](const TaskEntry& entry) {
        return !entry.routine || !entry.routine->is_active();
    });
}

bool TaskScheduler::initialize_routine_state(const std::shared_ptr<Routine>& routine, ProcessingToken token)
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

void TaskScheduler::start_conditional_thread(ConditionalWorker& worker)
{
    if (worker.thread.joinable())
        return;

    worker.thread = std::jthread([this, &worker](const std::stop_token& st) {
        const std::stop_callback on_stop(st, [&worker] { worker.wake.notify(); });

        while (!st.stop_requested()) {
            const auto pass = pump_conditional(worker);
            if (pass.progressed) {
                continue;
            }

            std::optional<std::chrono::steady_clock::time_point> until = pass.deadline;
            if (pass.polled) {
                const auto next_poll = std::chrono::steady_clock::now() + conditional_poll_interval();
                until = until ? std::min(*until, next_poll) : next_poll;
            }

            if (until) {
                (void)worker.wake.wait_until(*until);
            } else {
                worker.wake.wait();
            }
        }
    });
}

void TaskScheduler::pause_all_tasks()
{
    for_each_worker([](ConditionalWorker& worker) {
        for (auto& entry : worker.tasks) {
            if (entry.routine && entry.routine->is_active()) {
                bool current_auto_resume = entry.routine->get_auto_resume();
                entry.routine->set_state<bool>("was_auto_resume", current_auto_resume);
                entry.routine->set_auto_resume(false);
            }
        }
    });

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
    for_each_worker([](ConditionalWorker& worker) {
        for (auto& entry : worker.tasks) {
            if (entry.routine && entry.routine->is_active()) {
                auto was_auto_resume = entry.routine->get_state<bool>("was_auto_resume");
                if (was_auto_resume) {
                    entry.routine->set_auto_resume(*was_auto_resume);
                } else {
                    entry.routine->set_auto_resume(true);
                }
            }
        }
        worker.wake.notify();
    });

    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            auto was_auto_resume = entry.routine->get_state<bool>("was_auto_resume");
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
    bool on_worker_thread = false;
    for_each_worker([&](ConditionalWorker& worker) {
        if (worker.thread.joinable() && worker.thread.get_id() == std::this_thread::get_id()) {
            on_worker_thread = true;
        }
    });
    if (on_worker_thread) {
        return;
    }

    if (t_draining == this) {
        MF_WARN(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "terminate_all_tasks ignored: called from a callback during a queue drain");
        return;
    }

    if (m_terminating.exchange(true)) {
        return;
    }
    TerminationScope terminating(m_terminating);

    while (m_inflight_submissions.load() != 0) {
        std::this_thread::yield();
    }

    for_each_worker([](ConditionalWorker& worker) {
        if (worker.thread.joinable()) {
            worker.thread.request_stop();
            worker.thread.join();
        }
    });

    drain_pending_tasks();
    for_each_worker([this](ConditionalWorker& worker) { drain_conditional_pending(worker); });

    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            entry.routine->set_should_terminate(true);
        }
    }

    for (auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            entry.routine->force_resume();
        }
    }

    constexpr int MAX_ATTEMPTS = 3;
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        bool any_active = false;
        for (auto& entry : m_tasks) {
            if (entry.routine && entry.routine->is_active()) {
                any_active = true;
                entry.routine->force_resume();
            }
        }

        if (!any_active) {
            break;
        }
    }

    bool all_done = true;
    for (const auto& entry : m_tasks) {
        if (entry.routine && entry.routine->is_active()) {
            all_done = false;
            MF_WARN(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
                "Coroutine '{}' stuck after {} attempts - forcing destruction",
                entry.name, MAX_ATTEMPTS);
        }
    }

    if (!all_done) {
        MF_WARN(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "Some coroutines did not complete - forcing destruction");
    } else {
        MF_INFO(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "All coroutines terminated successfully");
    }

    for_each_worker([](ConditionalWorker& worker) {
        for (auto& entry : worker.tasks) {
            if (entry.routine && entry.routine->is_active())
                entry.routine->set_should_terminate(true);
        }
        for (auto& entry : worker.tasks) {
            if (entry.routine && entry.routine->is_active())
                entry.routine->force_resume();
        }
        worker.tasks.clear();
    });

    m_tasks.clear();
}

void TaskScheduler::process_buffer_cycle_tasks()
{
    drain_pending_tasks();

    m_current_buffer_cycle++;
    auto tasks = get_tasks_for_token(ProcessingToken::SAMPLE_ACCURATE);

    for (auto& task : tasks) {
        if (task && task->is_active()) {
            task->try_resume_with_context(m_current_buffer_cycle, DelayContext::BUFFER_BASED);
        }
    }
}

void TaskScheduler::drain_pending_tasks()
{
    if (!m_pending_ops.has_pending())
        return;

    DrainScope drain_scope(this);

    m_pending_ops.drain([this](PendingTaskOp&& op) {
        if (op.is_addition) {
            auto existing_it = find_task_by_name(op.entry.name);
            if (existing_it != m_tasks.end()) {
                if (existing_it->routine && existing_it->routine->is_active())
                    existing_it->routine->set_should_terminate(true);
                m_tasks.erase(existing_it);
            }
            m_tasks.push_back(std::move(op.entry));
        } else {
            auto it = (op.entry.name.empty() && op.entry.routine)
                ? find_task_by_routine(op.entry.routine)
                : find_task_by_name(op.entry.name);
            if (it != m_tasks.end()) {
                if (it->routine && it->routine->is_active())
                    it->routine->set_should_terminate(true);
                m_tasks.erase(it);
            }
        }
    });
}

void TaskScheduler::drain_conditional_pending(ConditionalWorker& worker)
{
    if (!worker.pending.has_pending())
        return;

    DrainScope drain_scope(this);

    worker.pending.drain([&worker](PendingTaskOp&& op) {
        if (op.is_addition) {
            auto it = std::ranges::find_if(worker.tasks,
                [&](const TaskEntry& e) { return e.name == op.entry.name; });
            if (it != worker.tasks.end()) {
                if (it->routine && it->routine->is_active())
                    it->routine->set_should_terminate(true);
                worker.tasks.erase(it);
            }
            worker.tasks.push_back(std::move(op.entry));
        } else {
            auto it = std::ranges::find_if(worker.tasks,
                [&](const TaskEntry& e) { return e.name == op.entry.name; });
            if (it != worker.tasks.end()) {
                if (it->routine && it->routine->is_active())
                    it->routine->set_should_terminate(true);
                worker.tasks.erase(it);
            }
        }
    });
}

void TaskScheduler::pump_cross(DelayContext context, ProcessingToken clock_token, uint64_t processing_units)
{
    auto cross_tasks = get_tasks_for_token(ProcessingToken::MULTI_RATE);
    if (cross_tasks.empty()) {
        return;
    }

    auto clock_it = m_token_clocks.find(clock_token);
    if (clock_it == m_token_clocks.end()) {
        return;
    }

    if (processing_units == 0) {
        processing_units = 1;
    }

    uint64_t base = clock_it->second->current_position();

    for (uint64_t i = 0; i < processing_units; ++i) {
        uint64_t pos = base + i;
        for (auto& routine : cross_tasks) {
            if (routine && routine->is_active()) {
                routine->try_resume_with_context(pos, context);
            }
        }
    }
}

TaskScheduler::ConditionalWorker* TaskScheduler::worker_at(uint32_t index) const noexcept
{
    return index < m_worker_count.load(std::memory_order_acquire) ? m_workers[index].get() : nullptr;
}

void TaskScheduler::wake_conditional() noexcept
{
    for_each_worker([](ConditionalWorker& worker) { worker.wake.notify(); });
}

void TaskScheduler::wake_conditional(WorkerId worker) noexcept
{
    if (auto* target = worker_at(worker.index)) {
        target->wake.notify();
    }
}

WorkerId TaskScheduler::add_worker(const std::string& name)
{
    const std::lock_guard lock(m_worker_creation);

    const uint32_t index = m_worker_count.load(std::memory_order_acquire);
    if (index >= max_workers) {
        MF_ERROR(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            "Worker '{}' not added: the scheduler already has {} workers", name, max_workers);
        return {};
    }

    m_workers[index] = std::make_unique<ConditionalWorker>(name);
    m_worker_count.store(index + 1, std::memory_order_release);
    return WorkerId { index };
}

void TaskScheduler::set_conditional_poll_interval(std::chrono::microseconds interval) noexcept
{
    m_conditional_poll_us.store(std::max<int64_t>(interval.count(), 1), std::memory_order_relaxed);
    wake_conditional();
}

std::chrono::microseconds TaskScheduler::conditional_poll_interval() const noexcept
{
    return std::chrono::microseconds(m_conditional_poll_us.load(std::memory_order_relaxed));
}

TaskScheduler::ConditionalPass TaskScheduler::pump_conditional(ConditionalWorker& worker)
{
    drain_conditional_pending(worker);

    std::erase_if(worker.tasks, [](const TaskEntry& e) {
        return !e.routine || !e.routine->is_active();
    });

    ConditionalPass pass;
    if (worker.tasks.empty()) {
        pass.idle = true;
        return pass;
    }

    for (auto& entry : worker.tasks) {
        if (!entry.routine || !entry.routine->is_active()) {
            continue;
        }

        if (entry.routine->try_resume(0)) {
            pass.progressed = true;
            continue;
        }

        if (!entry.routine->get_auto_resume()) {
            continue;
        }

        const auto* free_routine = dynamic_cast<const FreeRoutine*>(entry.routine.get());
        if (!free_routine) {
            pass.polled = true;
            continue;
        }

        const auto hint = free_routine->wait_hint();
        if (!hint.armed) {
            continue;
        }

        if (hint.deadline) {
            pass.deadline = pass.deadline ? std::min(*pass.deadline, *hint.deadline) : *hint.deadline;
        } else if (!hint.wake_only) {
            pass.polled = true;
        }
    }

    return pass;
}

}
