#pragma once

#include "FileReader.hpp"

#include "MayaFlux/IO/AudioStreamContext.hpp"
#include "MayaFlux/IO/FFmpegDemuxContext.hpp"
#include "MayaFlux/IO/VideoStreamContext.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoFileContainer.hpp"

namespace MayaFlux::IO {

/**
 * @enum VideoReadOptions
 * @brief Video-specific reading options.
 */
enum class VideoReadOptions : uint32_t {
    NONE = 0,
    EXTRACT_AUDIO = 1 << 0, ///< Also decode the best audio stream into a SoundFileContainer.
    ALL = 0xFFFFFFFF
};

inline VideoReadOptions operator|(VideoReadOptions a, VideoReadOptions b)
{
    return static_cast<VideoReadOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline VideoReadOptions operator&(VideoReadOptions a, VideoReadOptions b)
{
    return static_cast<VideoReadOptions>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @class VideoFileReader
 * @brief FFmpeg-based video file reader for MayaFlux.
 *
 * Load-purpose reader that decodes an entire video file into memory.
 * Produces a VideoFileContainer (RGBA uint8_t pixel data) and optionally
 * a SoundFileContainer if the container holds an audio stream and
 * EXTRACT_AUDIO is set.
 *
 * Mirrors SoundFileReader in structure: open → read_all / read_frames →
 * load_into_container → close. The reader is transient — open, decode,
 * close. Streaming decode is a future processor concern, not a reader
 * concern.
 *
 * The demuxer is shared between audio and video stream contexts so
 * both can be decoded from a single open() call. Packets are filtered
 * by stream_index in the decode loop.
 *
 * All decoded video data is converted to RGBA via SwsContext (matching
 * Vulkan VK_FORMAT_R8G8B8A8_UNORM and TextureBuffer pipeline).
 * Audio data is converted to double via SwrContext (matching existing
 * SoundFileContainer convention).
 */
class VideoFileReader : public FileReader {
public:
    VideoFileReader();
    ~VideoFileReader() override;

    bool can_read(const std::string& filepath) const override;

    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;

    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;
    [[nodiscard]] std::vector<FileRegion> get_regions() const override;

    std::vector<Kakshya::DataVariant> read_all() override;
    std::vector<Kakshya::DataVariant> read_region(const FileRegion& region) override;

    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    [[nodiscard]] std::vector<uint64_t> get_read_position() const override;
    bool seek(const std::vector<uint64_t>& position) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;

    [[nodiscard]] std::type_index get_data_type() const override { return typeid(std::vector<uint8_t>); }
    [[nodiscard]] std::type_index get_container_type() const override { return typeid(Kakshya::VideoFileContainer); }

    [[nodiscard]] std::string get_last_error() const override;
    [[nodiscard]] bool supports_streaming() const override { return false; }
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 1; }
    [[nodiscard]] size_t get_num_dimensions() const override;
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override;

    // =========================================================================
    // Video-specific API
    // =========================================================================

    /**
     * @brief Read a range of video frames into RGBA uint8_t data.
     * @param num_frames Number of frames to decode.
     * @param offset     Frame offset from beginning.
     * @return Single-element vector containing std::vector<uint8_t>.
     */
    std::vector<Kakshya::DataVariant> read_frames(uint64_t num_frames, uint64_t offset = 0);

    /**
     * @brief Set video-specific read options.
     */
    void set_video_options(VideoReadOptions options) { m_video_options = options; }

    /**
     * @brief Set target output dimensions (0 = keep source).
     */
    void set_target_dimensions(uint32_t width, uint32_t height)
    {
        m_target_width = width;
        m_target_height = height;
    }

    /**
     * @brief Set target output pixel format (negative = AV_PIX_FMT_RGBA).
     */
    void set_target_pixel_format(int format) { m_target_pixel_format = format; }

    /**
     * @brief Set audio-specific options for the embedded audio stream.
     */
    // void set_audio_options(AudioReadOptions options) { m_audio_options = options; }

    /**
     * @brief Set target sample rate for embedded audio resampling.
     */
    void set_target_sample_rate(uint32_t sample_rate) { m_target_sample_rate = sample_rate; }

    /**
     * @brief After load_into_container(), retrieve the audio container if EXTRACT_AUDIO was set.
     * @return Audio container or nullptr if no audio stream exists or was not extracted.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::SoundFileContainer> get_audio_container() const
    {
        return m_audio_container;
    }

private:
    // =========================================================================
    // Contexts
    // =========================================================================

    std::shared_ptr<FFmpegDemuxContext> m_demux;
    std::shared_ptr<VideoStreamContext> m_video;
    std::shared_ptr<AudioStreamContext> m_audio;

    mutable std::shared_mutex m_context_mutex;

    // =========================================================================
    // Reader state
    // =========================================================================

    std::string m_filepath;
    FileReadOptions m_options = FileReadOptions::ALL;
    VideoReadOptions m_video_options = VideoReadOptions::NONE;
    // AudioReadOptions m_audio_options = AudioReadOptions::NONE;

    mutable std::string m_last_error;
    mutable std::mutex m_error_mutex;

    mutable std::optional<FileMetadata> m_cached_metadata;
    mutable std::vector<FileRegion> m_cached_regions;
    mutable std::mutex m_metadata_mutex;

    std::atomic<uint64_t> m_current_frame_position { 0 };

    uint32_t m_target_width = 0;
    uint32_t m_target_height = 0;
    int m_target_pixel_format = -1;
    uint32_t m_target_sample_rate = 0;

    std::shared_ptr<Kakshya::SoundFileContainer> m_audio_container;

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * @brief Decode video frames into RGBA uint8_t data.
     *
     * Reads packets from the demuxer, filters by video stream_index,
     * decodes via avcodec, scales via sws_scale into the output format.
     *
     * @param demux      Demux context (packet source).
     * @param video      Video stream context (codec + scaler).
     * @param num_frames Number of frames to decode.
     * @return Single-element vector containing std::vector<uint8_t>.
     */
    std::vector<Kakshya::DataVariant> decode_video_frames(
        const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video,
        uint64_t num_frames);

    /**
     * @brief Decode all audio frames from the file.
     *
     * Seeks to beginning, reads all packets filtering by audio stream_index,
     * decodes and resamples to double. Used when EXTRACT_AUDIO is set.
     *
     * @param demux Demux context (packet source).
     * @param audio Audio stream context (codec + resampler).
     * @return DataVariant vector (interleaved doubles).
     */
    std::vector<Kakshya::DataVariant> decode_audio_frames(
        const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<AudioStreamContext>& audio);

    bool seek_internal(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video,
        uint64_t frame_position);

    void build_metadata(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video,
        const std::shared_ptr<AudioStreamContext>& audio) const;

    void build_regions(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video,
        const std::shared_ptr<AudioStreamContext>& audio) const;

    void set_error(const std::string& error) const;
    void clear_error() const;
};

} // namespace MayaFlux::IO
