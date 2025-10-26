#include "BufferPipeline.hpp"

#include "CycleCoordinator.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kriya/Awaiters.hpp"

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
    if (m_buffer_manager) {
        for (auto& op : m_operations) {
            if (op.get_type() == BufferOperation::OpType::MODIFY && op.m_attached_processor) {
                m_buffer_manager->remove_processor(
                    op.m_attached_processor,
                    op.m_target_buffer);
            }
        }
    }
}

BufferPipeline& BufferPipeline::branch_if(
    std::function<bool(uint32_t)> condition,
    const std::function<void(BufferPipeline&)>& branch_builder,
    bool synchronous,
    uint64_t samples_per_operation)
{
    auto branch_pipeline = std::make_shared<BufferPipeline>();
    if (m_scheduler) {
        branch_pipeline->m_scheduler = m_scheduler;
    }
    branch_builder(*branch_pipeline);

    m_branches.push_back({ std::move(condition),
        std::move(branch_pipeline),
        synchronous,
        samples_per_operation });

    return *this;
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

void BufferPipeline::execute_buffer_rate(uint32_t max_cycles)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }

    auto self = shared_from_this();

    m_max_cycles = max_cycles;

    // m_continuous_execution = true;

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

    m_max_cycles = 1;
    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(1, 0));
    m_scheduler->add_task(std::move(routine));
}

void BufferPipeline::execute_for_cycles(uint32_t cycles)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }

    m_max_cycles = cycles;
    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(cycles, 0));
    m_scheduler->add_task(std::move(routine));
}

void BufferPipeline::execute_continuous()
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline requires scheduler for execution");
    }
    m_continuous_execution = true;
    auto self = shared_from_this();

    m_max_cycles = UINT64_MAX;

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        execute_internal(0, 0));

    m_scheduler->add_task(std::move(routine));
    m_active_self = self;
}

void BufferPipeline::execute_scheduled(
    uint32_t max_cycles,
    uint64_t samples_per_operation)
{
    if (!m_scheduler) {
        error<std::runtime_error>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Pipeline must have scheduler for scheduled execution");
    }

    auto self = shared_from_this();

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

Kakshya::DataVariant BufferPipeline::extract_buffer_data(const std::shared_ptr<Buffers::AudioBuffer>& buffer, bool should_process)
{
    auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (audio_buffer) {
        if (should_process) {
            audio_buffer->process_default();
        }
        const auto& data_span = audio_buffer->get_data();
        std::vector<double> data_vector(data_span.begin(), data_span.end());
        return data_vector;
    }

    return std::vector<double> {};
}

void BufferPipeline::write_to_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, const Kakshya::DataVariant& data)
{
    auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (audio_buffer) {
        try {
            auto audio_data = std::get<std::vector<double>>(data);
            auto& buffer_data = audio_buffer->get_data();

            if (buffer_data.size() != audio_data.size()) {
                buffer_data.resize(audio_data.size());
            }

            std::ranges::copy(audio_data, buffer_data.begin());

        } catch (const std::bad_variant_access& e) {
            error_rethrow(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "Data type mismatch when writing to audio buffer: {}",
                e.what());
        }
    }

    // TODO: Handle other buffer types
}

void BufferPipeline::write_to_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, const Kakshya::DataVariant& data)
{
    try {
        auto audio_data = std::get<std::vector<double>>(data);
        std::span<const double> data_span(audio_data.data(), audio_data.size());

        container->write_frames(data_span, 0);

    } catch (const std::bad_variant_access& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Data type mismatch when writing to container: {}",
            e.what());
    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error writing to container: {}",
            e.what());
    }
}

Kakshya::DataVariant BufferPipeline::read_from_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container,
    uint64_t /*start_frame*/,
    uint32_t length)
{
    try {
        uint32_t read_length = length;
        if (read_length == 0) {
            read_length = static_cast<uint32_t>(container->get_total_elements() / container->get_num_channels());
        }

        std::vector<double> output_data(static_cast<size_t>(read_length * container->get_num_channels()));
        std::span<double> output_span(output_data.data(), output_data.size());

        uint64_t frames_read = container->read_frames(output_span, read_length);

        if (frames_read < output_data.size()) {
            output_data.resize(frames_read);
        }

        return output_data;

    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error reading from container: {}",
            e.what());

        return std::vector<double> {};
    }
}

