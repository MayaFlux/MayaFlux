#pragma once

extern "C" {
struct AVFormatContext;
struct AVStream;
struct AVDictionaryEntry;
}

#include "MayaFlux/IO/FileReader.hpp"

namespace MayaFlux::IO {

/**
 * @class FFmpegDemuxContext
 * @brief RAII owner of a single AVFormatContext and associated demux state.
 *
 * Encapsulates all format-level (container) FFmpeg operations:
 * - Opening and probing a media file via libavformat
 * - Enumerating streams and selecting best stream per media type
 * - Seeking at the format level (av_seek_frame)
 * - Extracting container-level metadata tags and chapter regions
 * - FFmpeg library initialization (once per process)
 *
 * Does NOT own any codec context, resampler, or scaler — those are
 * domain-specific and belong to AudioStreamContext / VideoStreamContext.
 *
 * Shared ownership via shared_ptr allows multiple stream contexts to
 * reference the same demuxer without duplicating the format state.
 * Not copyable or movable; always heap-allocated through make_shared.
 */
class MAYAFLUX_API FFmpegDemuxContext {
public:
    FFmpegDemuxContext() = default;
    ~FFmpegDemuxContext();

    FFmpegDemuxContext(const FFmpegDemuxContext&) = delete;
    FFmpegDemuxContext& operator=(const FFmpegDemuxContext&) = delete;
    FFmpegDemuxContext(FFmpegDemuxContext&&) = delete;
    FFmpegDemuxContext& operator=(FFmpegDemuxContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open a media file and probe stream information.
     * @param filepath Absolute or relative path to the media file.
     * @return True on success; false sets internal error string.
     */
    bool open(const std::string& filepath);

    /**
     * @brief Close the format context and release all demux resources.
     *        Safe to call multiple times.
     */
    void close();

    /**
     * @brief True if the format context is open and stream info was found.
     */
    [[nodiscard]] bool is_open() const { return format_context != nullptr; }

    // =========================================================================
    // Stream discovery
    // =========================================================================

    /**
     * @brief Find the best stream of the requested media type.
     * @param media_type AVMediaType value (AVMEDIA_TYPE_AUDIO, AVMEDIA_TYPE_VIDEO, …)
     * @param out_codec  Receives a pointer to the matched codec (may be nullptr if
     *                   caller does not need it).
     * @return Stream index ≥ 0 on success, negative on failure.
     */
    [[nodiscard]] int find_best_stream(int media_type, const void** out_codec = nullptr) const;

    /**
     * @brief Access a stream by index.
     * @param index Stream index in format_context->streams[].
     * @return Pointer to AVStream, or nullptr if index is out of range.
     */
    [[nodiscard]] AVStream* get_stream(int index) const;

    /**
     * @brief Number of streams in the container.
     */
    [[nodiscard]] unsigned int stream_count() const;

    // =========================================================================
    // Seeking
    // =========================================================================

    /**
     * @brief Seek to the nearest keyframe at or before the given timestamp.
     * @param stream_index Target stream index.
     * @param timestamp    Timestamp in the stream's time_base units.
     * @return True on success.
     */
    bool seek(int stream_index, int64_t timestamp);

    /**
     * @brief Flush the demuxer's internal read buffers.
     *        Call after seek before reading new packets.
     */
    void flush();

    // =========================================================================
    // Metadata / regions
    // =========================================================================

    /**
     * @brief Extract container-level metadata tags into a FileMetadata attributes map.
     * @param out_metadata Target metadata struct; attributes field is populated.
     */
    void extract_container_metadata(FileMetadata& out_metadata) const;

    /**
     * @brief Extract chapter information as FileRegion entries.
     * @return Vector of FileRegion with type="chapter".
     */
    [[nodiscard]] std::vector<FileRegion> extract_chapter_regions() const;

    /**
     * @brief Total container duration in seconds, or 0 if unknown.
     */
    [[nodiscard]] double duration_seconds() const;

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // FFmpeg global init (idempotent, thread-safe)
    // =========================================================================

    /**
     * @brief Initialise FFmpeg logging level once per process.
     *        Called automatically by the constructor of every stream context.
     */
    static void init_ffmpeg();

    // =========================================================================
    // Raw handle — accessible to stream contexts for packet reading
    // =========================================================================

    AVFormatContext* format_context = nullptr; ///< Owned; freed in destructor.

private:
    std::string m_last_error;

    static std::atomic<bool> s_ffmpeg_initialized;
    static std::mutex s_ffmpeg_init_mutex;
};

} // namespace MayaFlux::IO
