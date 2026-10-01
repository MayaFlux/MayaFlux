#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

bool BufferPipeline::operation_due(const BufferOperation& op) const
{
    if (op.get_type() == BufferOperation::OpType::CONDITION) {
        if (!op.m_condition || !op.m_condition(m_current_cycle)) {
            return false;
        }
    }

    return m_current_cycle % op.m_cycle_interval == 0;
}

uint32_t BufferPipeline::operation_iterations(const BufferOperation& op) const
{
    if (op.get_type() == BufferOperation::OpType::CAPTURE) {
        return op.m_capture.get_cycle_count();
    }
    return 1;
}

void BufferPipeline::start_cycle()
{
    if (m_cycle_start_callback) {
        m_cycle_start_callback(m_current_cycle);
    }
}

void BufferPipeline::reset_cycle_state()
{
    for (auto& state : m_data_states) {
        if (state != DataState::EMPTY) {
            state = DataState::EMPTY;
        }
    }

    reset_accumulated_data();
}

void BufferPipeline::flow_through(size_t index, uint64_t cycle)
{
    if (index + 1 >= m_operations.size()) {
        return;
    }

    auto& dependent_op = m_operations[index + 1];

    if (m_data_states[index + 1] != DataState::CONSUMED
        && BufferOperation::is_process_phase_operation(dependent_op)) {
        process_operation(dependent_op, cycle);
        m_data_states[index + 1] = DataState::READY;
    }
}

std::vector<std::shared_ptr<Vruta::Routine>> BufferPipeline::dispatch_cycle_branches(bool frame_rate)
{
    std::vector<std::shared_ptr<Vruta::Routine>> synchronous_tasks;

    for (auto& branch : m_branches) {
        if (!branch.condition(m_current_cycle)) {
            continue;
        }

        std::shared_ptr<Vruta::Routine> task = frame_rate
            ? std::shared_ptr<Vruta::Routine>(dispatch_frame_branch(branch))
            : std::shared_ptr<Vruta::Routine>(dispatch_branch_async(branch, m_current_cycle));

        if (branch.synchronous && task) {
            synchronous_tasks.push_back(task);
        }
    }

    return synchronous_tasks;
}

bool BufferPipeline::any_active(const std::vector<std::shared_ptr<Vruta::Routine>>& tasks)
{
    return std::ranges::any_of(tasks, [](const auto& task) {
        return task && task->is_active();
    });
}

void BufferPipeline::finish_cycle()
{
    cleanup_completed_branches();

    if (m_cycle_end_callback) {
        m_cycle_end_callback(m_current_cycle);
    }

    cleanup_expired_data();
}

Vruta::SoundRoutine BufferPipeline::execute_phased(uint64_t max_cycles, uint64_t samples_per_operation)
{
    auto& promise = co_await Kriya::GetAudioPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (promise.should_terminate) {
            break;
        }

        start_cycle();
        reset_cycle_state();

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!BufferOperation::is_capture_phase_operation(op) || !operation_due(op)) {
                continue;
            }

            const uint32_t op_iterations = operation_iterations(op);

            for (uint32_t iter = 0; iter < op_iterations; ++iter) {
                process_operation(op, m_current_cycle + iter);
                if (m_capture_timing == Vruta::DelayContext::BUFFER_BASED) {
                    co_await BufferDelay { 1 };
                } else if (m_capture_timing == Vruta::DelayContext::SAMPLE_BASED && samples_per_operation > 0) {
                    co_await SampleDelay { samples_per_operation };
                }
            }

            m_data_states[i] = DataState::READY;
        }

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!BufferOperation::is_process_phase_operation(op)) {
                continue;
            }

            if (m_data_states[i] == DataState::CONSUMED) {
                continue;
            }

            if (!operation_due(op)) {
                continue;
            }

            process_operation(op, m_current_cycle);
            m_data_states[i] = DataState::READY;

            if (m_process_timing == Vruta::DelayContext::BUFFER_BASED) {
                co_await BufferDelay { 1 };
            } else if (m_process_timing == Vruta::DelayContext::SAMPLE_BASED && samples_per_operation > 0) {
                co_await SampleDelay { samples_per_operation };
            }
        }

        const auto synchronous_tasks = dispatch_cycle_branches(false);

        while (any_active(synchronous_tasks)) {
            if (m_process_timing == Vruta::DelayContext::BUFFER_BASED) {
                co_await BufferDelay { 1 };
            } else {
                co_await SampleDelay { 1 };
            }
        }

        finish_cycle();

        m_current_cycle++;
        cycles_executed++;
    }

    if (m_on_complete)
        m_on_complete();
}

