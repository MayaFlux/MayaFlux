#pragma once

#include "CameraReader.hpp"
#include "ImageWriter.hpp"
#include "VideoFileReader.hpp"

#include <future>

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
class VideoFileContainer;
class SoundFileContainer;
class CameraContainer;
}

namespace MayaFlux::Buffers {
class VideoContainerBuffer;
class SoundContainerBuffer;
class TextureBuffer;
class TextBuffer;
class MeshBuffer;
class BufferManager;
}

namespace MayaFlux::Registry::Service {
class IOService;
}

namespace MayaFlux::IO {

using TextureResolver = std::function<std::shared_ptr<Core::VKImage>(const std::string& path)>;

class ImageReader;
class ModelReader;

struct LoadConfig {
    FileReadOptions file_options { FileReadOptions::EXTRACT_METADATA | FileReadOptions::EXTRACT_REGIONS };
    AudioReadOptions audio_options { AudioReadOptions::DEINTERLEAVE };
    VideoReadOptions video_options {};
    uint32_t target_width { 1920 };
    uint32_t target_height { 1080 };
};

/**
 * @struct VideoLoadResult
 * @brief Result of load_video() when audio extraction is requested via
 *        VideoReadOptions::EXTRACT_AUDIO.
 *
 * @c audio is nullptr if the file has no audio track or EXTRACT_AUDIO was
 * not set in the options passed to load_video().
 */
struct VideoLoadResult {
    std::shared_ptr<Kakshya::VideoFileContainer> video;
    std::shared_ptr<Kakshya::SoundFileContainer> audio;
};

/**
 * @class IOManager
 * @brief Optional orchestration layer for IO reader lifetime and IOService dispatch.
 *
 * MayaFlux IO readers are fully self-sufficient — VideoFileReader, SoundFileReader,
 * and ImageReader can all be used standalone without this class.  IOManager exists
 * to solve problems that only arise at scale:
 *
 *  - Globally unique reader_ids across concurrent VideoFileReader instances, so
 *    IOService::request_decode routes to the correct decode thread.
 *  - A single IOService registration backed by a keyed dispatch map, rather than
 *    each reader overwriting the previous registration.
 *  - Shared ownership of VideoFileReader instances whose background decode thread
 *    must outlive the scope in which they were created.
 *  - High-level load/hook functions that collapse the full open→load→processor
 *    setup→buffer wiring sequence into single calls, mirroring what Depot does
 *    for audio and eliminating the boilerplate currently required for video.
 *
 * VideoFileReader detects the IOService registration generically via BackendRegistry
 * and skips its own registration — no IOManager-specific knowledge in the reader.
 *
 * Levels of integration (all valid):
 *  1. VideoFileReader standalone, single stream — self-registers, self-ids.
 *  2. IOManager constructed directly — multi-stream safe, no Engine required.
 *  3. Engine constructs IOManager in Init() — fully orchestrated, zero user wiring.
 */
class MAYAFLUX_API IOManager {
public:
    /**
     * @brief Construct IOManager and register the IOService into BackendRegistry.
     *
     * Must be constructed before any VideoFileReader::load_into_container() call
     * that should participate in managed dispatch.
     */
    IOManager(uint64_t sample_rate, uint32_t buffer_size, uint32_t frame_rate, const std::shared_ptr<Buffers::BufferManager>& buffer_manager);

    /**
     * @brief Unregisters IOService, releases all owned readers, clears stored buffers.
     */
    ~IOManager();

    IOManager(const IOManager&) = delete;
    IOManager& operator=(const IOManager&) = delete;
    IOManager(IOManager&&) = delete;
    IOManager& operator=(IOManager&&) = delete;

    // ─────────────────────────────────────────────────────────────────────────
    // Video — load
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Load a video file into a VideoFileContainer.
     *
     * Performs can_read check, opens the
     * file with metadata extraction, creates the container, registers the reader
     * with IOManager (assigning a globally unique reader_id), calls
     * setup_io_service(), loads into container, and configures the default
     * FrameAccessProcessor with auto_advance enabled.
     *
     * Audio is NOT extracted. Use load_video(path, options) for that.
     *
     * @param filepath Path to the video file.
     * @return Loaded VideoFileContainer, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::VideoFileContainer> load_video(const std::string& filepath);

