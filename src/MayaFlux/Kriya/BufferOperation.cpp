#include "BufferOperation.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/SoundFileBridge.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/Kakshya/Source/DynamicVideoStream.hpp"
#include "MayaFlux/Kakshya/Source/VideoFileContainer.hpp"
#include "MayaFlux/Kriya/PipelineHelpers/PipelineGraphicsData.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

std::shared_ptr<Kakshya::DynamicVideoStream> make_ring(
    const std::shared_ptr<Buffers::VideoContainerBuffer>& source,
    uint64_t ring_frames)
{
    const uint64_t frames = ring_frames > 0 ? ring_frames : 3ULL * Vruta::s_registered_frame_rate;

    return std::make_shared<Kakshya::DynamicVideoStream>(Kakshya::VideoStreamSpec {
        .width = source->get_width(),
        .height = source->get_height(),
        .format = source->get_format(),
        .frame_rate = static_cast<double>(Vruta::s_registered_frame_rate),
        .ring_frames = frames });
}

Buffers::ProcessingToken capture_token(const BufferCapture& capture)
{
    if (capture.get_audio_buffer()) {
        return Buffers::ProcessingToken::AUDIO_BACKEND;
    }
    if (capture.get_graphics_buffer()) {
        return Buffers::ProcessingToken::GRAPHICS_BACKEND;
    }
    throw std::invalid_argument("Capture operation requires a source buffer");
}

}

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

BufferOperation BufferOperation::capture_camera(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const IO::CameraConfig& config,
    BufferCapture::CaptureMode mode,
    uint32_t cycle_count,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    auto camera = io_manager->open_camera(config);
    if (!camera) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to open camera");
    }

    auto buffer = io_manager->hook_camera_to_buffer(camera);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook camera to graphics buffer");
    }

    if (render) {
        detail::ensure_rendering(buffer, *render);
    }

    return { OpType::CAPTURE, BufferCapture(buffer, mode, cycle_count) };
}

CaptureBuilder BufferOperation::capture_camera_from(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const IO::CameraConfig& config,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    auto camera = io_manager->open_camera(config);
    if (!camera) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to open camera");
    }

    auto buffer = io_manager->hook_camera_to_buffer(camera);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook camera to graphics buffer");
    }

    if (render) {
        detail::ensure_rendering(buffer, *render);
    }

    return CaptureBuilder(buffer);
}

BufferOperation BufferOperation::capture_file(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    uint32_t channel,
    uint32_t cycle_count)
{
    auto file_container = io_manager->load_audio(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto file_buffer = std::make_shared<Buffers::SoundFileBridge>(channel, file_container);
    file_buffer->setup_processors(Buffers::ProcessingToken::AUDIO_BACKEND);

    BufferCapture capture(file_buffer,
        cycle_count > 0 ? BufferCapture::CaptureMode::ACCUMULATE : BufferCapture::CaptureMode::TRANSIENT,
        cycle_count);

    capture.set_processing_control(BufferCapture::ProcessingControl::ON_CAPTURE);

    return { BufferOperation::OpType::CAPTURE, std::move(capture) };
}

BufferOperation BufferOperation::capture_file(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    IO::LoadConfig config,
    uint32_t cycle_count,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    auto video = io_manager->load_video(filepath, config).video;
    if (!video) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to load video file: {}", filepath);
    }

    auto buffer = io_manager->hook_video_container_to_buffer(video);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook video file to graphics buffer: {}", filepath);
    }

    if (render) {
        detail::ensure_rendering(buffer, *render);
    }

    return { OpType::CAPTURE, BufferCapture(buffer,
        cycle_count > 1 ? BufferCapture::CaptureMode::ACCUMULATE : BufferCapture::CaptureMode::TRANSIENT,
        cycle_count) };
}

CaptureBuilder BufferOperation::capture_file_from(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    uint32_t channel)
{
    auto file_container = io_manager->load_audio(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto file_buffer = std::make_shared<Buffers::SoundFileBridge>(channel, file_container);
    file_buffer->setup_processors(Buffers::ProcessingToken::AUDIO_BACKEND);

    return CaptureBuilder(file_buffer).on_capture_processing();
}

CaptureBuilder BufferOperation::capture_file_from(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    IO::LoadConfig config,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    auto video = io_manager->load_video(filepath, config).video;
    if (!video) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to load video file: {}", filepath);
    }

    auto buffer = io_manager->hook_video_container_to_buffer(video);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook video file to graphics buffer: {}", filepath);
    }

    if (render) {
        detail::ensure_rendering(buffer, *render);
    }

    return CaptureBuilder(buffer);
}

