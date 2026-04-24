#include "IOManager.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Processors/FrameAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/CameraContainer.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoFileContainer.hpp"

#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextBuffer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/IOService.hpp"

#include "ImageExport.hpp"
#include "ModelReader.hpp"
#include "STBImageWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include <libavdevice/avdevice.h>
}

namespace MayaFlux::IO {

namespace {

    TextureResolver make_default_resolver(const std::string& filepath)
    {
        const auto base_dir = std::filesystem::path(filepath).parent_path();
        return [base_dir](const std::string& raw) -> std::shared_ptr<Core::VKImage> {
            auto tex_path = FileReader::resolve_path((base_dir / raw).generic_string());
            return IO::ImageReader::load_texture(tex_path);
        };
    }

}

IOManager::IOManager(uint64_t sample_rate, uint32_t buffer_size, uint32_t frame_rate, const std::shared_ptr<Buffers::BufferManager>& buffer_manager)
    : m_sample_rate(sample_rate)
    , m_buffer_size(buffer_size)
    , m_frame_rate(frame_rate)
    , m_buffer_manager(buffer_manager)
{
    m_io_service = std::make_shared<Registry::Service::IOService>();

    m_io_service->request_decode = [this](uint64_t reader_id) {
        dispatch_decode_request(reader_id);
    };

    Registry::BackendRegistry::instance()
        .register_service<Registry::Service::IOService>(
            [svc = m_io_service]() -> void* { return svc.get(); });

    m_io_service->request_frame = [this](uint64_t reader_id) {
        dispatch_frame_request(reader_id);
    };

    STBImageWriter::register_with_registry();

    MF_INFO(Journal::Component::Core, Journal::Context::Init, "IOManager initialised");
}

IOManager::~IOManager()
{
    Registry::BackendRegistry::instance()
        .unregister_service<Registry::Service::IOService>();

    m_io_service.reset();

    {
        std::unique_lock lock(m_readers_mutex);
        m_video_readers.clear();
    }

    {
        std::unique_lock lock(m_camera_mutex);
        m_camera_readers.clear();
    }

    {
        std::unique_lock lock(m_buffers_mutex);
        m_video_buffers.clear();
        m_audio_buffers.clear();
    }

    MF_INFO(Journal::Component::Core, Journal::Context::Init, "IOManager destroyed");
}

std::shared_ptr<Kakshya::VideoFileContainer>
IOManager::load_video(const std::string& filepath)
{
    return load_video(filepath, {}).video;
}

VideoLoadResult
IOManager::load_video(const std::string& filepath, LoadConfig config)
{
    auto reader = std::make_shared<IO::VideoFileReader>();

    if (!reader->can_read(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "Cannot read video file: {}", filepath);
        return {};
    }

    reader->set_video_options(config.video_options);
    reader->set_audio_options(config.audio_options);
    reader->set_target_dimensions(config.target_width, config.target_height);

    if ((config.video_options & VideoReadOptions::EXTRACT_AUDIO) == VideoReadOptions::EXTRACT_AUDIO) {
        reader->set_target_sample_rate(m_sample_rate);
    }

    if (!reader->open(filepath, config.file_options)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "Failed to open video file: {}", reader->get_last_error());
        return {};
    }

    const uint64_t reader_id = register_video_reader(reader);
    reader->setup_io_service(reader_id);

    auto video_container = std::dynamic_pointer_cast<Kakshya::VideoFileContainer>(reader->create_container());

    if (!video_container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "Failed to create video container from: {}", filepath);
        release_video_reader(reader_id);
        return {};
    }

    if (!reader->load_into_container(video_container)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "Failed to load video data: {}", reader->get_last_error());
        release_video_reader(reader_id);
        return {};
    }

    configure_frame_processor(video_container);

    VideoLoadResult result;
    result.video = video_container;

    if ((config.video_options & IO::VideoReadOptions::EXTRACT_AUDIO) == IO::VideoReadOptions::EXTRACT_AUDIO) {
        auto audio_container = reader->get_audio_container();
        if (audio_container) {
            configure_audio_processor(audio_container);
            result.audio = audio_container;
            m_extracted_audio[video_container] = audio_container;
        } else {
            MF_WARN(Journal::Component::API, Journal::Context::FileIO,
                "No audio track found in: {}", filepath);
        }
    }

    MF_INFO(Journal::Component::API, Journal::Context::FileIO,
        "Loaded video: {}", filepath);

    return result;
}