    /**
     * @brief Load a video file into a VideoFileContainer.
     *
     * Performs can_read check, opens with
     * metadata and region extraction, registers the reader (assigning a globally
     * unique reader_id), calls setup_io_service(), loads into container, and
     * configures the default FrameAccessProcessor with auto_advance enabled.
     *
     * Passing VideoReadOptions::EXTRACT_AUDIO also extracts the embedded
     * SoundFileContainer, configures its ContiguousAccessProcessor identically
     * to load_audio_file() in Depot, and populates VideoLoadResult::audio.
     *
     * @param filepath Path to the video file.
     * @param config LoadConfig struct containing options and target dimensions.
     * @return VideoLoadResult containing video container and optional audio container.
     */
    [[nodiscard]] VideoLoadResult load_video(
        const std::string& filepath, LoadConfig config);

    // ─────────────────────────────────────────────────────────────────────────
    // Video — hook
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Wire a VideoFileContainer to the graphics buffer system.
     *
     * Creates a VideoContainerBuffer via BufferManager with GRAPHICS_BACKEND token,
     * stores it keyed by container pointer, and returns it.
     *
     * @param container Loaded VideoFileContainer to wire.
     * @return Created VideoContainerBuffer, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VideoContainerBuffer>
    hook_video_container_to_buffer(
        const std::shared_ptr<Kakshya::VideoFileContainer>& container);

    // ─────────────────────────────────────────────────────────────────────────
    // Video — retrieve
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Retrieve the VideoContainerBuffer created for a container.
     * @param container Container previously passed to hook_video_container_to_buffer().
     * @return Stored buffer, or nullptr if not found.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VideoContainerBuffer>
    get_video_buffer(
        const std::shared_ptr<Kakshya::VideoFileContainer>& container) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Audio — load
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Load an audio file into a SoundFileContainer.
     *
     * Performs can_read check, opens the file, creates and populates the
     * container, and configures the default ContiguousAccessProcessor
     * with auto_advance enabled.
     *
     * @param filepath Path to the audio file.
     * @param config LoadConfig struct containing audio read options.
     * @return Loaded SoundFileContainer, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::SoundFileContainer> load_audio(const std::string& filepath, LoadConfig config = {});

    /**
     * @brief Load an audio file into a fully resident, size-bounded DynamicSoundStream.
     *
     * Applies the engine sample rate, ROW_MAJOR layout, and the engine buffer
     * size as the CursorAccessProcessor block size. The result is ready for use
     * with StreamSliceProcessor without any further configuration.
     *
     * @param filepath   Path to the audio file.
     * @param max_frames Upper bound on frame count. 0 defaults to 5 s at the
     *                   engine sample rate inside SoundFileReader::load_bounded.
     * @param truncate   If true, silently truncate files exceeding max_frames.
     * @return Configured DynamicSoundStream, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::DynamicSoundStream> load_audio_bounded(
        const std::string& filepath,
        uint64_t max_frames = 0,
        bool truncate = false);

    // ─────────────────────────────────────────────────────────────────────────
    // Audio — hook
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Wire a SoundFileContainer to the audio buffer system.
     *
     * Creates per-channel SoundContainerBuffers with AUDIO_BACKEND token,
     * stores them keyed by container pointer, and returns them.
     *
     * @param container Loaded SoundFileContainer to wire.
     * @return Per-channel SoundContainerBuffers, or empty on failure.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>
    hook_audio_container_to_buffers(
        const std::shared_ptr<Kakshya::SoundFileContainer>& container);

    // ─────────────────────────────────────────────────────────────────────────
    // Audio — retrieve
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Retrieve the SoundContainerBuffers created for a container.
     * @param container Container previously passed to hook_audio_container_to_buffers().
     * @return Stored buffer vector, or empty if not found.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>
    get_audio_buffers(
        const std::shared_ptr<Kakshya::SoundFileContainer>& container) const;

    /**
     * @brief Retrieve the SoundFileContainer extracted from a video file.
     * @param container VideoFileContainer whose audio was extracted during load_video().
     * @return Extracted SoundFileContainer, or nullptr if not found.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::SoundFileContainer>
    get_extracted_audio(const std::shared_ptr<Kakshya::VideoFileContainer>& container) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Camera — open
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Open a camera device and create a CameraContainer.
     *
     * Constructs a CameraReader, opens the device, creates the container,
     * configures its FrameAccessProcessor (auto_advance disabled), assigns
     * a globally unique reader_id, registers the reader for IOService
     * dispatch, and wires the container's IOService callback via
     * CameraContainer::setup_io().
     *
     * After this call, the normal processing pipeline drives frame pulls:
     * CameraContainer::process_default() → IOService::request_frame(reader_id)
     * → dispatch_frame_request() → CameraReader::pull_frame_all()
     * → decode thread → pull_frame() → mutable_frame_ptr() write → READY.
     *
     * @param config Device and resolution configuration.
     * @return Opened CameraContainer, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::CameraContainer>
    open_camera(const CameraConfig& config);

    // ─────────────────────────────────────────────────────────────────────────
    // Camera — hook
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Wire a CameraContainer to the graphics buffer system.
     *
     * Creates a VideoContainerBuffer via BufferManager with GRAPHICS_BACKEND
     * token, stores it keyed by container pointer, and returns it. Identical
     * to hook_video_container_to_buffer() but accepts CameraContainer.
     *
     * @param container Opened CameraContainer from open_camera().
     * @return Created VideoContainerBuffer, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VideoContainerBuffer>
    hook_camera_to_buffer(const std::shared_ptr<Kakshya::CameraContainer>& container);

    // ─────────────────────────────────────────────────────────────────────────
    // Camera — retrieve
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Retrieve the VideoContainerBuffer created for a camera container.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VideoContainerBuffer>
    get_camera_buffer(const std::shared_ptr<Kakshya::CameraContainer>& container) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Video reader management
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Assign a globally unique reader_id and take ownership of a reader.
     *
     * Called internally by load_video(). Advanced users who open a
     * VideoFileReader manually can call this before load_into_container() to
     * participate in managed dispatch and lifetime tracking.
     *
     * Calls reader->set_reader_id(id) before returning.
     *
     * @param reader Fully-opened VideoFileReader.
     * @return Globally unique reader_id assigned to this reader.
     */
    [[nodiscard]] uint64_t register_video_reader(std::shared_ptr<VideoFileReader> reader);

