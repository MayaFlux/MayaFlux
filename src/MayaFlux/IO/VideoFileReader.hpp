#pragma once

#include "SoundFileReader.hpp"

#include "MayaFlux/IO/AudioStreamContext.hpp"
#include "MayaFlux/IO/FFmpegDemuxContext.hpp"
#include "MayaFlux/IO/VideoStreamContext.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoFileContainer.hpp"

#include <condition_variable>

namespace MayaFlux::IO {

/// @brief Video-specific reading options.
enum class VideoReadOptions : uint32_t {
    NONE = 0,
    EXTRACT_AUDIO = 1 << 0,
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
 * @brief Streaming FFmpeg-based video file reader with background decode.
 *
 * The reader is NOT transient — it stays alive alongside its container and
 * owns the FFmpeg decode contexts plus a background decode thread. The
 * lifecycle is:
 *
 *   open() → create_container() → load_into_container() → [playback] → close()
 *
 * load_into_container() sets up the container's ring buffer, synchronously
 * decodes the first batch (so frame 0 is available immediately), then starts
 * a background thread that batch-decodes ahead of the consumer read head.
 *
 * When the consumer (FrameAccessProcessor → VideoStreamReader) advances past
 * ring_capacity - refill_threshold decoded frames, the decode thread
 * automatically refills with the next decode_batch_size frames.
 *
 * Seek invalidates the ring, repositions the demuxer, synchronously decodes
 * the first batch at the new position, then restarts background decoding.
 *
 * Audio extraction (EXTRACT_AUDIO) is delegated to SoundFileReader as before.
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
    [[nodiscard]] bool supports_streaming() const override { return true; }
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return m_decode_batch_size; }
    [[nodiscard]] size_t get_num_dimensions() const override;
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override;

    // =========================================================================
    // Streaming configuration
    // =========================================================================

    /**
     * @brief Set the number of decoded frame slots in the ring buffer.
     *        Default: 32. Must be a power of 2 (enforced by FixedStorage).
     *        Must be called before load_into_container().
     */
    void set_ring_capacity(uint32_t n)
    {
        n = std::max(4U, n);
        uint32_t p = 1;
        while (p < n)
            p <<= 1;
        m_ring_capacity = p;
    }

    /**
     * @brief Set the number of frames decoded per batch by the background thread.
     *        Default: 8.
     */
    void set_decode_batch_size(uint32_t n) { m_decode_batch_size = std::max(1U, n); }

    /**
     * @brief Start refilling when fewer than this many frames remain ahead of
     *        the consumer read head. Default: ring_capacity / 4.
     *        A value of 0 means auto-compute as ring_capacity / 4.
     */
    void set_refill_threshold(uint32_t n) { m_refill_threshold = n; }

    // =========================================================================
    // Video-specific configuration
    // =========================================================================

    void set_video_options(VideoReadOptions options) { m_video_options = options; }
    void set_target_dimensions(uint32_t width, uint32_t height)
    {
        m_target_width = width;
        m_target_height = height;
    }
    void set_target_sample_rate(uint32_t sample_rate) { m_target_sample_rate = sample_rate; }
    void set_audio_options(AudioReadOptions options) { m_audio_options = options; }

    /**
     * @brief After load_into_container(), retrieve the audio container if
     *        EXTRACT_AUDIO was set.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::SoundFileContainer> get_audio_container() const
    {
        return m_audio_container;
    }

private:
    // =========================================================================
    // FFmpeg state
    // =========================================================================

    std::shared_ptr<FFmpegDemuxContext> m_demux;
    std::shared_ptr<VideoStreamContext> m_video;
    std::shared_ptr<AudioStreamContext> m_audio;
    mutable std::shared_mutex m_context_mutex;

    std::string m_filepath;
    FileReadOptions m_options = FileReadOptions::ALL;
    VideoReadOptions m_video_options = VideoReadOptions::NONE;
    AudioReadOptions m_audio_options = AudioReadOptions::NONE;

    uint32_t m_target_width { 0 };
    uint32_t m_target_height { 0 };
    uint32_t m_target_sample_rate { 0 };

    mutable std::string m_last_error;
    mutable std::mutex m_error_mutex;
    mutable std::optional<FileMetadata> m_cached_metadata;
    mutable std::vector<FileRegion> m_cached_regions;
    mutable std::mutex m_metadata_mutex;

    std::shared_ptr<Kakshya::SoundFileContainer> m_audio_container;

    // =========================================================================
    // Streaming decode state
    // =========================================================================

    uint32_t m_ring_capacity { 32 };
    uint32_t m_decode_batch_size { 8 };
    uint32_t m_refill_threshold { 0 };

    std::weak_ptr<Kakshya::VideoFileContainer> m_container_ref;

    std::thread m_decode_thread;
    std::mutex m_decode_mutex;
    std::condition_variable m_decode_cv;
    std::atomic<bool> m_decode_stop { false };
    std::atomic<bool> m_decode_active { false };
    std::atomic<uint64_t> m_decode_head { 0 };

    /// @brief One-frame sws scratch buffer (padded linesize, reused by decode thread).
    std::vector<uint8_t> m_sws_buf;

    // =========================================================================
    // Decode thread
    // =========================================================================

    void start_decode_thread();
    void stop_decode_thread();
    void decode_thread_func();

    /**
     * @brief Decode up to batch_size frames starting at m_decode_head.
     *        Pumps packets in a loop, draining multiple frames per packet
     *        batch. Returns the number of frames actually decoded.
     */
    uint64_t decode_batch(Kakshya::VideoFileContainer& vc, uint64_t batch_size);

    // =========================================================================
    // Shared helpers
    // =========================================================================

    bool seek_internal(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video,
        uint64_t frame_position);

    void build_metadata(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video) const;

    void build_regions(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<VideoStreamContext>& video) const;

    void set_error(const std::string& msg) const;
    void clear_error() const;
};

} // namespace MayaFlux::IO