uint64_t IOManager::register_video_reader(std::shared_ptr<IO::VideoFileReader> reader)
{
    if (!reader) {
        MF_WARN(Journal::Component::Core, Journal::Context::FileIO,
            "IOManager::register_video_reader called with null reader");
        return 0;
    }

    const uint64_t id = m_next_reader_id.fetch_add(1, std::memory_order_relaxed);
    reader->set_reader_id(id);

    {
        std::unique_lock lock(m_readers_mutex);
        m_video_readers.emplace(id, std::move(reader));
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::FileIO,
        "IOManager: registered VideoFileReader id={}", id);

    return id;
}

void IOManager::release_video_reader(uint64_t reader_id)
{
    std::unique_lock lock(m_readers_mutex);
    auto it = m_video_readers.find(reader_id);

    if (it == m_video_readers.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::FileIO,
            "IOManager::release_video_reader: unknown id={}", reader_id);
        return;
    }

    m_video_readers.erase(it);

    MF_DEBUG(Journal::Component::Core, Journal::Context::FileIO,
        "IOManager: released VideoFileReader id={}", reader_id);
}

void IOManager::dispatch_decode_request(uint64_t reader_id)
{
    std::shared_lock lock(m_readers_mutex);
    auto it = m_video_readers.find(reader_id);

    if (it == m_video_readers.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::AsyncIO,
            "IOManager: dispatch_decode_request unknown reader_id={}", reader_id);
        return;
    }

    it->second->signal_decode();
}

void IOManager::dispatch_frame_request(uint64_t reader_id)
{
    std::shared_lock lock(m_camera_mutex);
    auto it = m_camera_readers.find(reader_id);

    if (it == m_camera_readers.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::AsyncIO,
            "IOManager: dispatch_frame_request unknown reader_id={}", reader_id);
        return;
    }

    it->second->pull_frame_all();
}

std::shared_ptr<Kakshya::SoundFileContainer> IOManager::load_audio(const std::string& filepath, LoadConfig config)
{
    auto reader = std::make_shared<IO::SoundFileReader>();

    if (!reader->can_read(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO, "Cannot read file: {}", filepath);
        return nullptr;
    }

    reader->set_target_sample_rate(m_sample_rate);
    reader->set_audio_options(config.audio_options);

    if (!reader->open(filepath, config.file_options)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO, "Failed to open file: {}", reader->get_last_error());
        return nullptr;
    }

    auto container = reader->create_container();
    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sound_container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime, "Failed to create sound container");
        return nullptr;
    }

    if (!reader->load_into_container(sound_container)) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime, "Failed to load audio data: {}", reader->get_last_error());
        return nullptr;
    }

    configure_audio_processor(sound_container);

    m_audio_readers.push_back(std::move(reader));

    return sound_container;
}

std::shared_ptr<Kakshya::DynamicSoundStream> IOManager::load_audio_bounded(
    const std::string& filepath,
    uint64_t max_frames,
    bool truncate)
{
    auto reader = std::make_shared<IO::SoundFileReader>();

    if (!reader->can_read(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_bounded: unsupported format '{}'", filepath);
        return nullptr;
    }

    reader->set_target_sample_rate(m_sample_rate);

    auto stream = reader->load_bounded(filepath, max_frames, truncate);
    if (!stream) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_bounded: failed for '{}'", filepath);
        return nullptr;
    }

    stream->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    m_audio_readers.push_back(std::move(reader));

    return stream;
}

