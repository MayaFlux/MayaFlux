#include "VideoEncodeContext.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace MayaFlux::IO {

namespace {

    /**
     * @brief Select encoder codec for a given container format.
     *
     * Falls back to H.264 for any unrecognised container. MP4 and MKV default
     * to H.264 because it has the widest hardware decode support. WebM defaults
     * to VP9. AVI defaults to MPEG-4 Part 2 for maximum compatibility.
     */
    AVCodecID infer_video_codec(AVFormatContext* fmt_ctx)
    {
        if (!fmt_ctx || !fmt_ctx->oformat)
            return AV_CODEC_ID_H264;

        const char* name = fmt_ctx->oformat->name;
        if (!name)
            return AV_CODEC_ID_H264;

        if (std::string_view(name).find("webm") != std::string_view::npos)
            return AV_CODEC_ID_VP9;
        if (std::string_view(name).find("avi") != std::string_view::npos)
            return AV_CODEC_ID_MPEG4;

        return AV_CODEC_ID_H264;
    }

    /**
     * @brief Resolve bytes-per-pixel for a packed AVPixelFormat.
     *
     * Only the formats the swapchain readback can deliver are handled here.
     * Any unrecognised format falls back to 4 (BGRA/RGBA assumption).
     */
    int bpp_for_format(AVPixelFormat fmt)
    {
        switch (fmt) {
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_ABGR:
            return 4;
        case AV_PIX_FMT_BGR24:
        case AV_PIX_FMT_RGB24:
            return 3;
        case AV_PIX_FMT_RGBA64LE:
        case AV_PIX_FMT_RGBA64BE:
        case AV_PIX_FMT_BGRA64LE:
        case AV_PIX_FMT_BGRA64BE:
            return 8;
        default:
            return 4;
        }
    }

} // namespace

// =========================================================================
// Destructor
// =========================================================================

VideoEncodeContext::~VideoEncodeContext()
{
    close();
}

void VideoEncodeContext::close()
{
    if (m_scratch_frame) {
        av_frame_free(&m_scratch_frame);
        m_scratch_frame = nullptr;
    }
    if (sws_context) {
        sws_freeContext(sws_context);
        sws_context = nullptr;
    }
    if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
    }
    m_stream = nullptr;
    m_stream_index = -1;
    m_pts = 0;
    m_width = 0;
    m_height = 0;
    m_last_error.clear();
}

// =========================================================================
// Open
// =========================================================================

