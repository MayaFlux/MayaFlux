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
 * @brief FFmpeg-based audio file reader for MayaFlux
 *
 * SoundFileReader provides a high-level interface for reading and decoding audio files using FFmpeg.
 * It supports a wide range of audio formats, automatic sample format conversion to double precision,
 * resampling, metadata extraction, region/marker extraction, and streaming/seekable access.
 *
 * Key Features:
 * - Format detection and demuxing via libavformat
 * - Audio decoding via libavcodec
 * - Sample format conversion and resampling via libswresample (always outputs double)
 * - Metadata and region extraction from FFmpeg's parsed structures
 * - Seeking and timestamp handling via FFmpeg's APIs
 * - Automatic creation and population of Kakshya::SoundFileContainer for downstream processing
 * - Thread-safe access for reading and metadata queries
 *
 * Usage:
 *   SoundFileReader reader;
 *   if (reader.open("file.wav")) {
 *       auto metadata = reader.get_metadata();
 *       auto all_data = reader.read_all();
 *       auto container = reader.create_container();
 *       // ...
 *       reader.close();
 *   }
 *
 * All audio data is converted to double precision for internal processing.
 * The reader can output data in either interleaved or deinterleaved (planar) layout.
 */
class SoundFileReader : public FileReader {
public:
    /**
     * @brief Construct a new SoundFileReader object.
     * Initializes internal state and prepares for file operations.
     */
    SoundFileReader();

    /**
     * @brief Destroy the SoundFileReader object.
     * Cleans up FFmpeg resources and internal state.
     */
    ~SoundFileReader() override;

    /**
     * @brief Check if this reader can open the given file.
     * @param filepath Path to the file.
     * @return True if the file can be read, false otherwise.
     */
    bool can_read(const std::string& filepath) const override;

    /**
     * @brief Open an audio file for reading.
     * @param filepath Path to the file.
     * @param options File read options.
     * @return True if the file was opened successfully.
     */
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;

    /**
     * @brief Close the currently open file and release resources.
     */
    void close() override;

    /**
     * @brief Check if a file is currently open.
     * @return True if a file is open, false otherwise.
     */
    bool is_open() const override;

    /**
     * @brief Get metadata for the currently open file.
     * @return Optional FileMetadata structure.
     */
    std::optional<FileMetadata> get_metadata() const override;

    /**
     * @brief Get all regions (markers, loops, etc.) from the file.
     * @return Vector of FileRegion structures.
     */
    std::vector<FileRegion> get_regions() const override;

    /**
     * @brief Read the entire audio file into memory.
     * @return DataVariant containing audio data as std::vector<double>.
     */
    std::vector<Kakshya::DataVariant> read_all() override;

    /**
     * @brief Read a specific region from the file.
     * @param region Region to read.
     * @return DataVariant containing region data.
     */
    std::vector<Kakshya::DataVariant> read_region(const FileRegion& region) override;

    /**
     * @brief Create a SignalSourceContainer for this file.
     * @return Shared pointer to a new SignalSourceContainer.
     */
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;

    /**
     * @brief Load file data into an existing SignalSourceContainer.
     * @param container Target container.
     * @return True if loading succeeded.
     */
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    /**
     * @brief Get the current read position in the file.
     * @return Vector of dimension indices (e.g., frame index).
     */
    std::vector<u_int64_t> get_read_position() const override;

    /**
     * @brief Seek to a specific position in the file.
     * @param position Vector of dimension indices.
     * @return True if seek succeeded.
     */
    bool seek(const std::vector<u_int64_t>& position) override;

    /**
     * @brief Get supported file extensions for this reader.
     * @return Vector of supported extensions (e.g., "wav", "flac").
     */
    std::vector<std::string> get_supported_extensions() const override;

    /**
     * @brief Get the C++ type of the data returned by this reader.
     * @return Type index for std::vector<double>.
     */
    std::type_index get_data_type() const override { return typeid(std::vector<double>); }

    /**
     * @brief Get the C++ type of the container returned by this reader.
     * @return Type index for Kakshya::SoundFileContainer.
     */
    std::type_index get_container_type() const override { return typeid(Kakshya::SoundFileContainer); }

    /**
     * @brief Get the last error message encountered by the reader.
     * @return Error string.
     */
    std::string get_last_error() const override;

    /**
     * @brief Check if the reader supports streaming access.
     * @return True if streaming is supported.
     */
    bool supports_streaming() const override;

    /**
     * @brief Get the preferred chunk size for streaming reads.
     * @return Preferred chunk size in frames.
     */
    u_int64_t get_preferred_chunk_size() const override;

    /**
     * @brief Get the number of dimensions in the audio data (typically 2: time, channel).
     * @return Number of dimensions.
     */
    size_t get_num_dimensions() const override;

    /**
     * @brief Get the size of each dimension (e.g., frames, channels).
     * @return Vector of dimension sizes.
     */
    std::vector<u_int64_t> get_dimension_sizes() const override;

