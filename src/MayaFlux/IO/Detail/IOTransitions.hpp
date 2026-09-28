#pragma once

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Kakshya {
class VideoFileContainer;
class SoundFileContainer;
class CameraContainer;
class StreamContainer;
struct VolumeData;
struct MeshData;
}

namespace MayaFlux::Buffers {
class BufferManager;
class VideoContainerBuffer;
class SoundContainerBuffer;
class VolumeGridBuffer;
class ComputeMeshBuffer;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
}

namespace MayaFlux::IO {
struct ModelWriteOptions;
struct ImageWriteOptions;
struct VolumeWriteOptions;
struct ImageData;
struct SpatialCaptureSource;
class VolumeCapture;
class SpatialCapture;
class SpatialCache;
}

namespace MayaFlux::IO::Detail {

using TextureResolver = std::function<std::shared_ptr<Core::VKImage>(const std::string& path)>;

void configure_video_container(
    const std::shared_ptr<Kakshya::VideoFileContainer>& container, uint32_t frame_rate);

void configure_audio_container(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container, uint32_t buffer_size);

void configure_camera_container(
    const std::shared_ptr<Kakshya::CameraContainer>& container, uint64_t reader_id);

TextureResolver make_default_resolver(const std::string& filepath);

std::shared_ptr<Buffers::VideoContainerBuffer> create_video_container_buffer(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::StreamContainer>& container);

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> create_audio_container_buffers(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const std::shared_ptr<Kakshya::SoundFileContainer>& container);

std::shared_ptr<Buffers::VolumeGridBuffer> create_volume_grid_buffer(
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
    const Kakshya::VolumeData& volume_data);

bool execute_mesh_write(
    const std::string& filepath,
    const std::vector<Kakshya::MeshData>& meshes,
    const ModelWriteOptions& options);

bool execute_image_write(
    const std::string& filepath,
    const ImageData& data,
    const ImageWriteOptions& options);

bool execute_volume_write(
    const std::string& filepath,
    const Kakshya::VolumeData& data,
    const VolumeWriteOptions& options);

bool execute_mesh_snapshot(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options);

bool execute_spatial_snapshot(
    const std::string& path_pattern,
    const std::vector<SpatialCaptureSource>& sources);

std::unique_ptr<VolumeCapture> create_volume_capture(
    Vruta::TaskScheduler& scheduler,
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::string& path_pattern,
    const std::vector<std::string>& field_names,
    const VolumeWriteOptions& options,
    std::function<bool(Kakshya::VolumeData&&, const std::string&, const VolumeWriteOptions&)> save_fn,
    uint32_t max_frames,
    uint64_t frame_interval);

std::unique_ptr<SpatialCapture> create_spatial_capture(
    Vruta::TaskScheduler& scheduler,
    const std::shared_ptr<SpatialCache>& cache,
    std::vector<SpatialCaptureSource> sources,
    uint32_t max_frames,
    uint64_t frame_interval);

} // namespace MayaFlux::IO::Detail