bool VideoEncodeContext::open(FFmpegMuxContext& mux,
    uint32_t width,
    uint32_t height,
    double frame_rate,
    AVPixelFormat src_pixel_format,
    AVCodecID codec_id)
{
    close();

    if (!mux.is_open()) {
        m_last_error = "VideoEncodeContext::open: mux context is not open";
        return false;
    }

    if (width == 0 || height == 0 || frame_rate <= 0.0) {
        m_last_error = "VideoEncodeContext::open: invalid dimensions or frame rate";
        return false;
    }

    m_width = width;
    m_height = height;
    m_src_src_bpp = bpp_for_format(src_pixel_format);

    if (codec_id == AV_CODEC_ID_NONE)
        codec_id = infer_video_codec(mux.format_context);

    const AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        m_last_error = std::string("avcodec_find_encoder failed for codec_id=")
            + std::to_string(static_cast<int>(codec_id));
        return false;
    }

    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        m_last_error = "avcodec_alloc_context3 failed";
        return false;
    }

    ///< H.264/H.265 require dimensions divisible by 2
    codec_context->width = static_cast<int>(width % 2 == 0 ? width : width - 1);
    codec_context->height = static_cast<int>(height % 2 == 0 ? height : height - 1);

    /*
     * Convert double fps to AVRational. Use millisecond time_base so fractional
     * rates (23.976, 29.97, 59.94) round-trip without drift.
     */
    const int fps_num = static_cast<int>(std::round(frame_rate * 1000.0));
    codec_context->framerate = { .num = fps_num, .den = 1000 };
    codec_context->time_base = { .num = 1000, .den = fps_num };

    /*
     * YUV420P is the universally supported pixel format for H.264/H.265.
     * VP9 and MPEG4 also accept it. For codecs that require a different native
     * format, callers must pass an appropriate codec_id and the caller-side
     * src_pixel_format. The sws_scale below handles any conversion.
     */
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

    /*
     *Reasonable quality defaults — not tunable here; callers set codec options
     * via AVDictionary on codec_context before open() returns if needed.
     */
    codec_context->bit_rate = 0; ///< let encoder decide from crf
    if (codec_id == AV_CODEC_ID_H264 || codec_id == AV_CODEC_ID_H265) {
        av_opt_set(codec_context->priv_data, "preset", "fast", 0);
        av_opt_set(codec_context->priv_data, "crf", "23", 0);
    }

    if (mux.format_context->oformat->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        m_last_error = "avcodec_open2 failed";
        close();
        return false;
    }

    m_stream = mux.add_stream();
    if (!m_stream) {
        m_last_error = "FFmpegMuxContext::add_stream returned nullptr";
        close();
        return false;
    }

    if (avcodec_parameters_from_context(m_stream->codecpar, codec_context) < 0) {
        m_last_error = "avcodec_parameters_from_context failed";
        close();
        return false;
    }

    m_stream->time_base = codec_context->time_base;
    m_stream_index = m_stream->index;

    // -------------------------------------------------------------------------
    // SwsContext: source format -> YUV420P
    // -------------------------------------------------------------------------
    sws_context = sws_getContext(
        codec_context->width,
        codec_context->height,
        src_pixel_format,
        codec_context->width,
        codec_context->height,
        codec_context->pix_fmt,
        SWS_BILINEAR,
        nullptr, nullptr, nullptr);

    if (!sws_context) {
        m_last_error = "sws_getContext failed";
        close();
        return false;
    }

    // -------------------------------------------------------------------------
    // Scratch frame for sws_scale output
    // -------------------------------------------------------------------------
    m_scratch_frame = av_frame_alloc();
    if (!m_scratch_frame) {
        m_last_error = "av_frame_alloc failed";
        close();
        return false;
    }

    m_scratch_frame->format = codec_context->pix_fmt;
    m_scratch_frame->width = codec_context->width;
    m_scratch_frame->height = codec_context->height;

    if (av_frame_get_buffer(m_scratch_frame, 32) < 0) {
        m_last_error = "av_frame_get_buffer failed";
        close();
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoEncodeContext] open: {}x{} @ {:.3f} fps | src={} enc={} stream#{}",
        codec_context->width, codec_context->height, frame_rate,
        av_get_pix_fmt_name(src_pixel_format) ? av_get_pix_fmt_name(src_pixel_format) : "unknown",
        avcodec_get_name(codec_id),
        m_stream_index);

    return true;
}

// =========================================================================
// Encoding
// =========================================================================

bool VideoEncodeContext::encode_frame(const uint8_t* src_data,
    size_t src_size,
    FFmpegMuxContext& mux)
{
    if (!is_valid())
        return false;

    const size_t expected = static_cast<size_t>(codec_context->width)
        * static_cast<size_t>(codec_context->height)
        * static_cast<size_t>(m_src_src_bpp);

    if (src_size < expected) {
        m_last_error = "encode_frame: src_size too small for declared dimensions";
        return false;
    }

    if (av_frame_make_writable(m_scratch_frame) < 0) {
        m_last_error = "av_frame_make_writable failed";
        return false;
    }

    ///< Source linesize: packed, no padding
    const int src_stride = codec_context->width * m_src_src_bpp;
    sws_scale(sws_context,
        &src_data, &src_stride,
        0, codec_context->height,
        m_scratch_frame->data, m_scratch_frame->linesize);

    m_scratch_frame->pts = m_pts++;

    if (avcodec_send_frame(codec_context, m_scratch_frame) < 0) {
        m_last_error = "avcodec_send_frame failed";
        return false;
    }

    return drain_packets(mux);
}

// =========================================================================
// Drain
// =========================================================================

bool VideoEncodeContext::drain(FFmpegMuxContext& mux)
{
    if (!is_valid())
        return false;

    if (avcodec_send_frame(codec_context, nullptr) < 0) {
        m_last_error = "avcodec_send_frame(null) failed during drain";
        return false;
    }

    return drain_packets(mux);
}

// =========================================================================
// Private helpers
// =========================================================================

bool VideoEncodeContext::drain_packets(FFmpegMuxContext& mux)
{
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        m_last_error = "av_packet_alloc failed";
        return false;
    }

    bool ok = true;
    while (true) {
        int ret = avcodec_receive_packet(codec_context, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            m_last_error = std::string("avcodec_receive_packet failed: ") + errbuf;
            ok = false;
            break;
        }

        pkt->stream_index = m_stream_index;
        av_packet_rescale_ts(pkt,
            codec_context->time_base,
            m_stream->time_base);

        if (!mux.write_packet(pkt)) {
            m_last_error = mux.last_error();
            ok = false;
            break;
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    return ok;
}

} // namespace MayaFlux::IO
