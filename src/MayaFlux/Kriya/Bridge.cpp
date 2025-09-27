#include "Bridge.hpp"

#include "MayaFlux/API/Depot.hpp"

#include "MayaFlux/Buffers/Container/FileBridgeBuffer.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kriya/Awaiters.hpp"

namespace MayaFlux::Kriya {

BufferOperation BufferOperation::capture_input(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    u_int32_t input_channel,
    BufferCapture::CaptureMode mode,
    u_int32_t cycle_count)
{
    // auto input_buffer = MayaFlux::create_input_listener_buffer(input_channel, true);
    auto input_buffer = std::make_shared<Buffers::AudioBuffer>(input_channel);
    buffer_manager->register_input_listener(input_buffer, input_channel);
    buffer_manager->add_audio_buffer(input_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, input_channel);

    BufferCapture capture(input_buffer, mode, cycle_count);
    if (mode == BufferCapture::CaptureMode::ACCUMULATE && cycle_count == 0) {
        capture.as_circular(4096);
    }

    return { BufferOperation::OpType::CAPTURE, std::move(capture) };
}

CaptureBuilder BufferOperation::capture_input_from(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    u_int32_t input_channel)
{
    auto input_buffer = std::make_shared<Buffers::AudioBuffer>(input_channel);
    buffer_manager->register_input_listener(input_buffer, input_channel);
    buffer_manager->add_audio_buffer(input_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, input_channel);
    return CaptureBuilder(input_buffer);
}

BufferOperation BufferOperation::capture_file(
    const std::string& filepath,
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    u_int32_t channel,
    u_int32_t cycle_count)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        throw std::runtime_error("Failed to load audio file: " + filepath);
    }

    auto file_buffer = std::make_shared<Buffers::FileBridgeBuffer>(channel, file_container);

    // buffer_manager->add_audio_buffer(file_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);

    BufferCapture capture(file_buffer,
        cycle_count > 0 ? BufferCapture::CaptureMode::ACCUMULATE : BufferCapture::CaptureMode::TRANSIENT,
        cycle_count);

    return { BufferOperation::OpType::CAPTURE, std::move(capture) };
}

CaptureBuilder BufferOperation::capture_file_from(
    const std::string& filepath,
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    u_int32_t channel)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        throw std::runtime_error("Failed to load audio file: " + filepath);
    }

    auto file_buffer = std::make_shared<Buffers::FileBridgeBuffer>(channel, file_container);
    // buffer_manager->add_audio_buffer(file_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);

    return CaptureBuilder(file_buffer);
}

BufferOperation BufferOperation::file_to_stream(
    const std::string& filepath,
    std::shared_ptr<Kakshya::DynamicSoundStream> target_stream,
    u_int32_t cycle_count)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        throw std::runtime_error("Failed to load audio file: " + filepath);
    }

    auto temp_buffer = std::make_shared<Buffers::FileBridgeBuffer>(0, file_container);

    BufferOperation op(OpType::ROUTE);
    op.m_source_container = temp_buffer->get_capture_stream();
    op.m_target_container = target_stream;
    op.m_load_length = cycle_count;
    return op;
}

BufferOperation BufferOperation::transform(std::function<Kakshya::DataVariant(const Kakshya::DataVariant&, u_int32_t)> transformer)
{
    BufferOperation op(OpType::TRANSFORM);
    op.m_transformer = std::move(transformer);
    return op;
}

BufferOperation BufferOperation::route_to_buffer(std::shared_ptr<Buffers::AudioBuffer> target)
{
    BufferOperation op(OpType::ROUTE);
    op.m_target_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::route_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> target)
{
    BufferOperation op(OpType::ROUTE);
    op.m_target_container = std::move(target);
    return op;
}

BufferOperation BufferOperation::load_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> source,
    std::shared_ptr<Buffers::AudioBuffer> target,
    u_int64_t start_frame,
    u_int32_t length)
{
    BufferOperation op(OpType::LOAD);
    op.m_source_container = std::move(source);
    op.m_target_buffer = std::move(target);
    op.m_start_frame = start_frame;
    op.m_load_length = length;
    return op;
}