std::shared_ptr<Kakshya::CameraContainer>
IOManager::open_camera(const CameraConfig& config)
{
    static std::once_flag s_avdevice_init;
    std::call_once(s_avdevice_init, [] { avdevice_register_all(); });

    auto reader = std::make_shared<CameraReader>();

    if (!reader->open(config)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "open_camera: failed — {}", reader->last_error());
        return nullptr;
    }

    auto container = reader->create_container();
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "open_camera: failed to create container — {}",
            reader->last_error());
        return nullptr;
    }

    container->create_default_processor();

    uint64_t rid = m_next_reader_id.fetch_add(1, std::memory_order_relaxed);
    reader->set_container(container);

    {
        std::unique_lock lock(m_camera_mutex);
        m_camera_readers[rid] = reader;
    }

    container->setup_io(rid);
    container->mark_ready_for_processing(true);

    MF_INFO(Journal::Component::API, Journal::Context::FileIO,
        "open_camera: reader_id={} device='{}' {}x{} @{:.1f}fps",
        rid, config.device_name,
        reader->width(), reader->height(), reader->frame_rate());

    return container;
}

std::shared_ptr<Buffers::TextureBuffer>
IOManager::load_image(const std::string& filepath)
{
    auto reader = std::make_shared<IO::ImageReader>();

    if (!reader->open(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "Failed to open image: {}", filepath);
        return nullptr;
    }

    auto texture_buffer = reader->create_texture_buffer();

    if (!texture_buffer) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "Failed to create texture buffer from: {}", filepath);
        return nullptr;
    }

    m_image_readers.push_back(std::move(reader));

    MF_INFO(Journal::Component::API, Journal::Context::FileIO,
        "Loaded image: {} ({}x{})",
        std::filesystem::path(filepath).filename().string(),
        texture_buffer->get_width(),
        texture_buffer->get_height());

    return texture_buffer;
}

std::vector<std::shared_ptr<Buffers::MeshBuffer>>
IOManager::load_mesh(const std::string& filepath, TextureResolver resolver)
{
    auto reader = std::make_shared<ModelReader>();

    if (!reader->can_read(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh: unsupported format '{}'", filepath);
        return {};
    }

    if (!reader->open(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh: failed to open '{}' — {}",
            filepath, reader->get_last_error());
        return {};
    }

    if (!resolver)
        resolver = make_default_resolver(filepath);

    auto buffers = reader->create_mesh_buffers(resolver);
    reader->close();

    if (buffers.empty()) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh: no meshes in '{}'", filepath);
        return {};
    }

    MF_INFO(Journal::Component::API, Journal::Context::FileIO,
        "IOManager::load_mesh: {} mesh(es) from '{}'",
        buffers.size(),
        std::filesystem::path(filepath).filename().string());

    return buffers;
}

std::shared_ptr<Nodes::Network::MeshNetwork>
IOManager::load_mesh_network(const std::string& filepath, TextureResolver resolver)
{
    auto reader = std::make_shared<ModelReader>();

    if (!reader->can_read(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh_network: unsupported format '{}'", filepath);
        return nullptr;
    }

    if (!reader->open(filepath)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh_network: failed to open '{}' — {}",
            filepath, reader->get_last_error());
        return nullptr;
    }

    if (!resolver)
        resolver = make_default_resolver(filepath);

    auto net = reader->create_mesh_network(resolver);
    reader->close();

    if (!net) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "IOManager::load_mesh_network: no network from '{}'", filepath);
        return nullptr;
    }

    MF_INFO(Journal::Component::API, Journal::Context::FileIO,
        "IOManager::load_mesh_network: {} slots from '{}'",
        net->slot_count(),
        std::filesystem::path(filepath).filename().string());

    return net;
}