void BufferPipeline::capture_operation(BufferOperation& op, uint64_t cycle)
{
    bool should_process = op.m_capture.get_processing_control() == BufferCapture::ProcessingControl::ON_CAPTURE;
    auto buffer_data = extract_buffer_data(op.m_capture.get_buffer(), should_process);

    if (op.m_capture.m_data_ready_callback) {
        op.m_capture.m_data_ready_callback(buffer_data, cycle);
    }

    auto capture_mode = op.m_capture.get_mode();

    switch (capture_mode) {
    case BufferCapture::CaptureMode::TRANSIENT:
        m_operation_data[&op] = buffer_data;
        break;

    case BufferCapture::CaptureMode::ACCUMULATE: {
        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                auto& existing = std::get<std::vector<double>>(it->second);
                const auto& new_data = std::get<std::vector<double>>(buffer_data);
                existing.insert(existing.end(), new_data.begin(), new_data.end());

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Data type mismatch during ACCUMULATE capture: {}",
                    e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }
    case BufferCapture::CaptureMode::CIRCULAR: {
        uint32_t circular_size = op.m_capture.get_circular_size();
        if (circular_size == 0) {
            circular_size = 4096;
        }

        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                auto& circular = std::get<std::vector<double>>(it->second);
                const auto& new_data = std::get<std::vector<double>>(buffer_data);

                circular.insert(circular.end(), new_data.begin(), new_data.end());

                if (circular.size() > circular_size) {
                    circular.erase(circular.begin(),
                        circular.begin() + static_cast<int64_t>(circular.size() - circular_size));
                }

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Data type mismatch during CIRCULAR capture: {}",
                    e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }
    case BufferCapture::CaptureMode::WINDOWED: {
        uint32_t window_size = op.m_capture.get_window_size();
        float overlap_ratio = op.m_capture.get_overlap_ratio();

        if (window_size == 0) {
            window_size = 512;
        }

        auto hop_size = static_cast<uint32_t>((float)window_size * (1.0F - overlap_ratio));
        if (hop_size == 0)
            hop_size = 1;

        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                auto& windowed = std::get<std::vector<double>>(it->second);
                const auto& new_data = std::get<std::vector<double>>(buffer_data);

                auto size_cache = static_cast<int64_t>(windowed.size());

                if (size_cache >= window_size) {
                    if (hop_size >= windowed.size()) {
                        windowed = std::get<std::vector<double>>(buffer_data);
                    } else {
                        windowed.erase(windowed.begin(),
                            windowed.begin() + std::min<int64_t>(hop_size, size_cache));
                        windowed.insert(windowed.end(), new_data.begin(), new_data.end());

                        if (windowed.size() > window_size) {
                            windowed.erase(windowed.begin(),
                                windowed.begin() + (size_cache - window_size));
                        }
                    }
                } else {
                    windowed.insert(windowed.end(), new_data.begin(), new_data.end());
                }

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                    "Data type mismatch during WINDOWED capture: {}", e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }

    case BufferCapture::CaptureMode::TRIGGERED: {
        if (op.m_capture.m_stop_condition && op.m_capture.m_stop_condition()) {
            m_operation_data[&op] = buffer_data;
        }
        break;
    }

    default:
        m_operation_data[&op] = buffer_data;
        break;
    }

    m_operation_data[&op] = buffer_data;

    if (has_immediate_routing(op)) {
        auto current_it = std::ranges::find_if(m_operations,
            [&op](const BufferOperation& o) { return &o == &op; });

        if (current_it != m_operations.end()) {
            auto next_it = std::next(current_it);
            if (next_it != m_operations.end() && next_it->get_type() == BufferOperation::OpType::ROUTE) {

                if (next_it->m_target_buffer) {
                    write_to_buffer(next_it->m_target_buffer, buffer_data);
                } else if (next_it->m_target_container) {
                    write_to_container(next_it->m_target_container, buffer_data);
                }

                size_t route_index = std::distance(m_operations.begin(), next_it);
                if (route_index < m_data_states.size()) {
                    m_data_states[route_index] = DataState::CONSUMED;
                }
            }
        }
    }
}

