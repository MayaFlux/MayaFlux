#include "VideoStreamContext.hpp"
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

// =========================================================================
// Destructor
// =========================================================================

VideoStreamContext::~VideoStreamContext()
{
    close();
}

void VideoStreamContext::close()
{
    if (sws_context) {
        sws_freeContext(sws_context);
        sws_context = nullptr;
    }
    if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
    }
    stream_index = -1;
    total_frames = 0;
    width = 0;
    height = 0;
    out_width = 0;
    out_height = 0;
    frame_rate = 0.0;
    src_pixel_format = -1;
    out_pixel_format = -1;
    out_bytes_per_pixel = 4;
    out_linesize = 0;
    m_last_error.clear();
}

// =========================================================================
// Open
// =========================================================================

bool VideoStreamContext::open(const FFmpegDemuxContext& demux,
    uint32_t target_width,
    uint32_t target_height,
    int target_format)
{
    close();
    FFmpegDemuxContext::init_ffmpeg();

    if (!demux.is_open()) {
        m_last_error = "Demux context is not open";
        return false;
    }

    const AVCodec* codec = nullptr;
    stream_index = demux.find_best_stream(AVMEDIA_TYPE_VIDEO,
        reinterpret_cast<const void**>(&codec));
    if (stream_index < 0 || !codec) {
        m_last_error = "No video stream found";
        return false;
    }

    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        m_last_error = "avcodec_alloc_context3 failed";
        return false;
    }

    AVStream* stream = demux.get_stream(stream_index);
    if (avcodec_parameters_to_context(codec_context, stream->codecpar) < 0) {
        m_last_error = "avcodec_parameters_to_context failed";
        close();
        return false;
    }

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        m_last_error = "avcodec_open2 failed";
        close();
        return false;
    }

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    uint32_t probed_width = 0;
    uint32_t probed_height = 0;

    if (codec_context->pix_fmt == AV_PIX_FMT_NONE) {
        AVPacket* probe_pkt = av_packet_alloc();
        AVFrame* probe_frm = av_frame_alloc();

        if (probe_pkt && probe_frm) {
            AVFormatContext* fmt = demux.format_context;
            bool probed = false;

            while (!probed && av_read_frame(fmt, probe_pkt) >= 0) {
                if (probe_pkt->stream_index != stream_index) {
                    av_packet_unref(probe_pkt);
                    continue;
                }
                if (avcodec_send_packet(codec_context, probe_pkt) >= 0) {
                    if (avcodec_receive_frame(codec_context, probe_frm) >= 0) {
                        if (codec_context->pix_fmt == AV_PIX_FMT_NONE
                            && probe_frm->format != AV_PIX_FMT_NONE) {
                            codec_context->pix_fmt = static_cast<AVPixelFormat>(probe_frm->format);
                        }
                        probed_width = static_cast<uint32_t>(probe_frm->width);
                        probed_height = static_cast<uint32_t>(probe_frm->height);
                        probed = true;
                        av_frame_unref(probe_frm);
                    }
                }
                av_packet_unref(probe_pkt);
            }
        }

        av_packet_free(&probe_pkt);
        av_frame_free(&probe_frm);
        avcodec_flush_buffers(codec_context);
    }

    width = static_cast<uint32_t>(codec_context->width);
    height = static_cast<uint32_t>(codec_context->height);

    if (probed_width > 0 && probed_height > 0) {
        width = probed_width;
        height = probed_height;
        codec_context->width = static_cast<int>(width);
        codec_context->height = static_cast<int>(height);
    }
#else
    width = static_cast<uint32_t>(codec_context->width);
    height = static_cast<uint32_t>(codec_context->height);
