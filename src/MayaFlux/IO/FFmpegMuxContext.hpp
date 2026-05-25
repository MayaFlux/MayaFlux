#pragma once

extern "C" {
struct AVFormatContext;
struct AVStream;
struct AVPacket;
}

namespace MayaFlux::IO {

/**
 * @class FFmpegMuxContext
 * @brief RAII owner of a single AVFormatContext on the write path.
 *
 * Encapsulates all format-level (container) FFmpeg mux operations:
 * - Allocating an output context and opening the I/O layer (avio)
 * - Writing the container header after all streams have been added
 * - Writing interleaved encoded packets
 * - Writing the container trailer and releasing all resources
 *
 * Does NOT own any codec context, resampler, scaler, or stream — those are
 * domain-specific and belong to AudioEncodeContext / VideoEncodeContext, which
 * each call avformat_new_stream() on this context during their own open().
 *
 * Stream contexts must be opened and fully configured before write_header()
 * is called. Packets submitted via write_packet() must have stream_index and
 * PTS/DTS set by the originating encode context.
 *
 * Not copyable or movable; always stack- or member-allocated inside the owner
 * (SoundFileWriter worker thread, future VideoFileWriter worker thread).
 *
 * FFmpeg global initialisation is shared with FFmpegDemuxContext via the
 * static FFmpegDemuxContext::init_ffmpeg() call.
 */
class MAYAFLUX_API FFmpegMuxContext {
public:
    FFmpegMuxContext() = default;
    ~FFmpegMuxContext();

    FFmpegMuxContext(const FFmpegMuxContext&) = delete;
    FFmpegMuxContext& operator=(const FFmpegMuxContext&) = delete;
    FFmpegMuxContext(FFmpegMuxContext&&) = delete;
    FFmpegMuxContext& operator=(FFmpegMuxContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Allocate an output context and open the avio layer for writing.
     *
     * Format is inferred from the filepath extension via
     * avformat_alloc_output_context2. Pass explicit_format to override
     * (e.g. "matroska" to force MKV regardless of extension).
     *
     * Does NOT write any data to disk. Call write_header() after all stream
     * contexts have been opened on this mux context.
     *
     * @param filepath        Output file path. Extension determines container.
     * @param explicit_format FFmpeg short format name override; empty = infer.
     * @return True on success; false sets the internal error string.
     */
    bool open(const std::string& filepath,
        const std::string& explicit_format = {});

    /**
     * @brief Write the container trailer, flush avio, and release all resources.
     *
     * av_write_trailer is called only if write_header() previously succeeded.
     * Safe to call multiple times or on a context that was never successfully
     * opened.
     */
    void close();

    /**
     * @brief True if the context is open and ready to accept streams / packets.
     */
    [[nodiscard]] bool is_open() const { return format_context != nullptr; }

    /**
     * @brief True if write_header() completed successfully.
     *
     * Encode contexts may use this to assert sequencing: packets must not be
     * submitted before the header is written.
     */
    [[nodiscard]] bool is_header_written() const { return m_header_written; }

    // =========================================================================
    // Stream registration
    // =========================================================================

    /**
     * @brief Allocate a new AVStream inside this context.
     *
     * Called by AudioEncodeContext::open() and VideoEncodeContext::open()
     * to add their respective streams before write_header().
     *
     * @return Pointer to the new AVStream, or nullptr on failure.
     */
    [[nodiscard]] AVStream* add_stream();

    // =========================================================================
    // Header / packet writing
    // =========================================================================

    /**
     * @brief Write the container header to the output file.
     *
     * Must be called after all stream contexts (audio, video, subtitle, etc.)
     * have been opened and have set their codec parameters on their AVStream.
     * Exactly one call is valid per open(); calling twice is an error.
     *
     * @return True on success.
     */
    bool write_header();

    /**
     * @brief Submit one encoded packet for interleaved writing.
     *
     * Uses av_interleaved_write_frame, which buffers packets internally to
     * preserve correct interleaving order for containers that require it
     * (MP4, MKV). For single-stream audio-only containers (WAV, FLAC) this
     * is equivalent to av_write_frame.
     *
     * The packet's stream_index, PTS, DTS, and duration must be set by the
     * calling encode context before this call. Ownership of the packet data
     * is transferred to FFmpeg; the caller must not reference pkt after return.
     *
     * @param pkt Encoded packet with all fields populated.
     * @return True on success.
     */
    bool write_packet(AVPacket* pkt);

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // Raw handle — accessible to encode contexts for stream setup
    // =========================================================================

    AVFormatContext* format_context = nullptr; ///< Owned; freed in close().

private:
    bool m_header_written {};
    std::string m_last_error;
};

} // namespace MayaFlux::IO