    /**
     * @brief Release ownership of the reader identified by reader_id.
     *
     * When the last shared_ptr to the reader is released its decode thread
     * is joined.  Safe to call from a container state-change callback.
     * No-op with a warning if reader_id is unknown.
     *
     * @param reader_id Id returned by register_video_reader().
     */
    void release_video_reader(uint64_t reader_id);

    // ─────────────────────────────────────────────────────────────────────────
    // Image — load
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Load an image file into a TextureBuffer.
     *
     * Opens the file via ImageReader, decodes to RGBA8, and creates a
     * TextureBuffer containing the pixel data. The reader is retained
     * for the lifetime of IOManager.
     *
     * @param filepath Path to the image file (PNG, JPG, BMP, TGA, etc.).
     * @return TextureBuffer with pixel data, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer>
    load_image(const std::string& filepath);

    // ─────────────────────────────────────────────────────────────────────────
    // Mesh — load
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Load all meshes from a 3D model file into MeshBuffer instances.
     *
     * Opens the file via ModelReader, extracts all aiMesh entries as MeshData,
     * constructs one MeshBuffer per mesh, calls setup_processors() on each,
     * and returns them. setup_rendering() is left to the caller.
     *
     * If resolver is null, the default resolver is used: paths resolved
     * relative to the model file's directory via ImageReader::load_texture.
     * Unresolvable textures are logged and skipped.
     *
     * @param filepath Path to the model file (glTF, FBX, OBJ, PLY, etc.).
     * @param resolver Optional texture resolver.
     * @return One MeshBuffer per mesh in scene order, or empty on failure.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::MeshBuffer>>
    load_mesh(
        const std::string& filepath,
        TextureResolver resolver = nullptr);

