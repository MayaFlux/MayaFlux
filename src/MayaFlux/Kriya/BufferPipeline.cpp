#include "BufferPipeline.hpp"

#include "CycleCoordinator.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

BufferPipeline::BufferPipeline(Vruta::TaskScheduler& scheduler, std::shared_ptr<Buffers::BufferManager> buffer_manager)
    : m_coordinator(std::make_shared<CycleCoordinator>(scheduler))
    , m_buffer_manager(std::move(buffer_manager))
    , m_scheduler(&scheduler)
{
}

BufferPipeline::~BufferPipeline()
{
    if (!m_buffer_manager) {
        return;
    }

    for (auto& op : m_operations) {
        if (op.m_attached_processor) {
            if (op.get_type() == BufferOperation::OpType::ROUTE && op.m_target_buffer) {
                m_buffer_manager->remove_processor(op.m_attached_processor, op.m_target_buffer);
            }
            if (op.get_type() == BufferOperation::OpType::MODIFY && op.m_target_buffer) {
                m_buffer_manager->remove_processor(op.m_attached_processor, op.m_target_buffer);
            }
            op.m_attached_processor = nullptr;
        }
    }
}

BufferPipeline& BufferPipeline::parallel(std::initializer_list<BufferOperation> operations)
{
    for (auto& op : operations) {
        BufferOperation parallel_op = BufferOperation(op);
        parallel_op.with_priority(255);
        m_operations.emplace_back(std::move(parallel_op));
    }
    return *this;
}

BufferPipeline& BufferPipeline::with_lifecycle(
    std::function<void(uint32_t)> on_cycle_start,
    std::function<void(uint32_t)> on_cycle_end)
{
    m_cycle_start_callback = std::move(on_cycle_start);
    m_cycle_end_callback = std::move(on_cycle_end);
    return *this;
}

void BufferPipeline::execute_buffer_rate(uint64_t max_cycles)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }

    auto self = shared_from_this();

    if (max_cycles == 0) {
        max_cycles = UINT64_MAX;
        m_continuous_execution = true;
    }
    m_max_cycles = max_cycles;

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(max_cycles, 0));

    m_scheduler->add_task(std::move(routine));

    m_active_self = self;
}

void BufferPipeline::execute_once()
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }
    auto self = shared_from_this();

    m_max_cycles = 1;
    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(1, 0));
    m_scheduler->add_task(std::move(routine));
    m_active_self = self;
}

void BufferPipeline::execute_for_cycles(uint64_t cycles)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }

    auto self = shared_from_this();

    if (cycles == 0) {
        cycles = UINT64_MAX;
        m_continuous_execution = true;
    }
    m_max_cycles = cycles;
    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(cycles, 0));
    m_scheduler->add_task(std::move(routine));
    m_active_self = self;
}

void BufferPipeline::execute_continuous()
{
    m_continuous_execution = true;
    execute_for_cycles(0);
}

void BufferPipeline::execute_scheduled(
    uint64_t max_cycles,
    uint64_t samples_per_operation)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline must have scheduler for scheduled execution");
    }

    auto self = shared_from_this();

    if (max_cycles == 0) {
        max_cycles = UINT64_MAX;
        m_continuous_execution = true;
    }

    m_max_cycles = max_cycles;

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(max_cycles, samples_per_operation));

    m_scheduler->add_task(std::move(routine));

    m_active_self = self;
}

void BufferPipeline::execute_scheduled_at_rate(
    uint32_t max_cycles,
    double seconds_per_operation)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline must have scheduler for scheduled execution");
    }

    uint64_t samples = m_scheduler->seconds_to_samples(seconds_per_operation);
    execute_scheduled(max_cycles, samples);
}

void BufferPipeline::mark_data_consumed(uint32_t operation_index)
{
    if (operation_index < m_data_states.size()) {
        m_data_states[operation_index] = DataState::CONSUMED;
    }
}

bool BufferPipeline::has_pending_data() const
{
    return std::ranges::any_of(m_data_states,
        [](DataState state) { return state == DataState::READY; });
}

BufferPipeline& BufferPipeline::on_complete(std::function<void()> cb)
{
    m_on_complete = std::move(cb);
    return *this;
}

Vruta::SoundRoutine BufferPipeline::execute_internal(uint64_t max_cycles, uint64_t samples_per_operation)
{
    switch (m_execution_strategy) {
    case ExecutionStrategy::PHASED:
        return execute_phased(max_cycles, samples_per_operation);

    case ExecutionStrategy::STREAMING:
        return execute_streaming(max_cycles, samples_per_operation);

    case ExecutionStrategy::PARALLEL:
        return execute_parallel(max_cycles, samples_per_operation);

    case ExecutionStrategy::REACTIVE:
        return execute_reactive(max_cycles, samples_per_operation);

    default:
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Unknown execution strategy in BufferPipeline");
    }
}

}