    /**
     * @brief Read a specific number of frames from the file.
     * @param num_frames Number of frames to read.
     * @param offset Frame offset from beginning.
     * @return DataVariant containing std::vector<double>.
     */
    std::vector<Kakshya::DataVariant> read_frames(u_int64_t num_frames, u_int64_t offset = 0);

    /**
     * @brief Set audio-specific read options.
     * @param options Audio read options (e.g., DEINTERLEAVE).
     */
    void set_audio_options(AudioReadOptions options) { m_audio_options = options; }

    /**
     * @brief Set the target sample rate for resampling.
     * @param sample_rate Target sample rate (0 = no resampling).
     */
    void set_target_sample_rate(u_int32_t sample_rate) { m_target_sample_rate = sample_rate; }

    /**
     * @brief Set the target bit depth (ignored, always outputs double).
     * @param bit_depth Target bit depth.
     * @deprecated Always outputs double precision.
     */
    void set_target_bit_depth(u_int32_t bit_depth) { m_target_bit_depth = bit_depth; }

    /**
     * @brief Initialize FFmpeg libraries (thread-safe, called automatically).
     */
    static void initialize_ffmpeg();

private:
    // FFmpeg contexts - let FFmpeg manage these

    /**
     * @brief FFmpeg format context (demuxer).
     */
    AVFormatContext* m_format_context = nullptr;

    /**
     * @brief FFmpeg codec context (decoder).
     */
    AVCodecContext* m_codec_context = nullptr;

    /**
     * @brief FFmpeg resampler context.
     */
    SwrContext* m_swr_context = nullptr;

    /**
     * @brief Index of the audio stream in the file.
     */
    int m_audio_stream_index = -1;

    /**
     * @brief True if a file is currently open.
     */
    std::atomic<bool> m_is_open { false };

    /**
     * @brief Path to the currently open file.
     */
    std::string m_filepath;

    /**
     * @brief File read options used for this session.
     */
    FileReadOptions m_options;

    /**
     * @brief Audio-specific read options.
     */
    AudioReadOptions m_audio_options = AudioReadOptions::NONE;

    /**
     * @brief Last error message encountered.
     */
    mutable std::string m_last_error;

    /**
     * @brief Cached file metadata.
     */
    mutable std::optional<FileMetadata> m_cached_metadata;

    /**
     * @brief Cached file regions (markers, loops, etc.).
     */
    mutable std::vector<FileRegion> m_cached_regions;

    /**
     * @brief Current frame position for reading.
     */
    std::atomic<u_int64_t> m_current_frame_position { 0 };

    /**
     * @brief Total number of frames in the file.
     */
    u_int64_t m_total_frames = 0;

    /**
     * @brief Target sample rate for resampling (0 = use source rate).
     */
    u_int32_t m_target_sample_rate = 0;

    /**
     * @brief Target bit depth (ignored, always outputs double).
     */
    u_int32_t m_target_bit_depth = 0;

    /**
     * @brief Mutex for thread-safe reading.
     */
    mutable std::mutex m_read_mutex;

    /**
     * @brief Mutex for thread-safe metadata access.
     */
    mutable std::mutex m_metadata_mutex;

    // Simplified internal methods

    /**
     * @brief Set up the FFmpeg resampler if needed.
     * @return True if setup succeeded.
     */
    bool setup_resampler();

    /**
     * @brief Clean up FFmpeg resources.
     */
    void cleanup_ffmpeg();

    /**
     * @brief Extract metadata from the file.
     */
    void extract_metadata();

    /**
     * @brief Extract region information from the file.
     */
    void extract_regions();

    /**
     * @brief Parse ID3 tags (placeholder, handled by FFmpeg).
     */
    void parse_id3_tags();

    /**
     * @brief Parse WAV chunks (placeholder, handled by FFmpeg).
     */
    void parse_wav_chunks();

    /**
     * @brief Parse FLAC comments (placeholder, handled by FFmpeg).
     */
    void parse_flac_comments();

    /**
     * @brief Decode a specific number of frames from the file.
     * @param num_frames Number of frames to decode.
     * @param offset Frame offset from beginning.
     * @return DataVariant containing decoded data.
     */
    std::vector<Kakshya::DataVariant> decode_frames(u_int64_t num_frames, u_int64_t offset);

    /**
     * @brief Convert interleaved audio data to deinterleaved (planar) format.
     * @param interleaved Input interleaved data.
     * @param channels Number of channels.
     * @return Deinterleaved data as std::vector<double>.
     */
    std::vector<std::vector<double>> deinterleave_data(const std::vector<double>& interleaved, u_int32_t channels);

    /**
     * @brief Set the last error message.
     * @param error Error string.
     */
    void set_error(const std::string& error) const;

    /**
     * @brief Clear the last error message.
     */
    void clear_error() const;

    /**
     * @brief True if FFmpeg has been initialized.
     */
    static std::atomic<bool> s_ffmpeg_initialized;

    /**
     * @brief Mutex for FFmpeg initialization.
     */
    static std::mutex s_ffmpeg_init_mutex;
};

} // namespace MayaFlux::IO