#endif

    if (codec_context->codec_id == AV_CODEC_ID_HEVC
        && stream->r_frame_rate.den > 0
        && stream->r_frame_rate.num > 0) {
        frame_rate = av_q2d(stream->r_frame_rate);
    } else if (stream->avg_frame_rate.den > 0 && stream->avg_frame_rate.num > 0) {
        frame_rate = av_q2d(stream->avg_frame_rate);
    } else if (stream->r_frame_rate.den > 0 && stream->r_frame_rate.num > 0) {
        frame_rate = av_q2d(stream->r_frame_rate);
    }

    src_pixel_format = codec_context->pix_fmt;

    if (stream->nb_frames > 0) {
        total_frames = static_cast<uint64_t>(stream->nb_frames);
    } else if (stream->duration != AV_NOPTS_VALUE
        && stream->time_base.num > 0 && stream->time_base.den > 0
        && frame_rate > 0.0) {
        double dur = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
        total_frames = static_cast<uint64_t>(dur * frame_rate);
    } else if (demux.format_context->duration != AV_NOPTS_VALUE && frame_rate > 0.0) {
        double dur = static_cast<double>(demux.format_context->duration)
            / static_cast<double>(AV_TIME_BASE);
        total_frames = static_cast<uint64_t>(dur * frame_rate);
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoStreamContext] stream #{} | "
        "avg_frame_rate={}/{} ({:.6f} fps) | "
        "r_frame_rate={}/{} ({:.6f} fps) | "
        "chosen frame_rate={:.6f} fps | "
        "nb_frames={} | "
        "stream duration={} (tb={}/{}, => {:.4f}s) | "
        "format duration={} (=> {:.4f}s) | "
        "total_frames={} | "
        "source={}",
        stream_index,
        stream->avg_frame_rate.num, stream->avg_frame_rate.den,
        (stream->avg_frame_rate.den > 0 ? av_q2d(stream->avg_frame_rate) : 0.0),
        stream->r_frame_rate.num, stream->r_frame_rate.den,
        (stream->r_frame_rate.den > 0 ? av_q2d(stream->r_frame_rate) : 0.0),
        frame_rate,
        static_cast<int64_t>(stream->nb_frames),
        stream->duration,
        stream->time_base.num, stream->time_base.den,
        (stream->duration != AV_NOPTS_VALUE && stream->time_base.den > 0
                ? static_cast<double>(stream->duration) * av_q2d(stream->time_base)
                : -1.0),
        demux.format_context->duration,
        (demux.format_context->duration != AV_NOPTS_VALUE
                ? static_cast<double>(demux.format_context->duration) / AV_TIME_BASE
                : -1.0),
        total_frames,
        (stream->nb_frames > 0 ? "nb_frames"
                               : (stream->duration != AV_NOPTS_VALUE ? "stream_duration*fps"
                                                                     : "format_duration*fps")));

    if (!setup_scaler(target_width, target_height, target_format)) {
        close();
        return false;
    }

    return true;
}

bool VideoStreamContext::open_device(const FFmpegDemuxContext& demux,
    uint32_t tw, uint32_t th, int tf)
{
    close();
    FFmpegDemuxContext::init_ffmpeg();

    if (!demux.is_open()) {
        m_last_error = "Demux context is not open";
        return false;
    }

    const AVCodec* codec = nullptr;
    stream_index = demux.find_best_stream(AVMEDIA_TYPE_VIDEO,
        reinterpret_cast<const void**>(&codec));
    if (stream_index < 0 || !codec) {
        m_last_error = "No video stream found";
        return false;
    }

    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        m_last_error = "avcodec_alloc_context3 failed";
        return false;
    }

    AVStream* stream = demux.get_stream(stream_index);
    if (avcodec_parameters_to_context(codec_context, stream->codecpar) < 0) {
        m_last_error = "avcodec_parameters_to_context failed";
        close();
        return false;
    }

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        m_last_error = "avcodec_open2 failed";
        close();
        return false;
    }

    target_width = tw;
    target_height = th;
    target_format = tf;

    src_pixel_format = codec_context->pix_fmt;

    width = static_cast<uint32_t>(codec_context->width);
    height = static_cast<uint32_t>(codec_context->height);

    if (width == 0 || height == 0) {
        AVStream* s = demux.get_stream(stream_index);
        width = static_cast<uint32_t>(s->codecpar->width);
        height = static_cast<uint32_t>(s->codecpar->height);
    }

    if (width == 0 || height == 0) {
        width = target_width;
        height = target_height;
    }

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    if (target_width > 0 && target_height > 0
        && width == target_height && height == target_width) {
        std::swap(width, height);
    }
