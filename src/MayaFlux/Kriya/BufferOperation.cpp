#include "BufferOperation.hpp"

#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/FileBridgeBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

BufferOperation BufferOperation::capture_input(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    uint32_t input_channel,
    BufferCapture::CaptureMode mode,
    uint32_t cycle_count)
{
    auto input_buffer = std::make_shared<Buffers::AudioBuffer>(input_channel);
    buffer_manager->register_input_listener(input_buffer, input_channel);
    buffer_manager->add_buffer(input_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, input_channel);

    BufferCapture capture(input_buffer, mode, cycle_count);
    if (mode == BufferCapture::CaptureMode::ACCUMULATE && cycle_count == 0) {
        capture.as_circular(4096);
    }

    return { BufferOperation::OpType::CAPTURE, std::move(capture) };
}

CaptureBuilder BufferOperation::capture_input_from(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    uint32_t input_channel)
{
    auto input_buffer = std::make_shared<Buffers::AudioBuffer>(input_channel);
    buffer_manager->register_input_listener(input_buffer, input_channel);
    buffer_manager->add_buffer(input_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, input_channel);
    return CaptureBuilder(input_buffer);
}

BufferOperation BufferOperation::capture_file(
    const std::string& filepath,
    uint32_t channel,
    uint32_t cycle_count)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto file_buffer = std::make_shared<Buffers::FileBridgeBuffer>(channel, file_container);
    file_buffer->setup_chain_and_processor();

    BufferCapture capture(file_buffer,
        cycle_count > 0 ? BufferCapture::CaptureMode::ACCUMULATE : BufferCapture::CaptureMode::TRANSIENT,
        cycle_count);

    capture.set_processing_control(BufferCapture::ProcessingControl::ON_CAPTURE);

    return { BufferOperation::OpType::CAPTURE, std::move(capture) };
}

CaptureBuilder BufferOperation::capture_file_from(
    const std::string& filepath,
    uint32_t channel)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto file_buffer = std::make_shared<Buffers::FileBridgeBuffer>(channel, file_container);
    file_buffer->setup_chain_and_processor();

    return CaptureBuilder(file_buffer).on_capture_processing();
}

BufferOperation BufferOperation::file_to_stream(
    const std::string& filepath,
    std::shared_ptr<Kakshya::DynamicSoundStream> target_stream,
    uint32_t cycle_count)
{
    auto file_container = MayaFlux::load_audio_file(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto temp_buffer = std::make_shared<Buffers::FileBridgeBuffer>(0, file_container);
    temp_buffer->setup_chain_and_processor();

    BufferOperation op(OpType::ROUTE);
    op.m_source_container = temp_buffer->get_capture_stream();
    op.m_target_container = std::move(target_stream);
    op.m_load_length = cycle_count;
    return op;
}

BufferOperation BufferOperation::transform(TransformationFunction transformer)
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
    uint64_t start_frame,
    uint32_t length)
{
    BufferOperation op(OpType::LOAD);
    op.m_source_container = std::move(source);
    op.m_target_buffer = std::move(target);
    op.m_start_frame = start_frame;
    op.m_load_length = length;
    return op;
}

BufferOperation BufferOperation::when(std::function<bool(uint32_t)> condition)
{
    BufferOperation op(OpType::CONDITION);
    op.m_condition = std::move(condition);
    return op;
}

BufferOperation BufferOperation::dispatch_to(OperationFunction handler)
{
    BufferOperation op(OpType::DISPATCH);
    op.m_dispatch_handler = std::move(handler);
    return op;
}

BufferOperation BufferOperation::fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Buffers::AudioBuffer> target)
{
    BufferOperation op(OpType::FUSE);
    op.m_source_buffers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Kakshya::DynamicSoundStream> target)
{
    BufferOperation op(OpType::FUSE);
    op.m_source_containers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_container = std::move(target);
    return op;
}

BufferOperation BufferOperation::modify_buffer(
    std::shared_ptr<Buffers::AudioBuffer> buffer,
    Buffers::BufferProcessingFunction modifier)
{
    BufferOperation op(OpType::MODIFY);
    op.m_target_buffer = std::move(buffer);
    op.m_buffer_modifier = std::move(modifier);
    return op;
}

CaptureBuilder BufferOperation::capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    return CaptureBuilder(std::move(buffer));
}

BufferOperation& BufferOperation::with_priority(uint8_t priority)
{
    m_priority = priority;
    return *this;
}

BufferOperation& BufferOperation::on_token(Buffers::ProcessingToken token)
{
    m_token = token;
    return *this;
}

BufferOperation& BufferOperation::every_n_cycles(uint32_t n)
{
    m_cycle_interval = n;
    return *this;
}

BufferOperation& BufferOperation::with_tag(const std::string& tag)
{
    m_tag = tag;
    return *this;
}

BufferOperation& BufferOperation::for_cycles(uint32_t count)
{
    if (m_type == OpType::MODIFY) {
        m_modify_cycle_count = count;
    } else if (m_type == OpType::CAPTURE) {
        m_capture.for_cycles(count);
    }
    return *this;
}

BufferOperation& BufferOperation::as_process_phase()
{
    m_execution_phase = ExecutionPhase::PROCESS;
    return *this;
}

BufferOperation& BufferOperation::as_streaming()
{
    m_is_streaming = true;
    return *this;
}

BufferOperation::BufferOperation(OpType type, BufferCapture capture)
    : m_type(type)
    , m_capture(std::move(capture))
    , m_tag(m_capture.get_tag())
{
}

BufferOperation::BufferOperation(OpType type)
    : m_type(type)
    , m_capture(nullptr)
{
}

bool BufferOperation::is_capture_phase_operation(const BufferOperation& op)
{
    if (op.get_execution_phase() == BufferOperation::ExecutionPhase::CAPTURE) {
        return true;
    }
    if (op.get_execution_phase() == BufferOperation::ExecutionPhase::PROCESS) {
        return false;
    }

    switch (op.get_type()) {
    case BufferOperation::OpType::CAPTURE:
        return true;
    case BufferOperation::OpType::MODIFY:
        return op.is_streaming();
    default:
        return false;
    }
}

bool BufferOperation::is_process_phase_operation(const BufferOperation& op)
{
    if (op.get_execution_phase() == BufferOperation::ExecutionPhase::PROCESS) {
        return true;
    }
    if (op.get_execution_phase() == BufferOperation::ExecutionPhase::CAPTURE) {
        return false;
    }

    switch (op.get_type()) {
    case BufferOperation::OpType::MODIFY:
        return !op.is_streaming();
    case BufferOperation::OpType::TRANSFORM:
    case BufferOperation::OpType::ROUTE:
    case BufferOperation::OpType::LOAD:
    case BufferOperation::OpType::DISPATCH:
    case BufferOperation::OpType::FUSE:
        return true;
    case BufferOperation::OpType::CAPTURE:
    case BufferOperation::OpType::CONDITION:
    case BufferOperation::OpType::BRANCH:
    case BufferOperation::OpType::SYNC:
    default:
        return false;
    }
}

}