Vruta::SoundRoutine BufferPipeline::execute_streaming(uint64_t max_cycles, uint64_t samples_per_operation)
{
    auto& promise = co_await Kriya::GetAudioPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (promise.should_terminate) {
            break;
        }

        start_cycle();

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!operation_due(op)) {
                continue;
            }

            if (m_data_states[i] == DataState::CONSUMED) {
                continue;
            }

            const uint32_t op_iterations = operation_iterations(op);

            for (uint32_t iter = 0; iter < op_iterations; ++iter) {

                process_operation(op, m_current_cycle + iter);
                m_data_states[i] = DataState::READY;

                flow_through(i, m_current_cycle + iter);

                if (m_capture_timing == Vruta::DelayContext::BUFFER_BASED) {
                    co_await BufferDelay { 1 };
                } else if (samples_per_operation > 0) {
                    co_await SampleDelay { samples_per_operation };
                }
            }
        }

        dispatch_cycle_branches(false);

        finish_cycle();

        m_current_cycle++;
        cycles_executed++;
    }

    if (m_on_complete)
        m_on_complete();
}

Vruta::SoundRoutine BufferPipeline::execute_parallel(uint64_t max_cycles, uint64_t samples_per_operation)
{
    MF_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
        "PARALLEL strategy not yet implemented, using PHASED as fallback");
    return execute_phased(max_cycles, samples_per_operation);
}

Vruta::SoundRoutine BufferPipeline::execute_reactive(uint64_t max_cycles, uint64_t samples_per_operation)
{
    MF_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
        "REACTIVE strategy not yet implemented, using PHASED as fallback");
    return execute_phased(max_cycles, samples_per_operation);
}

Vruta::GraphicsRoutine BufferPipeline::execute_frame_phased(uint64_t max_cycles, uint64_t frames_per_operation)
{
    auto& promise = co_await Kriya::GetGraphicsPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (promise.should_terminate) {
            break;
        }

        bool waited = false;

        start_cycle();
        reset_cycle_state();

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!BufferOperation::is_capture_phase_operation(op) || !operation_due(op)) {
                continue;
            }

            const uint32_t op_iterations = operation_iterations(op);

            for (uint32_t iter = 0; iter < op_iterations; ++iter) {
                process_operation(op, m_current_cycle + iter);
                waited = true;
                co_await FrameDelay { .frames_to_wait = 1 };
            }

            m_data_states[i] = DataState::READY;
        }

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!BufferOperation::is_process_phase_operation(op)) {
                continue;
            }

            if (m_data_states[i] == DataState::CONSUMED) {
                continue;
            }

            if (!operation_due(op)) {
                continue;
            }

            process_operation(op, m_current_cycle);
            m_data_states[i] = DataState::READY;

            if (frames_per_operation > 0) {
                waited = true;
                co_await FrameDelay { .frames_to_wait = frames_per_operation };
            }
        }

        const auto synchronous_tasks = dispatch_cycle_branches(true);

        while (any_active(synchronous_tasks)) {
            waited = true;
            co_await FrameDelay { .frames_to_wait = 1 };
        }

        finish_cycle();

        m_current_cycle++;
        cycles_executed++;

        if (!waited) {
            co_await FrameDelay { .frames_to_wait = 1 };
        }
    }

    if (m_on_complete)
        m_on_complete();
}

Vruta::GraphicsRoutine BufferPipeline::execute_frame_streaming(uint64_t max_cycles, uint64_t frames_per_operation)
{
    auto& promise = co_await Kriya::GetGraphicsPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (promise.should_terminate) {
            break;
        }

        bool waited = false;

        start_cycle();

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!operation_due(op)) {
                continue;
            }

            if (m_data_states[i] == DataState::CONSUMED) {
                continue;
            }

            const uint32_t op_iterations = operation_iterations(op);
            const bool is_capture = op.get_type() == BufferOperation::OpType::CAPTURE;

            for (uint32_t iter = 0; iter < op_iterations; ++iter) {

                process_operation(op, m_current_cycle + iter);
                m_data_states[i] = DataState::READY;

                flow_through(i, m_current_cycle + iter);

                if (is_capture) {
                    waited = true;
                    co_await FrameDelay { .frames_to_wait = 1 };
                } else if (frames_per_operation > 0) {
                    waited = true;
                    co_await FrameDelay { .frames_to_wait = frames_per_operation };
                }
            }
        }

        dispatch_cycle_branches(true);

        finish_cycle();

        m_current_cycle++;
        cycles_executed++;

        if (!waited) {
            co_await FrameDelay { .frames_to_wait = 1 };
        }
    }

    if (m_on_complete)
        m_on_complete();
}

Vruta::GraphicsRoutine BufferPipeline::execute_frame_internal(uint64_t max_cycles, uint64_t frames_per_operation)
{
    switch (m_execution_strategy) {
    case ExecutionStrategy::STREAMING:
        return execute_frame_streaming(max_cycles, frames_per_operation);

    case ExecutionStrategy::PARALLEL:
    case ExecutionStrategy::REACTIVE:
        MF_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "PARALLEL and REACTIVE strategies not yet implemented, using PHASED as fallback");
        return execute_frame_phased(max_cycles, frames_per_operation);

    case ExecutionStrategy::PHASED:
    default:
        return execute_frame_phased(max_cycles, frames_per_operation);
    }
}

}