BufferOperation BufferOperation::file_to_stream(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    std::shared_ptr<Kakshya::DynamicSoundStream> target_stream,
    uint32_t cycle_count)
{
    auto file_container = io_manager->load_audio(filepath);
    if (!file_container) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO, std::source_location::current(),
            "Failed to load audio file: {}", filepath);
    }

    auto temp_buffer = std::make_shared<Buffers::SoundFileBridge>(0, file_container);
    temp_buffer->setup_processors(Buffers::ProcessingToken::AUDIO_BACKEND);

    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_source_audio_stream = temp_buffer->get_capture_stream();
    op.m_target_audio_stream = std::move(target_stream);
    op.m_load_length = cycle_count;
    return op;
}

BufferOperation BufferOperation::file_to_stream(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    std::shared_ptr<Kakshya::DynamicVideoStream> target_stream,
    IO::LoadConfig config,
    uint32_t cycle_count,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    auto video = io_manager->load_video(filepath, config).video;
    if (!video) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to load video file: {}", filepath);
    }

    if (render) {
        auto buffer = io_manager->hook_video_container_to_buffer(video);
        if (!buffer) {
            error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
                std::source_location::current(), "Failed to hook video file to graphics buffer: {}", filepath);
        }
        detail::ensure_rendering(buffer, *render);
    }

    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_stream = std::move(video);
    op.m_target_graphics_stream = std::move(target_stream);
    op.m_load_length = cycle_count;
    return op;
}

BufferOperation BufferOperation::capture_to_stream(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const IO::CameraConfig& config,
    uint64_t ring_frames,
    std::optional<Portal::Graphics::RenderConfig> live,
    std::optional<Portal::Graphics::RenderConfig> display)
{
    auto camera = io_manager->open_camera(config);
    if (!camera) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to open camera");
    }

    auto buffer = io_manager->hook_camera_to_buffer(camera);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook camera to graphics buffer");
    }

    if (live) {
        detail::ensure_rendering(buffer, *live);
    }

    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_buffer = buffer;
    op.m_target_graphics_stream = make_ring(buffer, ring_frames);
    op.m_render = std::move(display);
    return op;
}

BufferOperation BufferOperation::capture_to_stream(
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::string& filepath,
    IO::LoadConfig config,
    uint64_t ring_frames,
    std::optional<Portal::Graphics::RenderConfig> live,
    std::optional<Portal::Graphics::RenderConfig> display)
{
    auto video = io_manager->load_video(filepath, config).video;
    if (!video) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to load video file: {}", filepath);
    }

    auto buffer = io_manager->hook_video_container_to_buffer(video);
    if (!buffer) {
        error<std::runtime_error>(Journal::Component::Kriya, Journal::Context::AsyncIO,
            std::source_location::current(), "Failed to hook video file to graphics buffer: {}", filepath);
    }

    if (live) {
        detail::ensure_rendering(buffer, *live);
    }

    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_buffer = buffer;
    op.m_target_graphics_stream = make_ring(buffer, ring_frames);
    op.m_render = std::move(display);
    return op;
}

BufferOperation BufferOperation::transform(TransformationFunction transformer,
    Buffers::ProcessingToken token)
{
    BufferOperation op(OpType::TRANSFORM, token);
    op.m_transformer = std::move(transformer);
    return op;
}

