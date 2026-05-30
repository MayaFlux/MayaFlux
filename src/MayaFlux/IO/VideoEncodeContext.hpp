#pragma once

#include "FFmpegMuxContext.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
struct AVStream;
struct SwsContext;
}

namespace MayaFlux::IO {

/**
 * @class VideoEncodeContext
 * @brief RAII owner of one video stream's encoder and pixel-format converter on the write path.
 *
 * Encapsulates all video-stream-specific FFmpeg objects:
 * - AVCodecContext for the selected or inferred encoder
 * - SwsContext converting from the source pixel format (e.g. AV_PIX_FMT_BGRA, as
 *   delivered by the Vulkan swapchain readback) to the encoder's required format
 *   (typically AV_PIX_FMT_YUV420P for H.264/H.265)
 * - AVFrame scratch buffer for sws_scale output
 * - Monotonically increasing PTS counter
 *
 * Does NOT own AVFormatContext — that belongs to FFmpegMuxContext.
 *
 * Open sequence:
 *   1. FFmpegMuxContext::open()
 *   2. VideoEncodeContext::open()      — adds stream, configures codec + sws
 *   3. FFmpegMuxContext::write_header()
 *   4. encode_frame() loop
 *   5. drain()                         — flushes encoder delay
 *   6. FFmpegMuxContext::close()       — writes trailer
 *
 * The source pixel format passed to open() must match the format delivered
 * to every subsequent encode_frame() call. Swapchain BGRA readbacks map to
 * AV_PIX_FMT_BGRA; RGBA readbacks to AV_PIX_FMT_RGBA.
 *
 * Not copyable or movable; always stack- or member-allocated inside the owner
 * (VideoFileWriter worker thread).
 *
 * The associated FFmpegMuxContext must outlive this object.
 */
class MAYAFLUX_API VideoEncodeContext {
public:
    VideoEncodeContext() = default;
    ~VideoEncodeContext();

    VideoEncodeContext(const VideoEncodeContext&) = delete;
    VideoEncodeContext& operator=(const VideoEncodeContext&) = delete;
    VideoEncodeContext(VideoEncodeContext&&) = delete;
    VideoEncodeContext& operator=(VideoEncodeContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open the encoder and register a video stream in the mux context.
     *
     * Selects the encoder: if codec_id is AV_CODEC_ID_NONE the container's
     * default video codec is used (MP4/MKV -> H.264, WebM -> VP9, AVI -> MPEG4).
     * Allocates AVCodecContext, initialises SwsContext converting from
     * src_pixel_format to the encoder's required pixel format, allocates the
     * scratch AVFrame, and copies codec parameters to the new AVStream.
     *
     * Must be called after FFmpegMuxContext::open() and before
     * FFmpegMuxContext::write_header().
     *
     * @param mux              Open mux context that will own the output stream.
     * @param width            Frame width in pixels.
     * @param height           Frame height in pixels.
     * @param frame_rate       Average frame rate in frames per second.
     * @param src_pixel_format Source AVPixelFormat delivered by the readback thread.
     * @param codec_id         Encoder override; AV_CODEC_ID_NONE = container default.
     * @return True on success; false sets the internal error string.
     */
    bool open(FFmpegMuxContext& mux,
        uint32_t width,
        uint32_t height,
        double frame_rate,
        AVPixelFormat src_pixel_format,
        AVCodecID codec_id);

    /**
     * @brief Release all owned resources.
     *        Safe to call multiple times or on a context that never opened.
     */
    void close();

    /**
     * @brief True if codec, scaler, and scratch frame are all ready.
     */
    [[nodiscard]] bool is_valid() const
    {
        return codec_context && sws_context && m_scratch_frame && m_stream_index >= 0;
    }

    // =========================================================================
    // Encoding
    // =========================================================================

    /**
     * @brief Encode one raw pixel frame into the mux context.
     *
     * Converts src_data from the source pixel format to the encoder's required
     * format via SwsContext, stamps the monotonic PTS, and submits the frame
     * to the encoder. Drains any available packets to the mux immediately.
     *
     * src_data must be a packed, row-major image of exactly width * height *
     * bytes_per_pixel(src_pixel_format) bytes as delivered by the swapchain
     * readback. No stride padding is expected; the source linesize is computed
     * as width * bytes_per_pixel.
     *
     * Non-blocking with respect to the observer thread: this runs exclusively
     * on the VideoFileWriter worker thread.
     *
     * @param src_data Pointer to the first byte of the packed source image.
     * @param src_size Byte count of src_data; must equal width * height * bpp.
     * @param mux      Mux context to receive encoded packets.
     * @return True on success.
     */
    bool encode_frame(const uint8_t* src_data,
        size_t src_size,
        uint32_t src_width,
        uint32_t src_height,
        FFmpegMuxContext& mux);

    /**
     * @brief Flush all buffered frames from the encoder to the mux.
     *
     * Sends a null frame to signal EOS to the encoder and drains all remaining
     * output packets. Call once before FFmpegMuxContext::close().
     *
     * @param mux Mux context to receive remaining packets.
     * @return True if all remaining packets were written successfully.
     */
    bool drain(FFmpegMuxContext& mux);

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // Owned handles
    // =========================================================================

    AVCodecContext* codec_context {}; ///< Owned; freed in destructor.
    SwsContext* sws_context {}; ///< Owned; freed in destructor.

    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }

private:
    AVStream* m_stream {}; ///< Weak ref into FFmpegMuxContext; not owned.
    AVFrame* m_scratch_frame {}; ///< Owned scratch buffer for encoder input.
    int64_t m_pts {};
    int m_stream_index { -1 };
    uint32_t m_width {};
    uint32_t m_height {};
    int m_src_src_bpp { 4 };
    AVPixelFormat m_src_pixel_fmt { AV_PIX_FMT_BGRA };
    int m_cached_src_width { -1 };
    int m_cached_src_height { -1 };

    std::string m_last_error;

    bool drain_packets(FFmpegMuxContext& mux);
};

} // namespace MayaFlux::IO