void IOManager::configure_frame_processor(
    const std::shared_ptr<Kakshya::VideoFileContainer>& container)
{
    auto existing = std::dynamic_pointer_cast<Kakshya::FrameAccessProcessor>(
        container->get_default_processor());

    if (existing) {
        existing->set_global_fps(m_frame_rate);
        existing->set_auto_advance(true);
        MF_DEBUG(Journal::Component::API, Journal::Context::ContainerProcessing,
            "Configured existing FrameAccessProcessor");
    } else {
        auto processor = std::make_shared<Kakshya::FrameAccessProcessor>();
        processor->set_global_fps(m_frame_rate);
        processor->set_auto_advance(true);
        container->set_default_processor(processor);
        MF_DEBUG(Journal::Component::API, Journal::Context::ContainerProcessing,
            "Created and set FrameAccessProcessor");
    }
}

void IOManager::configure_audio_processor(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container)
{
    container->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    const std::vector<uint64_t> output_shape = {
        m_buffer_size,
        container->get_num_channels()
    };

    auto existing = std::dynamic_pointer_cast<Kakshya::ContiguousAccessProcessor>(
        container->get_default_processor());

    if (existing) {
        existing->set_output_size(output_shape);
        existing->set_auto_advance(true);
        MF_DEBUG(Journal::Component::API, Journal::Context::ContainerProcessing,
            "Configured existing ContiguousAccessProcessor");
    } else {
        auto processor = std::make_shared<Kakshya::ContiguousAccessProcessor>();
        processor->set_output_size(output_shape);
        processor->set_auto_advance(true);
        container->set_default_processor(processor);
        MF_DEBUG(Journal::Component::API, Journal::Context::ContainerProcessing,
            "Created and set ContiguousAccessProcessor");
    }
}

std::shared_ptr<Buffers::VideoContainerBuffer>
IOManager::hook_video_container_to_buffer(
    const std::shared_ptr<Kakshya::VideoFileContainer>& container)
{
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "hook_video_container_to_buffer: null container");
        return nullptr;
    }

    auto stream_container = std::dynamic_pointer_cast<Kakshya::StreamContainer>(container);

    if (!stream_container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "hook_video_container_to_buffer: container is not a VideoStreamContainer");
        return nullptr;
    }

    auto video_buffer = m_buffer_manager->create_graphics_buffer<Buffers::VideoContainerBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        stream_container);

    {
        std::unique_lock lock(m_buffers_mutex);
        m_video_buffers[container] = video_buffer;
    }

    MF_INFO(Journal::Component::API, Journal::Context::BufferManagement,
        "Hooked VideoFileContainer to VideoContainerBuffer ({}x{})",
        video_buffer->get_width(), video_buffer->get_height());

    return video_buffer;
}

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>
IOManager::hook_audio_container_to_buffers(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container)
{
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "hook_audio_container_to_buffers: null container");
        return {};
    }

    uint32_t num_channels = container->get_num_channels();
    std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> created_buffers;

    MF_TRACE(
        Journal::Component::API,
        Journal::Context::BufferManagement,
        "Setting up audio playback for {} channels...",
        num_channels);

    for (uint32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = m_buffer_manager->create_audio_buffer<MayaFlux::Buffers::SoundContainerBuffer>(
            MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();

        created_buffers.push_back(std::move(container_buffer));

        MF_INFO(
            Journal::Component::API,
            Journal::Context::BufferManagement,
            "✓ Created buffer for channel {}",
            channel);
    }

    m_audio_buffers[container] = created_buffers;

    return created_buffers;
}

std::shared_ptr<Buffers::VideoContainerBuffer>
IOManager::hook_camera_to_buffer(
    const std::shared_ptr<Kakshya::CameraContainer>& container)
{
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "hook_camera_to_buffer: null container");
        return nullptr;
    }

    auto stream_container = std::dynamic_pointer_cast<Kakshya::StreamContainer>(container);
    if (!stream_container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "hook_camera_to_buffer: container is not a StreamContainer");
        return nullptr;
    }

    auto video_buffer = m_buffer_manager->create_graphics_buffer<Buffers::VideoContainerBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        stream_container);

    if (!video_buffer) {
        MF_ERROR(Journal::Component::API, Journal::Context::BufferManagement,
            "hook_camera_to_buffer: failed to create VideoContainerBuffer");
        return nullptr;
    }

    {
        std::unique_lock lock(m_camera_mutex);
        m_camera_buffers[container] = video_buffer;
    }

    MF_INFO(Journal::Component::API, Journal::Context::BufferManagement,
        "Hooked CameraContainer to VideoContainerBuffer ({}x{})",
        video_buffer->get_width(), video_buffer->get_height());

    return video_buffer;
}