BufferOperation BufferOperation::when(std::function<bool(u_int32_t)> condition)
{
    BufferOperation op(OpType::CONDITION);
    op.m_condition = std::move(condition);
    return op;
}

BufferOperation BufferOperation::dispatch_to(std::function<void(const Kakshya::DataVariant&, u_int32_t)> handler)
{
    BufferOperation op(OpType::DISPATCH);
    op.m_dispatch_handler = std::move(handler);
    return op;
}

BufferOperation BufferOperation::fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
    std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, u_int32_t)> fusion_func,
    std::shared_ptr<Buffers::AudioBuffer> target)
{
    BufferOperation op(OpType::FUSE);
    op.m_source_buffers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
    std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, u_int32_t)> fusion_func,
    std::shared_ptr<Kakshya::DynamicSoundStream> target)
{
    BufferOperation op(OpType::FUSE);
    op.m_source_containers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_container = std::move(target);
    return op;
}

CaptureBuilder BufferOperation::capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    return CaptureBuilder(buffer);
}

BufferOperation& BufferOperation::with_priority(u_int8_t priority)
{
    m_priority = priority;
    return *this;
}

BufferOperation& BufferOperation::on_token(Buffers::ProcessingToken token)
{
    m_token = token;
    return *this;
}

BufferOperation& BufferOperation::every_n_cycles(u_int32_t n)
{
    m_cycle_interval = n;
    return *this;
}

BufferOperation& BufferOperation::with_tag(const std::string& tag)
{
    m_tag = tag;
    return *this;
}

BufferPipeline& BufferPipeline::branch_if(std::function<bool(u_int32_t)> condition,
    const std::function<void(BufferPipeline&)>& branch_builder)
{
    m_branches.emplace_back(std::move(condition), BufferPipeline {});
    branch_builder(m_branches.back().second);
    return *this;
}

BufferPipeline& BufferPipeline::parallel(std::initializer_list<BufferOperation> operations)
{
    for (auto& op : operations) {
        BufferOperation parallel_op = std::move(const_cast<BufferOperation&>(op));
        parallel_op.with_priority(255);
        m_operations.emplace_back(std::move(parallel_op));
    }
    return *this;
}

BufferPipeline& BufferPipeline::with_lifecycle(
    std::function<void(u_int32_t)> on_cycle_start,
    std::function<void(u_int32_t)> on_cycle_end)
{
    m_cycle_start_callback = on_cycle_start;
    m_cycle_end_callback = on_cycle_end;
    return *this;
}

void BufferPipeline::execute_continuous()
{
    m_continuous_execution = true;
    execute_internal(0);
}

void BufferPipeline::mark_data_consumed(u_int32_t operation_index)
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

void BufferPipeline::execute_internal(u_int32_t max_cycles)
{
    if (m_operations.empty())
        return;

    m_data_states.resize(m_operations.size(), DataState::EMPTY);

    u_int32_t cycles_executed = 0;
    while ((max_cycles == 0 || cycles_executed < max_cycles) && (m_continuous_execution || cycles_executed < max_cycles)) {

        if (m_cycle_start_callback) {
            m_cycle_start_callback(m_current_cycle);
        }

        for (size_t i = 0; i < m_operations.size(); ++i) {
            if (m_operations[i].get_type() != BufferOperation::OpType::CONDITION || (m_operations[i].m_condition && m_operations[i].m_condition(m_current_cycle))) {

                if (m_current_cycle % m_operations[i].m_cycle_interval == 0) {
                    process_operation(m_operations[i], m_current_cycle);
                    m_data_states[i] = DataState::READY;
                }
            }
        }

        process_branches(m_current_cycle);

        cleanup_expired_data();

        if (m_cycle_end_callback) {
            m_cycle_end_callback(m_current_cycle);
        }

        m_current_cycle++;
        cycles_executed++;

        if (!m_continuous_execution && cycles_executed >= max_cycles) {
            break;
        }
    }
}