void BufferPipeline::reset_accumulated_data()
{
    for (auto& op : m_operations) {
        if (op.get_type() == BufferOperation::OpType::CAPTURE) {
            auto mode = op.m_capture.get_mode();
            if (mode == BufferCapture::CaptureMode::ACCUMULATE || mode == BufferCapture::CaptureMode::CIRCULAR || mode == BufferCapture::CaptureMode::WINDOWED) {
                m_operation_data.erase(&op);
            }
        }
    }
}

bool BufferPipeline::has_immediate_routing(const BufferOperation& op) const
{
    auto it = std::ranges::find_if(m_operations,
        [&op](const BufferOperation& o) { return &o == &op; });

    if (it == m_operations.end() || std::next(it) == m_operations.end()) {
        return false;
    }

    auto next_op = std::next(it);
    return next_op->get_type() == BufferOperation::OpType::ROUTE;
}

void BufferPipeline::process_operation(BufferOperation& op, uint64_t cycle)
{
    try {
        switch (op.get_type()) {
        case BufferOperation::OpType::CAPTURE: {
            capture_operation(op, cycle);

            break;
        }

        case BufferOperation::OpType::TRANSFORM: {
            Kakshya::DataVariant input_data;
            if (m_operation_data.find(&op) != m_operation_data.end()) {
                input_data = m_operation_data[&op];
            } else {
                for (auto& it : m_operation_data) {
                    input_data = it.second;
                    break;
                }
            }

            if (op.m_transformer) {
                auto transformed = op.m_transformer(input_data, cycle);
                m_operation_data[&op] = transformed;

                for (auto& m_operation : std::ranges::reverse_view(m_operations)) {
                    if (&m_operation == &op)
                        continue;
                    if (m_operation.get_type() == BufferOperation::OpType::CAPTURE && m_operation.m_capture.get_buffer()) {
                        write_to_buffer(m_operation.m_capture.get_buffer(), transformed);
                        break;
                    }
                }
            }
            break;
        }

        case BufferOperation::OpType::ROUTE: {
            Kakshya::DataVariant data_to_route;
            if (m_operation_data.find(&op) != m_operation_data.end()) {
                data_to_route = m_operation_data[&op];
            } else {
                for (auto& it : m_operation_data) {
                    data_to_route = it.second;
                    break;
                }
            }

            if (op.m_target_buffer) {
                write_to_buffer(op.m_target_buffer, data_to_route);
            } else if (op.m_target_container) {
                write_to_container(op.m_target_container, data_to_route);
            }
            break;
        }

        case BufferOperation::OpType::LOAD: {
            auto loaded_data = read_from_container(op.m_source_container,
                op.m_start_frame,
                op.m_load_length);

            if (op.m_target_buffer) {
                write_to_buffer(op.m_target_buffer, loaded_data);
            }

            m_operation_data[&op] = loaded_data;
            break;
        }

        case BufferOperation::OpType::FUSE: {
            std::vector<Kakshya::DataVariant> fusion_inputs;

            for (auto& source_buffer : op.m_source_buffers) {
                bool should_process = op.m_capture.get_processing_control() == BufferCapture::ProcessingControl::ON_CAPTURE;
                auto buffer_data = extract_buffer_data(source_buffer, should_process);
                fusion_inputs.push_back(buffer_data);
            }

            for (auto& source_container : op.m_source_containers) {
                auto container_data = read_from_container(source_container, 0, 0);
                fusion_inputs.push_back(container_data);
            }

            if (op.m_fusion_function && !fusion_inputs.empty()) {
                auto fused_data = op.m_fusion_function(fusion_inputs, cycle);

                if (op.m_target_buffer) {
                    write_to_buffer(op.m_target_buffer, fused_data);
                } else if (op.m_target_container) {
                    write_to_container(op.m_target_container, fused_data);
                }

                m_operation_data[&op] = fused_data;
            }
            break;
        }

        case BufferOperation::OpType::DISPATCH: {
            Kakshya::DataVariant data_to_dispatch;
            if (m_operation_data.find(&op) != m_operation_data.end()) {
                data_to_dispatch = m_operation_data[&op];
            } else {
                for (auto& it : m_operation_data) {
                    data_to_dispatch = it.second;
                    break;
                }
            }

            if (op.m_dispatch_handler) {
                op.m_dispatch_handler(data_to_dispatch, cycle);
            }
            break;
        }

        case BufferOperation::OpType::MODIFY: {
            if (!m_buffer_manager) {
                error<std::invalid_argument>(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    std::source_location::current(),
                    "BufferPipeline has no BufferManager for MODIFY operation");
            }

            if (!op.m_attached_processor) {
                op.m_attached_processor = m_buffer_manager->attach_quick_process(
                    op.m_buffer_modifier,
                    op.m_target_buffer);
                if (m_max_cycles != 0 && op.is_streaming()) {
                    op.m_modify_cycle_count = m_max_cycles - cycle;
                }
            }

            if (op.m_modify_cycle_count > 0 && cycle >= op.m_modify_cycle_count - 1) {
                if (op.m_attached_processor) {
                    m_buffer_manager->remove_processor(
                        op.m_attached_processor,
                        op.m_target_buffer);
                    op.m_attached_processor = nullptr;
                }
            }

            break;
        }

        case BufferOperation::OpType::CONDITION:
            break;

        default:
            MF_ERROR(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Unknown operation type in pipeline");
            break;
        }
    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error processing operation in BufferPipeline: {}",
            e.what());
    }
}