    /**
     * @brief Load a 3D model file as a MeshNetwork.
     *
     * One MeshSlot per aiMesh. Per-slot diffuse textures resolved via resolver.
     * Null resolver uses the default: ImageReader::load_texture relative to the
     * model file's directory.
     *
     * @param filepath Path to the model file.
     * @param resolver Optional texture resolver.
     * @return Populated MeshNetwork, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Nodes::Network::MeshNetwork>
    load_mesh_network(
        const std::string& filepath,
        TextureResolver resolver = nullptr);

    // ─────────────────────────────────────────────────────────────────────────
    // Image — save
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Save image data to disk asynchronously.
     *
     * The source is downloaded from the GPU on the calling thread (must have
     * command queue access) and the encode + disk flush is dispatched to a
     * worker. Returns once the download completes and the encode task is
     * queued; encode/flush failures are logged but do not block the caller.
     *
     * For synchronous semantics (e.g. shutdown flush, tests) use the
     * IO::save_image / IO::save_texture_buffer free functions in
     * ImageExport.hpp.
     *
     * The extension of @p filepath selects the writer via
     * ImageWriterRegistry. Format-variant compatibility is the writer's
     * responsibility (PNG wants uint8, EXR wants float, etc.).
     *
     * @return True if the download succeeded and the encode task was queued.
     */
    bool save_image(
        const std::shared_ptr<Core::VKImage>& image,
        const std::string& filepath,
        const IO::ImageWriteOptions& options = {});

    bool save_image(
        const std::shared_ptr<Buffers::TextureBuffer>& buffer,
        const std::string& filepath,
        const IO::ImageWriteOptions& options = {});

    bool save_image(
        const std::shared_ptr<Buffers::TextBuffer>& buffer,
        const std::string& filepath,
        const IO::ImageWriteOptions& options = {});

    /**
     * @brief Save an already-downloaded ImageData to disk asynchronously.
     *
     * For callers that have an ImageData in hand (e.g. from
     * IO::download_image) and want to avoid the GPU download step. Purely
     * CPU-bound from here, so no thread restrictions on the caller.
     */
    bool save_image(
        IO::ImageData data,
        const std::string& filepath,
        const IO::ImageWriteOptions& options = {});

    /**
     * @brief Wait for all in-flight save operations to complete.
     *
     * Called automatically by the destructor. Invoke explicitly to flush
     * pending saves before a checkpoint.
     */
    void wait_for_pending_saves();

private:
    uint64_t m_sample_rate;

    uint32_t m_buffer_size;

    uint32_t m_frame_rate;

    /**
     * @brief IOService::request_decode target — shared-lock lookup + signal_decode().
     * Non-blocking. Safe from any thread.
     */
    void dispatch_decode_request(uint64_t reader_id);

    /**
     * @brief IOService::request_frame target — shared-lock lookup + pull_frame_all().
     * Non-blocking. Safe from any thread.
     */
    void dispatch_frame_request(uint64_t reader_id);

    void configure_frame_processor(
        const std::shared_ptr<Kakshya::VideoFileContainer>& container);

    void configure_audio_processor(
        const std::shared_ptr<Kakshya::SoundFileContainer>& container);

    // ── readers ──────────────────────────────────────────────────────

    std::atomic<uint64_t> m_next_reader_id { 1 };
    mutable std::shared_mutex m_readers_mutex;
    mutable std::shared_mutex m_camera_mutex;
    std::unordered_map<uint64_t, std::shared_ptr<VideoFileReader>> m_video_readers;
    std::unordered_map<uint64_t, std::shared_ptr<CameraReader>> m_camera_readers;

    std::vector<std::shared_ptr<SoundFileReader>> m_audio_readers;

    std::vector<std::shared_ptr<ImageReader>> m_image_readers;

    std::vector<std::shared_ptr<ModelReader>> m_model_readers;

    // ── Stored buffers ─────────────────────────────────────────────────────

    mutable std::shared_mutex m_buffers_mutex;
    std::mutex m_save_tasks_mutex;
    std::vector<std::future<bool>> m_save_tasks;

    std::unordered_map<
        std::shared_ptr<Kakshya::VideoFileContainer>,
        std::shared_ptr<Buffers::VideoContainerBuffer>>
        m_video_buffers;

    std::unordered_map<
        std::shared_ptr<Kakshya::CameraContainer>,
        std::shared_ptr<Buffers::VideoContainerBuffer>>
        m_camera_buffers;

    std::unordered_map<
        std::shared_ptr<Kakshya::VideoFileContainer>,
        std::shared_ptr<Kakshya::SoundFileContainer>>
        m_extracted_audio;

    std::unordered_map<
        std::shared_ptr<Kakshya::SoundFileContainer>,
        std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>>
        m_audio_buffers;

    std::shared_ptr<Buffers::BufferManager> m_buffer_manager;

    // ── IOService ──────────────────────────────────────────────────────────

    std::shared_ptr<Registry::Service::IOService> m_io_service;
};

} // namespace MayaFlux::IO
