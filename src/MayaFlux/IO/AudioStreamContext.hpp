#pragma once

#include "FFmpegDemuxContext.hpp"

extern "C" {
struct AVCodecContext;
struct SwrContext;
}

namespace MayaFlux::IO {

/**
 * @class AudioStreamContext
 * @brief RAII owner of one audio stream's codec and resampler state.
 *
 * Encapsulates all audio-stream-specific FFmpeg objects:
 * - AVCodecContext for the selected audio stream
 * - SwrContext for sample-format conversion and optional resampling
 * - Cached audio parameters: sample_rate, channel count, total_frames
 *
 * Does NOT own AVFormatContext — that belongs to FFmpegDemuxContext.
 * Packet reading is always delegated to the demuxer's format_context;
 * this context only decodes and converts packets it receives.
 *
 * Destruction order (enforced in destructor):
 *   swr_context → codec_context
 * The associated FFmpegDemuxContext must outlive this object.
 */
class MAYAFLUX_API AudioStreamContext {
public:
    AudioStreamContext() = default;
    ~AudioStreamContext();

    AudioStreamContext(const AudioStreamContext&) = delete;
    AudioStreamContext& operator=(const AudioStreamContext&) = delete;
    AudioStreamContext(AudioStreamContext&&) = delete;
    AudioStreamContext& operator=(AudioStreamContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open the audio stream from an already-probed demux context.
     *
     * Finds the best audio stream, allocates and opens the codec context,
     * caches audio parameters, and initialises the SwrContext for conversion
     * to AV_SAMPLE_FMT_DBL (or AV_SAMPLE_FMT_DBLP if planar output requested).
     *
     * @param demux          Open demux context (must outlive this object).
     * @param planar_output  If true, configure swr for planar double output.
     * @param target_rate    Resample target in Hz; 0 = keep source rate.
     * @return True on success.
     */
    bool open(const FFmpegDemuxContext& demux,
        bool planar_output = false,
        uint32_t target_rate = 0);

    /**
     * @brief Release codec and resampler resources.
     *        Safe to call multiple times.
     */
    void close();

    /**
     * @brief True if the codec and resampler are ready for decoding.
     */
    [[nodiscard]] bool is_valid() const
    {
        return codec_context && swr_context && stream_index >= 0;
    }

    // =========================================================================
    // Codec flush
    // =========================================================================

    /**
     * @brief Flush codec internal buffers (call after a seek).
     */
    void flush_codec();

    /**
     * @brief Drain any samples buffered inside the resampler.
     *
     * swr_init() and seek+flush sequences can leave delay-compensation
     * samples inside SwrContext. Calling this discards them so that the
     * next decode_frames() call starts from a clean resampler state.
     * Must be called after open() and after every seek+flush pair.
     */
    void drain_resampler_init();

    // =========================================================================
    // Stream-level metadata extraction
    // =========================================================================

    /**
     * @brief Populate stream-specific fields into an existing FileMetadata.
     *        Appends codec name, channel layout, bit_rate, sample_rate, etc.
     * @param demux     The demux context that owns the format_context.
     * @param out       Metadata struct to append into.
     */
    void extract_stream_metadata(const FFmpegDemuxContext& demux, FileMetadata& out) const;

    /**
     * @brief Extract cue/marker regions from stream metadata tags.
     * @param demux The demux context.
     * @return Vector of FileRegion with type="cue" or "marker".
     */
    [[nodiscard]] std::vector<FileRegion> extract_cue_regions(const FFmpegDemuxContext& demux) const;

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // Owned handles — accessible to SoundFileReader for decode loops
    // =========================================================================

    AVCodecContext* codec_context = nullptr; ///< Owned; freed in destructor.
    SwrContext* swr_context = nullptr; ///< Owned; freed in destructor.

    int stream_index = -1;
    uint64_t total_frames = 0;
    uint32_t sample_rate = 0;
    uint32_t channels = 0;

private:
    std::string m_last_error;

    bool setup_resampler(bool planar_output, uint32_t target_rate);
};

} // namespace MayaFlux::IO
