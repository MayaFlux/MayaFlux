#include "BufferPipeline.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Staging/DataReadProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Kriya {

BufferPipeline::BufferPipeline(
    Vruta::TaskScheduler& scheduler,
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<IO::IOManager> io_manager)
    : m_buffer_manager(std::move(buffer_manager))
    , m_io_manager(std::move(io_manager))
    , m_scheduler(&scheduler)
{
}

BufferPipeline::~BufferPipeline()
{
    for (auto& [key, entry] : m_readers) {
        entry.buffer->get_processing_chain()->remove_processor(entry.processor, entry.buffer);
    }

    if (!m_buffer_manager) {
        return;
    }

    for (auto& op : m_operations) {
        if (op.m_attached_processor) {
            const bool detachable = op.get_type() == BufferOperation::OpType::ROUTE
                || op.get_type() == BufferOperation::OpType::LOAD
                || op.get_type() == BufferOperation::OpType::FUSE
                || op.get_type() == BufferOperation::OpType::MODIFY;

            if (detachable && op.m_target_audio_buffer) {
                m_buffer_manager->remove_processor(op.m_attached_processor, op.m_target_audio_buffer);
            }
            if (detachable && op.m_target_graphics_buffer) {
                m_buffer_manager->remove_processor(op.m_attached_processor, op.m_target_graphics_buffer);
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

    m_routine = routine;
    m_scheduler->add_task(std::move(routine));

    m_active_self = self;
}

void BufferPipeline::execute_frame_rate(uint64_t max_cycles, uint64_t frames_per_operation)
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

    auto routine = std::make_shared<Vruta::GraphicsRoutine>(
        execute_frame_internal(max_cycles, frames_per_operation));

    m_routine = routine;
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
    m_routine = routine;
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
    m_routine = routine;
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

    m_routine = routine;
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

void BufferPipeline::end()
{
    m_continuous_execution = false;

    const auto routine = std::move(m_routine);
    const auto self = std::move(m_active_self);

    if (!routine) {
        return;
    }

    const bool running = routine->is_active();

    if (m_scheduler) {
        m_scheduler->cancel_task(routine);
    }

    if (running && m_on_complete) {
        m_on_complete();
    }
}

bool BufferPipeline::is_running() const
{
    return m_routine && m_routine->is_active();
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
    prepare_displays();

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

namespace {

    bool recording_arguments_valid(
        const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
        const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
        const std::shared_ptr<Kakshya::DynamicSoundStream>& stream)
    {
        if (scheduler && buffer_manager && stream) {
            return true;
        }
        MF_ERROR(Journal::Component::Kriya, Journal::Context::BufferManagement,
            "recording requires a scheduler, a buffer manager and a stream");
        return false;
    }

    std::shared_ptr<BufferPipeline> run_recording(
        const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
        const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
        const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
        const std::shared_ptr<Buffers::AudioBuffer>& listener,
        uint32_t input_channel,
        uint64_t cycles,
        std::optional<uint32_t> monitor,
        uint32_t stream_channel)
    {
        auto pipeline = BufferPipeline::create(*scheduler, buffer_manager);
        pipeline >> BufferOperation::capture_from(listener).for_cycles(1)
            >> BufferOperation::route_to_container(stream, stream_channel);
        pipeline->on_complete([buffer_manager, listener, input_channel, monitor] {
            buffer_manager->unregister_input_listener(listener, input_channel);
            if (monitor) {
                buffer_manager->remove_buffer(listener, Buffers::ProcessingToken::AUDIO_BACKEND, *monitor);
            }
        });
        pipeline->execute_buffer_rate(cycles);
        return pipeline;
    }

}

std::pair<std::shared_ptr<BufferPipeline>, std::shared_ptr<Buffers::AudioBuffer>> record(
    const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
    uint32_t input_channel,
    uint64_t cycles,
    std::optional<uint32_t> monitor,
    uint32_t stream_channel)
{
    if (!recording_arguments_valid(scheduler, buffer_manager, stream)) {
        return { nullptr, nullptr };
    }

    auto listener = BufferOperation::capture_input_from(buffer_manager, input_channel, monitor).get_audio_buffer();
    auto pipeline = run_recording(scheduler, buffer_manager, stream, listener, input_channel, cycles, monitor, stream_channel);

    return { std::move(pipeline), std::move(listener) };
}

std::shared_ptr<BufferPipeline> record_into(
    const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
    const std::shared_ptr<Buffers::AudioBuffer>& listener,
    uint32_t input_channel,
    uint64_t cycles,
    std::optional<uint32_t> monitor,
    uint32_t stream_channel)
{
    if (!listener || !recording_arguments_valid(scheduler, buffer_manager, stream)) {
        return nullptr;
    }

    buffer_manager->register_input_listener(listener, input_channel);
    if (monitor) {
        buffer_manager->add_buffer(listener, Buffers::ProcessingToken::AUDIO_BACKEND, *monitor);
    }

    return run_recording(scheduler, buffer_manager, stream, listener, input_channel, cycles, monitor, stream_channel);
}

}
