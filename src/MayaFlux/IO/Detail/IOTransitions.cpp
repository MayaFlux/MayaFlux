#include "IOTransitions.hpp"

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/IO/Image/ImageReader.hpp"

#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Processors/FrameAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/CameraContainer.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoFileContainer.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"

#include "MayaFlux/IO/Model/ModelExport.hpp"
#include "MayaFlux/IO/Model/ModelWriter.hpp"

#include "MayaFlux/IO/Image/ImageWriter.hpp"

#include "MayaFlux/IO/Volume/VolumeTransfer.hpp"
#include "MayaFlux/IO/Volume/VolumeWriter.hpp"

#include "MayaFlux/IO/Spatial/SpatialTransfer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO::Detail {

void configure_video_container(
    const std::shared_ptr<Kakshya::VideoFileContainer>& container, uint32_t m_frame_rate)
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

void configure_audio_container(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container, uint32_t buffer_size)
{
    container->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    const std::vector<uint64_t> output_shape = {
        buffer_size,
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

void configure_camera_container(
    const std::shared_ptr<Kakshya::CameraContainer>& container, uint64_t reader_id)
{
    container->setup_io(reader_id);
    container->mark_ready_for_processing(true);
}

TextureResolver make_default_resolver(const std::string& filepath)
{
    const auto base_dir = std::filesystem::path(filepath).parent_path();
    return [base_dir](const std::string& raw) -> std::shared_ptr<Core::VKImage> {
        auto tex_path = FileReader::resolve_path((base_dir / raw).generic_string());
        return IO::ImageReader::load_texture(tex_path);
    };
}

std::shared_ptr<Buffers::VideoContainerBuffer> create_video_container_buffer(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::StreamContainer>& container)
{
    return buffer_manager->create_graphics_buffer<Buffers::VideoContainerBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        container);
}

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> create_audio_container_buffers(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::SoundFileContainer>& container)
{
    std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> created_buffers;

    const uint32_t num_channels = container->get_num_channels();
    created_buffers.reserve(num_channels);

    for (uint32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = buffer_manager->create_audio_buffer<Buffers::SoundContainerBuffer>(
            Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();
        created_buffers.push_back(std::move(container_buffer));
    }

    return created_buffers;
}

std::shared_ptr<Buffers::VolumeGridBuffer> create_volume_grid_buffer(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const Kakshya::VolumeData& volume_data)
{
    std::vector<Buffers::VolumeGridBuffer::FieldDecl> decls;
    decls.reserve(volume_data.fields.size());
    for (const auto& field : volume_data.fields) {
        decls.push_back({
            .name = field.name,
            .stride_bytes = field.is_vector() ? sizeof(glm::vec4) : sizeof(float),
            .double_buffered = true,
            .semantics = field.semantics,
        });
    }

    auto buffer = buffer_manager->create_graphics_buffer<Buffers::VolumeGridBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        volume_data.lattice, decls);

    if (!buffer)
        return nullptr;

    if (!IO::upload_volume(volume_data, buffer))
        return nullptr;

    return buffer;
}

bool execute_mesh_write(
    const std::string& filepath,
    const std::vector<Kakshya::MeshData>& meshes,
    const ModelWriteOptions& options)
{
    auto writer = ModelWriterRegistry::instance().create_writer(filepath);
    if (!writer) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "save_mesh: no writer registered for '{}'", filepath);
        return false;
    }

    const bool ok = writer->write(filepath, meshes, options);
    if (!ok) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "save_mesh: writer failed for '{}': {}", filepath, writer->get_last_error());
    } else {
        MF_INFO(Journal::Component::API, Journal::Context::FileIO,
            "save_mesh: wrote '{}'", filepath);
    }
    return ok;
}

bool execute_image_write(
    const std::string& filepath,
    const ImageData& data,
    const ImageWriteOptions& options)
{
    auto writer = ImageWriterRegistry::instance().create_writer(filepath);
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
}

bool execute_volume_write(
    const std::string& filepath,
    const Kakshya::VolumeData& data,
    const VolumeWriteOptions& options)
{
    auto writer = VolumeWriterRegistry::instance().create_writer(filepath);
    if (!writer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_volume task: no writer registered for '{}'", filepath);
        return false;
    }
    const bool ok = writer->write(filepath, data, options);
    if (!ok) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_volume task: writer failed for '{}': {}",
            filepath, writer->get_last_error());
    } else {
        MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
            "save_volume task: wrote '{}'", filepath);
    }
    return ok;
}

bool execute_mesh_snapshot(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return IO::save_mesh_snapshot(buffer, path_pattern, options);
}

bool execute_spatial_snapshot(
    const std::string& path_pattern,
    const std::vector<SpatialCaptureSource>& sources)
{
    return IO::save_spatial_snapshot(path_pattern, sources);
}

std::unique_ptr<VolumeCapture> create_volume_capture(
    Vruta::TaskScheduler& scheduler,
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::string& path_pattern,
    const std::vector<std::string>& field_names,
    const VolumeWriteOptions& options,
    std::function<bool(Kakshya::VolumeData&&, const std::string&, const VolumeWriteOptions&)> save_fn,
    uint32_t max_frames,
    uint64_t frame_interval)
{
    auto capture = std::make_unique<VolumeCapture>(
        scheduler, volume, path_pattern, field_names, options, std::move(save_fn));

    capture->start(max_frames, frame_interval);
    return capture;
}

std::unique_ptr<SpatialCapture> create_spatial_capture(
    Vruta::TaskScheduler& scheduler,
    const std::shared_ptr<SpatialCache>& cache,
    std::vector<SpatialCaptureSource> sources,
    uint32_t max_frames,
    uint64_t frame_interval)
{
    auto capture = std::make_unique<SpatialCapture>(scheduler, cache, std::move(sources));
    capture->start(max_frames, frame_interval);
    return capture;
}

} // namespace MayaFlux::IO::Detail
