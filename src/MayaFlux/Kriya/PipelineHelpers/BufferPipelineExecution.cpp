#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

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

        if (m_cycle_start_callback) {
            m_cycle_start_callback(m_current_cycle);
        }

        for (auto& state : m_data_states) {
            if (state != DataState::EMPTY) {
                state = DataState::EMPTY;
            }
        }

        reset_accumulated_data();

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (!BufferOperation::is_capture_phase_operation(op)) {
                continue;
            }

            if (op.get_type() == BufferOperation::OpType::CONDITION) {
                if (!op.m_condition || !op.m_condition(m_current_cycle)) {
                    continue;
                }
            }

            if (m_current_cycle % op.m_cycle_interval != 0) {
                continue;
            }

            uint32_t op_iterations = 1;
            if (op.get_type() == BufferOperation::OpType::CAPTURE) {
                op_iterations = op.m_capture.get_cycle_count();
            }

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

            if (op.get_type() == BufferOperation::OpType::CONDITION) {
                if (!op.m_condition || !op.m_condition(m_current_cycle)) {
                    continue;
                }
            }

            if (m_current_cycle % op.m_cycle_interval != 0) {
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

        std::vector<std::shared_ptr<Vruta::SoundRoutine>> current_cycle_sync_tasks;

        for (auto& branch : m_branches) {
            if (branch.condition(m_current_cycle)) {
                auto task = dispatch_branch_async(branch, m_current_cycle);

                if (branch.synchronous && task) {
                    current_cycle_sync_tasks.push_back(task);
                }
            }
        }

        if (!current_cycle_sync_tasks.empty()) {
            bool any_active = true;
            while (any_active) {
                any_active = false;

                for (auto& task : current_cycle_sync_tasks) {
                    if (task && task->is_active()) {
                        any_active = true;
                        break;
                    }
                }

                if (any_active) {
                    if (m_process_timing == Vruta::DelayContext::BUFFER_BASED) {
                        co_await BufferDelay { 1 };
                    } else {
                        co_await SampleDelay { 1 };
                    }
                }
            }
        }

        cleanup_completed_branches();

        if (m_cycle_end_callback) {
            m_cycle_end_callback(m_current_cycle);
        }

        cleanup_expired_data();

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

        if (m_cycle_start_callback) {
            m_cycle_start_callback(m_current_cycle);
        }

        for (size_t i = 0; i < m_operations.size(); ++i) {
            auto& op = m_operations[i];

            if (op.get_type() == BufferOperation::OpType::CONDITION) {
                if (!op.m_condition || !op.m_condition(m_current_cycle)) {
                    continue;
                }
            }

            if (m_current_cycle % op.m_cycle_interval != 0) {
                continue;
            }

            if (m_data_states[i] == DataState::CONSUMED) {
                continue;
            }

            uint32_t op_iterations = 1;
            if (op.get_type() == BufferOperation::OpType::CAPTURE) {
                op_iterations = op.m_capture.get_cycle_count();
            }

            for (uint32_t iter = 0; iter < op_iterations; ++iter) {

                process_operation(op, m_current_cycle + iter);
                m_data_states[i] = DataState::READY;

                for (size_t j = i + 1; j < m_operations.size(); ++j) {
                    auto& dependent_op = m_operations[j];

                    if (m_data_states[j] != DataState::CONSUMED
                        && BufferOperation::is_process_phase_operation(dependent_op)) {
                        process_operation(dependent_op, m_current_cycle + iter);
                        m_data_states[j] = DataState::READY;
                    }

                    break;
                }

                if (m_capture_timing == Vruta::DelayContext::BUFFER_BASED) {
                    co_await BufferDelay { 1 };
                } else if (samples_per_operation > 0) {
                    co_await SampleDelay { samples_per_operation };
                }
            }
        }

        for (auto& branch : m_branches) {
            if (branch.condition(m_current_cycle)) {
                dispatch_branch_async(branch, m_current_cycle);
            }
        }

        cleanup_completed_branches();

        if (m_cycle_end_callback) {
            m_cycle_end_callback(m_current_cycle);
        }

        cleanup_expired_data();

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

}