BufferOperation BufferOperation::route_to_buffer(std::shared_ptr<Buffers::AudioBuffer> target)
{
    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_target_audio_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::route_to_buffer(std::shared_ptr<Buffers::VKBuffer> target)
{
    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_target_graphics_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::route_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> target, uint32_t channel)
{
    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_target_audio_stream = std::move(target);
    op.m_target_audio_channel = channel;
    return op;
}

BufferOperation BufferOperation::route_to_container(
    std::shared_ptr<Kakshya::DynamicVideoStream> target,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    BufferOperation op(OpType::ROUTE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_target_graphics_stream = std::move(target);
    op.m_render = std::move(render);
    return op;
}

BufferOperation BufferOperation::load_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> source,
    std::shared_ptr<Buffers::AudioBuffer> target,
    uint64_t start_frame,
    uint32_t length)
{
    BufferOperation op(OpType::LOAD, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_source_audio_stream = std::move(source);
    op.m_target_audio_buffer = std::move(target);
    op.m_start_frame = start_frame;
    op.m_load_length = length;
    return op;
}

BufferOperation BufferOperation::load_from_container(
    std::shared_ptr<Kakshya::DynamicVideoStream> source,
    std::shared_ptr<Buffers::VKBuffer> target,
    uint64_t start_frame,
    uint32_t length)
{
    BufferOperation op(OpType::LOAD, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_stream = std::move(source);
    op.m_target_graphics_buffer = std::move(target);
    op.m_start_frame = start_frame;
    op.m_load_length = length;
    return op;
}

BufferOperation BufferOperation::when(std::function<bool(uint32_t)> condition,
    Buffers::ProcessingToken token)
{
    BufferOperation op(OpType::CONDITION, token);
    op.m_condition = std::move(condition);
    return op;
}

BufferOperation BufferOperation::dispatch_to(OperationFunction handler,
    Buffers::ProcessingToken token)
{
    BufferOperation op(OpType::DISPATCH, token);
    op.m_dispatch_handler = std::move(handler);
    return op;
}

BufferOperation BufferOperation::fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Buffers::AudioBuffer> target)
{
    BufferOperation op(OpType::FUSE, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_source_audio_buffers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_audio_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::fuse_data(
    std::vector<std::shared_ptr<Buffers::VKBuffer>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Buffers::VKBuffer> target)
{
    BufferOperation op(OpType::FUSE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_buffers = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_graphics_buffer = std::move(target);
    return op;
}

BufferOperation BufferOperation::fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Kakshya::DynamicSoundStream> target)
{
    BufferOperation op(OpType::FUSE, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_source_audio_streams = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_audio_stream = std::move(target);
    return op;
}

BufferOperation BufferOperation::fuse_containers(
    std::vector<std::shared_ptr<Kakshya::DynamicVideoStream>> sources,
    TransformVectorFunction fusion_func,
    std::shared_ptr<Kakshya::DynamicVideoStream> target)
{
    BufferOperation op(OpType::FUSE, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_source_graphics_streams = std::move(sources);
    op.m_fusion_function = std::move(fusion_func);
    op.m_target_graphics_stream = std::move(target);
    return op;
}

BufferOperation BufferOperation::modify_buffer(
    std::shared_ptr<Buffers::AudioBuffer> buffer,
    Buffers::AudioProcessingFunction modifier)
{
    BufferOperation op(OpType::MODIFY, Buffers::ProcessingToken::AUDIO_BACKEND);
    op.m_target_audio_buffer = std::move(buffer);
    op.m_audio_buffer_modifier = std::move(modifier);
    return op;
}

BufferOperation BufferOperation::modify_buffer(
    std::shared_ptr<Buffers::VKBuffer> buffer,
    Buffers::GraphicsProcessingFunction modifier)
{
    BufferOperation op(OpType::MODIFY, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    op.m_target_graphics_buffer = std::move(buffer);
    op.m_graphics_buffer_modifier = std::move(modifier);
    return op;
}

CaptureBuilder BufferOperation::capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    return CaptureBuilder(std::move(buffer));
}

CaptureBuilder BufferOperation::capture_from(
    std::shared_ptr<Buffers::VKBuffer> buffer,
    std::optional<Portal::Graphics::RenderConfig> render)
{
    if (render) {
        detail::ensure_rendering(buffer, *render);
    }

    return CaptureBuilder(std::move(buffer));
}

CaptureBuilder BufferOperation::capture_from(std::nullptr_t)
{
    return CaptureBuilder(nullptr);
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
    , m_token(capture_token(m_capture))
    , m_tag(m_capture.get_tag())
{
}

BufferOperation::BufferOperation(OpType type, Buffers::ProcessingToken token)
    : m_type(type)
    , m_token(token)
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