std::shared_ptr<Vruta::SoundRoutine> BufferPipeline::dispatch_branch_async(BranchInfo& branch, uint64_t /*cycle*/)
{
    if (!m_scheduler)
        return nullptr;

    if (!m_coordinator) {
        m_coordinator = std::make_unique<CycleCoordinator>(*m_scheduler);
    }

    branch.pipeline->m_active_self = branch.pipeline;

    auto branch_routine = branch.pipeline->execute_internal(1, branch.samples_per_operation);

    auto task = std::make_shared<Vruta::SoundRoutine>(std::move(branch_routine));
    m_scheduler->add_task(task);

    m_branch_tasks.push_back(task);

    return task;
}

void BufferPipeline::cleanup_expired_data()
{
    for (size_t i = 0; i < m_data_states.size(); ++i) {
        if (m_data_states[i] == DataState::READY) {
            if (i < m_operations.size() && m_operations[i].get_type() == BufferOperation::OpType::CAPTURE && m_operations[i].m_capture.get_mode() == BufferCapture::CaptureMode::TRANSIENT) {

                if (m_operations[i].m_capture.m_data_expired_callback) {
                    auto it = m_operation_data.find(&m_operations[i]);
                    if (it != m_operation_data.end()) {
                        m_operations[i].m_capture.m_data_expired_callback(it->second, m_current_cycle);
                    }
                }

                m_data_states[i] = DataState::EXPIRED;
            } else {
                m_data_states[i] = DataState::CONSUMED;
            }
        }
    }

    auto it = m_operation_data.begin();
    while (it != m_operation_data.end()) {
        if (m_current_cycle > 2) {
            it = m_operation_data.erase(it);
        } else {
            ++it;
        }
    }
}

