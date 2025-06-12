#pragma once

#include "FileReader.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

// Forward declarations for FFmpeg types
extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;
}

namespace MayaFlux::IO {

/**
 * @enum AudioReadOptions
 * @brief Audio-specific reading options
 */
enum class AudioReadOptions : u_int32_t {
    NONE = 0,
    NORMALIZE = 1 << 0, // Not implemented - would use FFmpeg's volume filter
    CONVERT_TO_MONO = 1 << 1, // Not implemented - would use FFmpeg's channel mixer
    DEINTERLEAVE = 1 << 2, // Convert from interleaved to planar layout
    ALL = 0xFFFFFFFF
};

inline AudioReadOptions operator|(AudioReadOptions a, AudioReadOptions b)
{
    return static_cast<AudioReadOptions>(static_cast<u_int32_t>(a) | static_cast<u_int32_t>(b));
}

inline AudioReadOptions operator&(AudioReadOptions a, AudioReadOptions b)
{
    return static_cast<AudioReadOptions>(static_cast<u_int32_t>(a) & static_cast<u_int32_t>(b));
}

/**
 * @class SoundFileReader
 * @brief FFmpeg-based audio file reader
 *
 * This class maximally leverages FFmpeg for all audio file operations:
 * - Format detection and demuxing via libavformat
 * - Audio decoding via libavcodec
 * - Sample format conversion and resampling via libswresample
 * - Metadata extraction from FFmpeg's parsed structures
 * - Seeking and timestamp handling via FFmpeg's APIs
 *
 * All audio data is converted to double precision for internal processing.
 * The reader automatically creates SoundFileContainers ready for immediate use.
 */
class SoundFileReader : public FileReader {
public:
    SoundFileReader();
    ~SoundFileReader() override;

    // FileReader interface implementation
    bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    bool is_open() const override;

    std::optional<FileMetadata> get_metadata() const override;
    std::vector<FileRegion> get_regions() const override;

    Kakshya::DataVariant read_all() override;
    Kakshya::DataVariant read_region(const FileRegion& region) override;

    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    std::vector<u_int64_t> get_read_position() const override;
    bool seek(const std::vector<u_int64_t>& position) override;

    std::vector<std::string> get_supported_extensions() const override;
    std::type_index get_data_type() const override { return typeid(std::vector<double>); }
    std::type_index get_container_type() const override { return typeid(Kakshya::SoundFileContainer); }
    std::string get_last_error() const override;

    bool supports_streaming() const override;
    u_int64_t get_preferred_chunk_size() const override;
    size_t get_num_dimensions() const override;
    std::vector<u_int64_t> get_dimension_sizes() const override;

    /**
     * @brief Read a specific number of frames
     * @param num_frames Number of frames to read
     * @param offset Frame offset from beginning
     * @return DataVariant containing std::vector<double>
     */
    Kakshya::DataVariant read_frames(u_int64_t num_frames, u_int64_t offset = 0);

    /**
     * @brief Set audio-specific read options
     * @param options Audio read options (currently only DEINTERLEAVE is implemented)
     */
    void set_audio_options(AudioReadOptions options) { m_audio_options = options; }

    /**
     * @brief Set target sample rate for resampling
     * @param sample_rate Target sample rate (0 = no resampling)
     */
    void set_target_sample_rate(u_int32_t sample_rate) { m_target_sample_rate = sample_rate; }

    /**
     * @brief Set target bit depth (ignored - always outputs doubles)
     * @param bit_depth Target bit depth
     * @deprecated Always outputs double precision
     */
    void set_target_bit_depth(u_int32_t bit_depth) { m_target_bit_depth = bit_depth; }

    /**
     * @brief Initialize FFmpeg (called automatically)
     */
    static void initialize_ffmpeg();

private:
    // FFmpeg contexts - let FFmpeg manage these
    AVFormatContext* m_format_context = nullptr;
    AVCodecContext* m_codec_context = nullptr;
    SwrContext* m_swr_context = nullptr;

    // Stream info
    int m_audio_stream_index = -1;
    std::atomic<bool> m_is_open { false };

    // File info
    std::string m_filepath;
    FileReadOptions m_options;
    AudioReadOptions m_audio_options = AudioReadOptions::NONE;
    mutable std::string m_last_error;

    // Cached data
    mutable std::optional<FileMetadata> m_cached_metadata;
    mutable std::vector<FileRegion> m_cached_regions;

    // Read state
    std::atomic<u_int64_t> m_current_frame_position { 0 };
    u_int64_t m_total_frames = 0;

    // Target format
    u_int32_t m_target_sample_rate = 0; // 0 = use source rate
    u_int32_t m_target_bit_depth = 0; // Ignored - always output doubles

    // Thread safety
    mutable std::mutex m_read_mutex;
    mutable std::mutex m_metadata_mutex;

    // Simplified internal methods
    bool setup_resampler();
    void cleanup_ffmpeg();

    void extract_metadata();
    void extract_regions();

    // Placeholder methods - FFmpeg handles the actual parsing
    void parse_id3_tags();
    void parse_wav_chunks();
    void parse_flac_comments();

    Kakshya::DataVariant decode_frames(u_int64_t num_frames, u_int64_t offset);
    std::vector<double> deinterleave_data(const std::vector<double>& interleaved, u_int32_t channels);

    void set_error(const std::string& error) const;
    void clear_error() const;

    static std::atomic<bool> s_ffmpeg_initialized;
    static std::mutex s_ffmpeg_init_mutex;
};

} // namespace MayaFlux::IO