void BufferPipeline::process_operation(BufferOperation& op, u_int32_t cycle)
{
    try {
        switch (op.get_type()) {
        case BufferOperation::OpType::CAPTURE: {
            auto buffer_data = extract_buffer_data(op.m_capture.get_buffer());

            if (op.m_capture.m_data_ready_callback) {
                op.m_capture.m_data_ready_callback(buffer_data, cycle);
            }

            m_operation_data[&op] = buffer_data;
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
                auto buffer_data = extract_buffer_data(source_buffer);
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

        case BufferOperation::OpType::CONDITION:
            break;

        default:
            std::cerr << "Unknown operation type in pipeline\n";
            break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing operation: " << e.what() << '\n';
    }
}

void BufferPipeline::process_branches(u_int32_t cycle)
{
    for (auto& branch : m_branches) {
        if (branch.first(cycle)) {
            branch.second.execute_once();
        }
    }
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

Kakshya::DataVariant BufferPipeline::extract_buffer_data(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (audio_buffer) {
        const auto& data_span = audio_buffer->get_data();
        std::vector<double> data_vector(data_span.begin(), data_span.end());
        return data_vector;
    }

    return std::vector<double> {};
}

void BufferPipeline::write_to_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, const Kakshya::DataVariant& data)
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
            std::cerr << "Data type mismatch when writing to audio buffer: " << e.what() << '\n';
        }
    }

    // TODO: Handle other buffer types
}

void BufferPipeline::write_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> container, const Kakshya::DataVariant& data)
{
    try {
        auto audio_data = std::get<std::vector<double>>(data);
        std::span<const double> data_span(audio_data.data(), audio_data.size());

        container->write_frames(data_span, 0);

    } catch (const std::bad_variant_access& e) {
        std::cerr << "Data type mismatch when writing to container: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error writing to container: " << e.what() << '\n';
    }
}

Kakshya::DataVariant BufferPipeline::read_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> container,
    u_int64_t start_frame,
    u_int32_t length)
{
    try {
        u_int32_t read_length = length;
        if (read_length == 0) {
            read_length = static_cast<u_int32_t>(container->get_total_elements() / container->get_num_channels());
        }

        std::vector<double> output_data(read_length * container->get_num_channels());
        std::span<double> output_span(output_data.data(), output_data.size());

        u_int64_t frames_read = container->read_frames(output_span, read_length);

        if (frames_read < output_data.size()) {
            output_data.resize(frames_read);
        }

        return output_data;

    } catch (const std::exception& e) {
        std::cerr << "Error reading from container: " << e.what() << '\n';
        return std::vector<double> {};
    }
}

CycleCoordinator::CycleCoordinator(Vruta::TaskScheduler& scheduler)
    : m_scheduler(scheduler)
{
}

Vruta::SoundRoutine CycleCoordinator::sync_pipelines(
    std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
    u_int32_t sync_every_n_cycles)
{

    auto& promise = co_await GetPromise {};
    u_int32_t cycle = 0;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        if (cycle % sync_every_n_cycles == 0) {
            for (auto& pipeline_ref : pipelines) {
                auto& pipeline = pipeline_ref.get();
                if (pipeline.has_pending_data()) {
                    std::cout << "Sync point: Pipeline has stale data at cycle " << cycle << '\n';
                }
            }
        }

        for (auto& pipeline_ref : pipelines) {
            pipeline_ref.get().execute_once();
        }

        cycle++;
        co_await SampleDelay { 1 };
    }
}

Vruta::SoundRoutine CycleCoordinator::manage_transient_data(
    std::shared_ptr<Buffers::AudioBuffer> buffer,
    std::function<void(u_int32_t)> on_data_ready,
    std::function<void(u_int32_t)> on_data_expired)
{

    auto& promise = co_await GetPromise {};
    u_int32_t cycle = 0;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        if (buffer->has_data_for_cycle()) {
            on_data_ready(cycle);
            co_await SampleDelay { 1 };

            if (buffer->has_data_for_cycle()) {
                on_data_expired(cycle + 1);
            }
        }

        cycle++;
        co_await SampleDelay { 1 };
    }
}
}
