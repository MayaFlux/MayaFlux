#pragma once

#include "FFmpegDemuxContext.hpp"

extern "C" {
struct AVCodecContext;
struct SwsContext;
}

namespace MayaFlux::IO {

/**
 * @class VideoStreamContext
 * @brief RAII owner of one video stream's codec and pixel-format scaler state.
 *
 * Encapsulates all video-stream-specific FFmpeg objects:
 * - AVCodecContext for the selected video stream
 * - SwsContext for pixel-format conversion and optional rescaling
 * - Cached video parameters: width, height, frame_rate, total_frames, pixel_format
 *
 * Does NOT own AVFormatContext — that belongs to FFmpegDemuxContext.
 * Packet reading is always delegated to the demuxer's format_context;
 * this context only decodes and converts packets it receives.
 *
 * The default output pixel format is AV_PIX_FMT_RGBA (4 bytes per pixel),
 * chosen for direct compatibility with Vulkan's VK_FORMAT_R8G8B8A8_UNORM
 * and the MayaFlux TextureBuffer / VKImage pipeline. For HDR workflows
 * or compute-shader ingestion, callers can request AV_PIX_FMT_RGBAF32
 * or other formats via the target_format parameter.
 *
 * Destruction order (enforced in destructor):
 *   sws_context → codec_context
 * The associated FFmpegDemuxContext must outlive this object.
 */
class MAYAFLUX_API VideoStreamContext {
public:
    VideoStreamContext() = default;
    ~VideoStreamContext();

    VideoStreamContext(const VideoStreamContext&) = delete;
    VideoStreamContext& operator=(const VideoStreamContext&) = delete;
    VideoStreamContext(VideoStreamContext&&) = delete;
    VideoStreamContext& operator=(VideoStreamContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open the video stream from an already-probed demux context.
     *
     * Finds the best video stream, allocates and opens the codec context,
     * caches video parameters, and initialises the SwsContext for conversion
     * to the target pixel format (default AV_PIX_FMT_RGBA).
     *
     * @param demux          Open demux context (must outlive this object).
     * @param target_width   Output width in pixels; 0 = keep source width.
     * @param target_height  Output height in pixels; 0 = keep source height.
     * @param target_format  Target AVPixelFormat; negative = AV_PIX_FMT_RGBA.
     * @return True on success.
     */
    bool open(const FFmpegDemuxContext& demux,
        uint32_t target_width = 0,
        uint32_t target_height = 0,
        int target_format = -1);

    /**
     * @brief Release codec and scaler resources.
     *        Safe to call multiple times.
     */
    void close();

    /**
     * @brief True if the codec and scaler are ready for decoding.
     */
    [[nodiscard]] bool is_valid() const
    {
        return codec_context && sws_context && stream_index >= 0;
    }

    // =========================================================================
    // Codec flush
    // =========================================================================

    /**
     * @brief Flush codec internal buffers (call after a seek).
     */
    void flush_codec();

    // =========================================================================
    // Stream-level metadata extraction
    // =========================================================================

    /**
     * @brief Populate stream-specific fields into an existing FileMetadata.
     *
     * Appends codec name, pixel format, dimensions, frame rate, bit_rate, etc.
     *
     * @param demux The demux context that owns the format_context.
     * @param out   Metadata struct to append into.
     */
    void extract_stream_metadata(const FFmpegDemuxContext& demux, FileMetadata& out) const;

    /**
     * @brief Extract keyframe positions as FileRegion entries.
     * @param demux The demux context.
     * @return Vector of FileRegion with type="keyframe".
     */
    [[nodiscard]] std::vector<FileRegion> extract_keyframe_regions(const FFmpegDemuxContext& demux) const;

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // Owned handles — accessible to VideoFileReader for decode loops
    // =========================================================================

    AVCodecContext* codec_context = nullptr; ///< Owned; freed in destructor.
    SwsContext* sws_context = nullptr; ///< Owned; freed in destructor.

    int stream_index = -1;
    uint64_t total_frames = 0;
    uint32_t width = 0; ///< Source width in pixels.
    uint32_t height = 0; ///< Source height in pixels.
    uint32_t out_width = 0; ///< Output width after scaling.
    uint32_t out_height = 0; ///< Output height after scaling.
    double frame_rate = 0.0; ///< Average frame rate (fps).
    int src_pixel_format = -1; ///< Source AVPixelFormat.
    int out_pixel_format = -1; ///< Output AVPixelFormat.
    uint32_t out_bytes_per_pixel = 4; ///< Bytes per pixel in output format.
    int out_linesize = 0; ///< Output row stride in bytes.

private:
    std::string m_last_error;

    /**
     * @brief Allocate and initialise the SwsContext for pixel format conversion.
     * @param target_width  Desired output width (0 = source width).
     * @param target_height Desired output height (0 = source height).
     * @param target_format Desired AVPixelFormat (negative = AV_PIX_FMT_RGBA).
     * @return True on success.
     */
    bool setup_scaler(uint32_t target_width, uint32_t target_height, int target_format);
};

} // namespace MayaFlux::IO