void BufferPipeline::cleanup_completed_branches()
{
    for (auto& branch : m_branches) {
        if (branch.pipeline) {
            branch.pipeline->m_active_self.reset();
        }
    }

    m_branch_tasks.erase(
        std::remove_if(m_branch_tasks.begin(), m_branch_tasks.end(),
            [](const auto& task) { return !task || !task->is_active(); }),
        m_branch_tasks.end());
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

Vruta::SoundRoutine BufferPipeline::execute_phased(uint64_t max_cycles, uint64_t samples_per_operation)
{
    auto& promise = co_await Kriya::GetPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    // std::cout << "Starting PHASED execution strategy\n";

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

        // ═══════════════════════════════════════════════════════
        // PHASE 1: CAPTURE - Execute all capture operations
        // ═══════════════════════════════════════════════════════
        // std::cout << "=== CAPTURE PHASE - Cycle " << m_current_cycle << " ===\n";

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
                // std::cout << "  Capture op [" << i << "] will execute "
                //           << op_iterations << " iterations\n";
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
            // std::cout << "  Capture op [" << i << "] completed, data READY\n";
        }

        // ═══════════════════════════════════════════════════════
        // PHASE 2: PROCESS - Execute all processing operations
        // ═══════════════════════════════════════════════════════
        // std::cout << "=== PROCESS PHASE - Cycle " << m_current_cycle << " ===\n";

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

            // std::cout << "  Processing op [" << i << "] type="
            //           << static_cast<int>(op.get_type()) << "\n";

            process_operation(op, m_current_cycle);
            m_data_states[i] = DataState::READY;

            if (m_process_timing == Vruta::DelayContext::BUFFER_BASED && samples_per_operation > 0) {
                co_await SampleDelay { samples_per_operation };
            }
        }

        // ═══════════════════════════════════════════════════════
        // Handle branches (if any)
        // ═══════════════════════════════════════════════════════
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
            // std::cout << "  Waiting for " << current_cycle_sync_tasks.size()
            //           << " synchronous branches\n";

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

        // std::cout << "Cycle " << (m_current_cycle - 1) << " complete\n\n";
    }

    // std::cout << "Pipeline execution complete. Total cycles: " << cycles_executed << "\n";
}

Vruta::SoundRoutine BufferPipeline::execute_streaming(uint64_t max_cycles, uint64_t samples_per_operation)
{
    auto& promise = co_await Kriya::GetPromise {};

    if (m_operations.empty()) {
        co_return;
    }

    m_data_states.resize(m_operations.size(), DataState::EMPTY);
    uint32_t cycles_executed = 0;

    // std::cout << "Starting STREAMING execution strategy\n";

    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (promise.should_terminate) {
            break;
        }

        if (m_cycle_start_callback) {
            m_cycle_start_callback(m_current_cycle);
        }

        // std::cout << "=== STREAMING CYCLE " << m_current_cycle << " ===\n";

        for (size_t i = 0; i < m_operations.size(); ++i) {
            // std::cout << " Checking op [" << i << "]\n";
            auto& op = m_operations[i];

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
                // std::cout << "  Op [" << i << "] iteration " << iter << "\n";

                process_operation(op, m_current_cycle + iter);
                m_data_states[i] = DataState::READY;

                for (size_t j = i + 1; j < m_operations.size(); ++j) {
                    auto& dependent_op = m_operations[j];

                    if (BufferOperation::is_process_phase_operation(dependent_op)) {
                        // std::cout << "    Streaming to dependent op [" << j << "]\n";
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

    // std::cout << "Streaming pipeline complete\n";
}

Vruta::SoundRoutine BufferPipeline::execute_parallel(uint64_t max_cycles, uint64_t samples_per_operation)
{
    // TODO: Implement parallel execution strategy
    std::cout << "PARALLEL strategy not yet implemented, using PHASED\n";
    return execute_phased(max_cycles, samples_per_operation);
}

Vruta::SoundRoutine BufferPipeline::execute_reactive(uint64_t max_cycles, uint64_t samples_per_operation)
{
    // TODO: Implement reactive execution strategy
    std::cout << "REACTIVE strategy not yet implemented, using PHASED\n";
    return execute_phased(max_cycles, samples_per_operation);
}

}