std::shared_ptr<Buffers::VideoContainerBuffer>
IOManager::get_video_buffer(
    const std::shared_ptr<Kakshya::VideoFileContainer>& container) const
{
    std::shared_lock lock(m_buffers_mutex);
    auto it = m_video_buffers.find(container);
    return it != m_video_buffers.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>
IOManager::get_audio_buffers(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container) const
{
    std::shared_lock lock(m_buffers_mutex);
    auto it = m_audio_buffers.find(container);
    return it != m_audio_buffers.end() ? it->second : std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> {};
}

std::shared_ptr<Kakshya::SoundFileContainer>
IOManager::get_extracted_audio(const std::shared_ptr<Kakshya::VideoFileContainer>& container) const
{
    std::shared_lock lock(m_buffers_mutex);
    auto it = m_extracted_audio.find(container);
    return it != m_extracted_audio.end() ? it->second : nullptr;
}

std::shared_ptr<Buffers::VideoContainerBuffer>
IOManager::get_camera_buffer(
    const std::shared_ptr<Kakshya::CameraContainer>& container) const
{
    std::shared_lock lock(m_camera_mutex);
    auto it = m_camera_buffers.find(container);
    return it != m_camera_buffers.end() ? it->second : nullptr;
}

bool IOManager::save_image(
    const std::shared_ptr<Core::VKImage>& image,
    const std::string& filepath,
    const IO::ImageWriteOptions& options)
{
    auto data = IO::download_image(image);
    if (!data) {
        return false;
    }
    return save_image(std::move(*data), filepath, options);
}

bool IOManager::save_image(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer,
    const std::string& filepath,
    const IO::ImageWriteOptions& options)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_image: null buffer");
        return false;
    }
    auto image = buffer->get_gpu_texture();
    if (!image) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_image: buffer has no GPU texture");
        return false;
    }
    return save_image(image, filepath, options);
}

bool IOManager::save_image(
    const std::shared_ptr<Buffers::TextBuffer>& buffer,
    const std::string& filepath,
    const IO::ImageWriteOptions& options)
{
    return save_image(
        std::static_pointer_cast<Buffers::TextureBuffer>(buffer),
        filepath,
        options);
}

bool IOManager::save_image(
    IO::ImageData data,
    const std::string& filepath,
    const IO::ImageWriteOptions& options)
{
    auto fut = std::async(std::launch::async,
        [data = std::move(data),
            filepath,
            options]() -> bool {
            auto writer = IO::ImageWriterRegistry::instance().create_writer(filepath);
            if (!writer) {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "save_image task: no writer registered for '{}'", filepath);
                return false;
            }
            const bool ok = writer->write(filepath, data, options);
            if (!ok) {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "save_image task: writer failed for '{}': {}",
                    filepath, writer->get_last_error());
            } else {
                MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
                    "save_image task: wrote '{}'", filepath);
            }
            return ok;
        });

    std::lock_guard lock(m_save_tasks_mutex);
    m_save_tasks.push_back(std::move(fut));

    std::erase_if(m_save_tasks, [](std::future<bool>& f) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });

    return true;
}

void IOManager::wait_for_pending_saves()
{
    std::vector<std::future<bool>> tasks;
    {
        std::lock_guard lock(m_save_tasks_mutex);
        tasks.swap(m_save_tasks);
    }
    for (auto& f : tasks) {
        f.wait();
    }
}

} // namespace MayaFlux::IO
