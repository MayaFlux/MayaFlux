#pragma once

#include "FFmpegDemuxContext.hpp"

extern "C" {
struct AVCodecContext;
struct SwsContext;
struct AVFrame;
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
     * @brief Open codec only, without initialising the SwsContext scaler.
     *
     * Intended for live capture devices (dshow, v4l2, avfoundation) where
     * pix_fmt is AV_PIX_FMT_NONE until the first decoded frame arrives and
     * sws_getContext therefore cannot be called at open time.
     *
     * After this call is_codec_valid() returns true; is_valid() returns false
     * until rebuild_scaler_from_frame() has been called successfully.
     *
     * @param demux          Open demux context (must outlive this object).
     * @param target_width   Desired output width  (0 = use frame width).
     * @param target_height  Desired output height (0 = use frame height).
     * @param target_format  Desired AVPixelFormat  (negative = AV_PIX_FMT_RGBA).
     * @return True if codec was opened successfully.
     */
    [[nodiscard]] bool open_device(const FFmpegDemuxContext& demux,
        uint32_t target_width = 0,
        uint32_t target_height = 0,
        int target_format = -1);

    /**
     * @brief True if the codec context is open and ready to receive packets.
     *        Does NOT require the SwsContext scaler to be initialised.
     *        Use this check in live-capture paths instead of is_valid().
     */
    [[nodiscard]] bool is_codec_valid() const
    {
        return codec_context && stream_index >= 0;
    }

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

    /**
     * @brief Rebuild the SwsContext using the pixel format resolved from a live
     *        decoded frame.
     *
     * dshow and similar capture devices on Windows leave pix_fmt as
     * AV_PIX_FMT_NONE until the first decoded frame arrives.  Call this once
     * from the camera's frame-receive loop on the very first valid AVFrame to
     * finalise the scaler before calling sws_scale.
     *
     * @param frame        The first successfully decoded AVFrame.
     * @param target_width  Desired output width  (0 = keep frame width).
     * @param target_height Desired output height (0 = keep frame height).
     * @param target_format Desired AVPixelFormat  (negative = AV_PIX_FMT_RGBA).
     * @return True if the scaler was (re)initialised successfully.
     */
    [[nodiscard]] bool rebuild_scaler_from_frame(
        const AVFrame* frame,
        uint32_t target_width = 0,
        uint32_t target_height = 0,
        int target_format = -1);

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
    uint64_t total_frames {};
    uint32_t width {}; ///< Source width in pixels.
    uint32_t height {}; ///< Source height in pixels.
    uint32_t out_width {}; ///< Output width after scaling.
    uint32_t out_height {}; ///< Output height after scaling.
    double frame_rate {}; ///< Average frame rate (fps).
    int src_pixel_format = -1; ///< Source AVPixelFormat.
    int out_pixel_format = -1; ///< Output AVPixelFormat.
    uint32_t out_bytes_per_pixel = 4; ///< Bytes per pixel in output format.
    int out_linesize {}; ///< Output row stride in bytes.

    uint32_t target_width {}; ///< Requested output width  (0 = source).
    uint32_t target_height {}; ///< Requested output height (0 = source).
    int target_format = -1; ///< Requested AVPixelFormat (negative = RGBA).

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