#endif

    out_width = target_width > 0 ? target_width : width;
    out_height = target_height > 0 ? target_height : height;

    if (stream->avg_frame_rate.den > 0 && stream->avg_frame_rate.num > 0)
        frame_rate = av_q2d(stream->avg_frame_rate);
    else if (stream->r_frame_rate.den > 0 && stream->r_frame_rate.num > 0)
        frame_rate = av_q2d(stream->r_frame_rate);

    auto fmt = av_get_pix_fmt_name(static_cast<AVPixelFormat>(src_pixel_format));

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoStreamContext] open_device: stream #{} | {}x{} | pix_fmt={}",
        stream_index, width, height, fmt ? fmt : "none");

    return true;
}

// =========================================================================
// Scaler
// =========================================================================

bool VideoStreamContext::setup_scaler(uint32_t target_width,
    uint32_t target_height,
    int target_format)
{
    if (!codec_context)
        return false;

    if (codec_context->pix_fmt == AV_PIX_FMT_NONE) {
        m_last_error = "setup_scaler: source pix_fmt is AV_PIX_FMT_NONE — "
                       "codec has not resolved its output format yet";
        return false;
    }

    out_width = target_width > 0 ? target_width : width;
    out_height = target_height > 0 ? target_height : height;
    out_pixel_format = target_format >= 0
        ? target_format
        : static_cast<int>(AV_PIX_FMT_RGBA);

    sws_context = sws_getContext(
        static_cast<int>(width),
        static_cast<int>(height),
        codec_context->pix_fmt,
        static_cast<int>(out_width),
        static_cast<int>(out_height),
        static_cast<AVPixelFormat>(out_pixel_format),
        SWS_BILINEAR,
        nullptr, nullptr, nullptr);

    if (!sws_context) {
        m_last_error = "sws_getContext failed";
        return false;
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(
        static_cast<AVPixelFormat>(out_pixel_format));
    if (desc) {
        int bits = 0;
        for (int c = 0; c < desc->nb_components; ++c)
            bits += desc->comp[c].depth;
        out_bytes_per_pixel = static_cast<uint32_t>((bits + 7) / 8);
    } else {
        out_bytes_per_pixel = 4;
    }

    out_linesize = static_cast<int>(out_width * out_bytes_per_pixel);
    int align_remainder = out_linesize % 32;
    if (align_remainder != 0)
        out_linesize += 32 - align_remainder;

    return true;
}

bool VideoStreamContext::rebuild_scaler_from_frame(
    const AVFrame* frame,
    uint32_t tw, uint32_t th, int tf)
{
    if (!frame || frame->width <= 0 || frame->height <= 0
        || frame->format == AV_PIX_FMT_NONE) {
        m_last_error = "rebuild_scaler_from_frame: invalid frame";
        return false;
    }

    if (codec_context && codec_context->pix_fmt == AV_PIX_FMT_NONE)
        codec_context->pix_fmt = static_cast<AVPixelFormat>(frame->format);

    if (width == 0 || height == 0) {
        width = static_cast<uint32_t>(frame->width);
        height = static_cast<uint32_t>(frame->height);
    }
    src_pixel_format = codec_context ? codec_context->pix_fmt : frame->format;

    if (sws_context) {
        sws_freeContext(sws_context);
        sws_context = nullptr;
    }

    const uint32_t use_w = tw > 0 ? tw : (target_width > 0 ? target_width : width);
    const uint32_t use_h = th > 0 ? th : (target_height > 0 ? target_height : height);
    const int use_f = tf >= 0 ? tf : (target_format >= 0 ? target_format : -1);

    return setup_scaler(use_w, use_h, use_f);
}

// =========================================================================
// Codec flush
// =========================================================================

void VideoStreamContext::flush_codec()
{
    if (codec_context)
        avcodec_flush_buffers(codec_context);
}

// =========================================================================
// Metadata
// =========================================================================

void VideoStreamContext::extract_stream_metadata(const FFmpegDemuxContext& demux,
    FileMetadata& out) const
{
    if (!codec_context || stream_index < 0)
        return;

    out.attributes["video_codec"] = std::string(avcodec_get_name(codec_context->codec_id));
    if (codec_context->codec && codec_context->codec->long_name)
        out.attributes["video_codec_long_name"] = std::string(codec_context->codec->long_name);
    out.attributes["video_width"] = width;
    out.attributes["video_height"] = height;
    out.attributes["video_frame_rate"] = frame_rate;
    out.attributes["video_total_frames"] = total_frames;
    out.attributes["video_bit_rate"] = codec_context->bit_rate;

    const char* pix_fmt_name = av_get_pix_fmt_name(codec_context->pix_fmt);
    if (pix_fmt_name)
        out.attributes["video_pixel_format"] = std::string(pix_fmt_name);

    if (codec_context->color_range != AVCOL_RANGE_UNSPECIFIED)
        out.attributes["video_color_range"] = static_cast<int>(codec_context->color_range);
    if (codec_context->colorspace != AVCOL_SPC_UNSPECIFIED)
        out.attributes["video_colorspace"] = static_cast<int>(codec_context->colorspace);
    if (codec_context->color_trc != AVCOL_TRC_UNSPECIFIED)
        out.attributes["video_color_trc"] = static_cast<int>(codec_context->color_trc);
    if (codec_context->color_primaries != AVCOL_PRI_UNSPECIFIED)
        out.attributes["video_color_primaries"] = static_cast<int>(codec_context->color_primaries);

    AVStream* stream = demux.get_stream(stream_index);
    if (!stream)
        return;

    if (stream->sample_aspect_ratio.num > 0 && stream->sample_aspect_ratio.den > 0) {
        out.attributes["video_sar_num"] = stream->sample_aspect_ratio.num;
        out.attributes["video_sar_den"] = stream->sample_aspect_ratio.den;
    }

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        out.attributes[std::string("video_stream_") + tag->key] = std::string(tag->value);
}

std::vector<FileRegion> VideoStreamContext::extract_keyframe_regions(
    const FFmpegDemuxContext& demux) const
{
    std::vector<FileRegion> regions;
    if (stream_index < 0 || !codec_context)
        return regions;

    AVStream* stream = demux.get_stream(stream_index);
    if (!stream)
        return regions;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
        return regions;

    int idx = 0;
    while (av_read_frame(demux.format_context, pkt) >= 0) {
        if (pkt->stream_index == stream_index && (pkt->flags & AV_PKT_FLAG_KEY)) {
            FileRegion r;
            r.type = "keyframe";
            r.name = "keyframe_" + std::to_string(idx);

            int64_t pts = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;
            double ts = 0.0;
            if (pts != AV_NOPTS_VALUE && stream->time_base.num > 0 && stream->time_base.den > 0)
                ts = static_cast<double>(pts) * av_q2d(stream->time_base);

            uint64_t frame_pos = 0;
            if (frame_rate > 0.0)
                frame_pos = static_cast<uint64_t>(ts * frame_rate);

            r.start_coordinates = { frame_pos };
            r.end_coordinates = { frame_pos };
            r.attributes["pts"] = pts;
            r.attributes["timestamp_seconds"] = ts;
            r.attributes["keyframe_index"] = idx;

            regions.push_back(std::move(r));
            ++idx;
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);

    av_seek_frame(demux.format_context, stream_index, 0, AVSEEK_FLAG_BACKWARD);

    return regions;
}

} // namespace MayaFlux::IO
