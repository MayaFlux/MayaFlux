#pragma once

#include "FileReader.hpp"

#include "MayaFlux/IO/AudioStreamContext.hpp"
#include "MayaFlux/IO/FFmpegDemuxContext.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

namespace MayaFlux::IO {

/**
 * @enum AudioReadOptions
 * @brief Audio-specific reading options
 */
enum class AudioReadOptions : uint32_t {
    NONE = 0,
    NORMALIZE = 1 << 0, ///< Not implemented — placeholder for future volume filter.
    CONVERT_TO_MONO = 1 << 2, ///< Not implemented — placeholder for channel mixer.
    DEINTERLEAVE = 1 << 3, ///< Output planar (per-channel) doubles instead of interleaved.
    ALL = 0xFFFFFFFF
};

inline AudioReadOptions operator|(AudioReadOptions a, AudioReadOptions b)
{
    return static_cast<AudioReadOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline AudioReadOptions operator&(AudioReadOptions a, AudioReadOptions b)
{
    return static_cast<AudioReadOptions>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
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
    std::vector<uint64_t> get_read_position() const override;

    /**
     * @brief Seek to a specific position in the file.
     * @param position Vector of dimension indices.
     * @return True if seek succeeded.
     */
    bool seek(const std::vector<uint64_t>& position) override;

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
    bool supports_streaming() const override { return true; }

    /**
     * @brief Get the preferred chunk size for streaming reads.
     * @return Preferred chunk size in frames, typically 4096.
     */
    uint64_t get_preferred_chunk_size() const override { return 4096; }

    /**
     * @brief Get the number of dimensions in the audio data (typically 2: time, channel).
     * @return Number of dimensions.
     */
    size_t get_num_dimensions() const override;

    /**
     * @brief Get the size of each dimension (e.g., frames, channels).
     * @return Vector of dimension sizes.
     */
    std::vector<uint64_t> get_dimension_sizes() const override;

    /**
     * @brief Read a specific number of frames from the file.
     * @param num_frames Number of frames to read.
     * @param offset Frame offset from beginning.
     * @return DataVariant containing std::vector<double>.
     */
    std::vector<Kakshya::DataVariant> read_frames(uint64_t num_frames, uint64_t offset = 0);

    /**
     * @brief Set audio-specific read options.
     * @param options Audio read options (e.g., DEINTERLEAVE).
     */
    void set_audio_options(AudioReadOptions options) { m_audio_options = options; }

    /**
     * @brief Set the target sample rate for resampling.
     * @param sample_rate Target sample rate (0 = no resampling).
     */
    void set_target_sample_rate(uint32_t sample_rate) { m_target_sample_rate = sample_rate; }

private:
    // =========================================================================
    // Contexts (composition — Option B)
    // =========================================================================

    std::shared_ptr<FFmpegDemuxContext> m_demux; ///< Container / format state.
    std::shared_ptr<AudioStreamContext> m_audio; ///< Codec + resampler state.

    mutable std::shared_mutex m_context_mutex; ///< Guards both context pointers.

    // =========================================================================
    // Reader state
    // =========================================================================

    /**
     * @brief Path to the currently open file.
     */
    std::string m_filepath;

    /**
     * @brief File read options used for this session.
     */
    FileReadOptions m_options = FileReadOptions::ALL;

    /**
     * @brief Audio-specific read options.
     */
    AudioReadOptions m_audio_options = AudioReadOptions::NONE;

    /**
     * @brief Last error message encountered.
     */
    mutable std::string m_last_error;

    /**
     * @brief Mutex for thread-safe error message access.
     */
    mutable std::mutex m_error_mutex;

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
    std::atomic<uint64_t> m_current_frame_position { 0 };

    /**
     * @brief Target sample rate for resampling (0 = use source rate).
     */
    uint32_t m_target_sample_rate = 0;

    /**
     * @brief Mutex for thread-safe metadata access.
     */
    mutable std::mutex m_metadata_mutex;

    // =========================================================================
    // Internal helpers
    // =========================================================================
    /**
     * @brief Decode num_frames PCM frames starting at offset.
     * @param ctx FFmpeg context.
     * @param num_frames Number of frames to decode.
     * @param offset Frame offset from beginning.
     * @return DataVariant containing decoded data.
     *
     * Caller must hold at least a shared lock on m_context_mutex.
     */
    std::vector<Kakshya::DataVariant> decode_frames(
        const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<AudioStreamContext>& audio,
        uint64_t num_frames,
        uint64_t offset);

    /**
     * @brief Seek the demuxer and flush the codec to the given frame position.
     * @param ctx FFmpeg context.
     * @param frame_position Target frame position.
     * @return True if seek succeeded.
     *
     * Caller must hold an exclusive lock on m_context_mutex.
     */
    bool seek_internal(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<AudioStreamContext>& audio,
        uint64_t frame_position);

    /**
     * @brief Build and cache FileMetadata from both contexts.
     */
    void build_metadata(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<AudioStreamContext>& audio) const;

    /**
     * @brief Build and cache FileRegion list from both contexts.
     */
    void build_regions(const std::shared_ptr<FFmpegDemuxContext>& demux,
        const std::shared_ptr<AudioStreamContext>& audio) const;

    /**
     * @brief Set the last error message.
     * @param error Error string.
     */
    void set_error(const std::string& error) const;

    /**
     * @brief Clear the last error message.
     */
    void clear_error() const;
};

} // namespace MayaFlux::IO
