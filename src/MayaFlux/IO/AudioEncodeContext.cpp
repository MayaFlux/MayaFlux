#include "AudioEncodeContext.hpp"
#include "FFmpegDemuxContext.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace MayaFlux::IO {

// =========================================================================
// Destructor
// =========================================================================

AudioEncodeContext::~AudioEncodeContext()
{
    close();
}

void AudioEncodeContext::close()
{
    if (fifo) {
        av_audio_fifo_free(fifo);
        fifo = nullptr;
    }
    if (swr_context) {
        swr_free(&swr_context);
        swr_context = nullptr;
    }
    if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
    }
    stream = nullptr;
    m_stream_index = -1;
    m_sample_rate = 0;
    m_channels = 0;
    m_pts = 0;
    m_last_error.clear();
}

// =========================================================================
// Open
// =========================================================================

bool AudioEncodeContext::open(FFmpegMuxContext& mux,
    uint32_t sample_rate,
    uint32_t channels,
    AVCodecID codec_id)
{
    close();
    FFmpegDemuxContext::init_ffmpeg();

    if (!mux.is_open()) {
        m_last_error = "AudioEncodeContext::open called on unopened mux";
        return false;
    }

    if (codec_id == AV_CODEC_ID_NONE)
        codec_id = mux.format_context->oformat->audio_codec;

    if (codec_id == AV_CODEC_ID_NONE) {
        m_last_error = "container has no default audio codec";
        return false;
    }

    const AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        m_last_error = std::string("avcodec_find_encoder failed for codec ")
            + avcodec_get_name(codec_id);
        return false;
    }

    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        m_last_error = "avcodec_alloc_context3 failed";
        return false;
    }

    AVSampleFormat enc_fmt = AV_SAMPLE_FMT_FLTP;
    {
        const AVSampleFormat* fmts = nullptr;
        int n_fmts = 0;
        if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT,
                0, reinterpret_cast<const void**>(&fmts), &n_fmts)
                >= 0
            && fmts && n_fmts > 0) {
            enc_fmt = fmts[0];
            for (int i = 0; i < n_fmts; ++i) {
                if (fmts[i] == AV_SAMPLE_FMT_S16) {
                    enc_fmt = AV_SAMPLE_FMT_S16;
                    break;
                }
                if (fmts[i] == AV_SAMPLE_FMT_FLTP) {
                    enc_fmt = AV_SAMPLE_FMT_FLTP;
                    break;
                }
            }
        }
    }

    uint32_t enc_rate = sample_rate;
    {
        const int* rates = nullptr;
        int n_rates = 0;
        if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_RATE,
                0, reinterpret_cast<const void**>(&rates), &n_rates)
                >= 0
            && rates && n_rates > 0) {
            bool found = false;
            for (int i = 0; i < n_rates; ++i) {
                if (static_cast<uint32_t>(rates[i]) == sample_rate) {
                    found = true;
                    break;
                }
            }
            if (!found)
                enc_rate = static_cast<uint32_t>(rates[0]);
        }
    }

    av_channel_layout_default(&codec_context->ch_layout,
        static_cast<int>(channels));
    codec_context->sample_rate = static_cast<int>(enc_rate);
    codec_context->sample_fmt = enc_fmt;
    codec_context->time_base = { .num = 1, .den = static_cast<int>(enc_rate) };

    if (mux.format_context->oformat->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        m_last_error = "avcodec_open2 failed";
        close();
        return false;
    }

    stream = mux.add_stream();
    if (!stream) {
        m_last_error = mux.last_error();
        close();
        return false;
    }

    if (avcodec_parameters_from_context(stream->codecpar, codec_context) < 0) {
        m_last_error = "avcodec_parameters_from_context failed";
        close();
        return false;
    }

    stream->time_base = codec_context->time_base;
    m_stream_index = stream->index;
    m_sample_rate = enc_rate;
    m_channels = channels;

    if (!setup_resampler() || !setup_fifo()) {
        close();
        return false;
    }

    return true;
}

// =========================================================================
// Resampler: AV_SAMPLE_FMT_DBL (interleaved) → encoder native format
// =========================================================================

bool AudioEncodeContext::setup_resampler()
{
    AVChannelLayout layout {};
    av_channel_layout_default(&layout, static_cast<int>(m_channels));

    int ret = swr_alloc_set_opts2(
        &swr_context,
        &layout, codec_context->sample_fmt, codec_context->sample_rate,
        &layout, AV_SAMPLE_FMT_DBL, static_cast<int>(m_sample_rate),
        0, nullptr);

    av_channel_layout_uninit(&layout);

    if (ret < 0 || !swr_context) {
        m_last_error = "swr_alloc_set_opts2 failed";
        return false;
    }

    if (swr_init(swr_context) < 0) {
        m_last_error = "swr_init failed";
        swr_free(&swr_context);
        swr_context = nullptr;
        return false;
    }

    return true;
}

// =========================================================================
// FIFO: sized to two encoder frames so one encode_frames() call never stalls
// =========================================================================

bool AudioEncodeContext::setup_fifo()
{
    int frame_size = codec_context->frame_size > 0
        ? codec_context->frame_size
        : 4096;

    fifo = av_audio_fifo_alloc(
        codec_context->sample_fmt,
        static_cast<int>(m_channels),
        frame_size * 2);

    if (!fifo) {
        m_last_error = "av_audio_fifo_alloc failed";
        return false;
    }

    return true;
}

// =========================================================================
// Encoding
// =========================================================================

bool AudioEncodeContext::encode_frames(std::span<const double> interleaved,
    uint32_t num_frames,
    FFmpegMuxContext& mux)
{
    if (!is_valid())
        return false;

    uint8_t** conv = nullptr;
    int linesize = 0;
    int alloc = av_samples_alloc_array_and_samples(
        &conv, &linesize,
        static_cast<int>(m_channels),
        static_cast<int>(num_frames),
        codec_context->sample_fmt, 0);

    if (alloc < 0 || !conv) {
        m_last_error = "av_samples_alloc_array_and_samples failed during encode";
        return false;
    }

    const auto* src = reinterpret_cast<const uint8_t*>(interleaved.data());
    int converted = swr_convert(
        swr_context,
        conv, static_cast<int>(num_frames),
        &src, static_cast<int>(num_frames));

    if (converted < 0) {
        av_freep(static_cast<void*>(&conv[0]));
        av_freep(static_cast<void*>(&conv));
        m_last_error = "swr_convert failed";
        return false;
    }

    int written = av_audio_fifo_write(fifo,
        reinterpret_cast<void**>(conv), converted);

    av_freep(static_cast<void*>(&conv[0]));
    av_freep(static_cast<void*>(&conv));

    if (written < converted) {
        m_last_error = "av_audio_fifo_write wrote fewer samples than expected";
        return false;
    }

    int frame_size = codec_context->frame_size > 0
        ? codec_context->frame_size
        : av_audio_fifo_size(fifo);

    while (av_audio_fifo_size(fifo) >= frame_size) {
        if (!send_fifo_frame(mux, false))
            return false;
    }

    return true;
}

// =========================================================================
// Drain
// =========================================================================

bool AudioEncodeContext::drain(FFmpegMuxContext& mux)
{
    if (!is_valid())
        return false;

    while (av_audio_fifo_size(fifo) > 0) {
        if (!send_fifo_frame(mux, true))
            return false;
    }

    if (avcodec_send_frame(codec_context, nullptr) < 0) {
        m_last_error = "avcodec_send_frame(null) failed during drain";
        return false;
    }

    return drain_packets(mux);
}

// =========================================================================
// Private helpers
// =========================================================================

bool AudioEncodeContext::send_fifo_frame(FFmpegMuxContext& mux, bool pad_to_frame_size)
{
    int frame_size = codec_context->frame_size > 0
        ? codec_context->frame_size
        : av_audio_fifo_size(fifo);

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        m_last_error = "av_frame_alloc failed";
        return false;
    }

    frame->nb_samples = frame_size;
    frame->format = codec_context->sample_fmt;
    frame->sample_rate = codec_context->sample_rate;
    av_channel_layout_copy(&frame->ch_layout, &codec_context->ch_layout);

    if (av_frame_get_buffer(frame, 0) < 0) {
        m_last_error = "av_frame_get_buffer failed";
        av_frame_free(&frame);
        return false;
    }

    if (av_frame_make_writable(frame) < 0) {
        m_last_error = "av_frame_make_writable failed";
        av_frame_free(&frame);
        return false;
    }

    int available = av_audio_fifo_size(fifo);
    int to_read = std::min(available, frame_size);

    int read = av_audio_fifo_read(fifo,
        reinterpret_cast<void**>(frame->data), to_read);

    if (read < to_read) {
        m_last_error = "av_audio_fifo_read returned fewer samples than expected";
        av_frame_free(&frame);
        return false;
    }

    if (pad_to_frame_size && read < frame_size) {
        int pad_samples = frame_size - read;
        int bytes_per_sample = av_get_bytes_per_sample(codec_context->sample_fmt);
        bool is_planar = av_sample_fmt_is_planar(codec_context->sample_fmt);
        int planes = is_planar ? static_cast<int>(m_channels) : 1;
        int samples_per_plane = is_planar ? pad_samples : pad_samples * static_cast<int>(m_channels);

        for (int p = 0; p < planes; ++p) {
            std::memset(
                frame->data[p] + static_cast<ptrdiff_t>(read) * bytes_per_sample * (is_planar ? 1 : static_cast<int>(m_channels)),
                0,
                static_cast<size_t>(samples_per_plane) * static_cast<size_t>(bytes_per_sample));
        }
        frame->nb_samples = frame_size;
    } else {
        frame->nb_samples = read;
    }

    frame->pts = m_pts;
    m_pts += frame->nb_samples;

    int ret = avcodec_send_frame(codec_context, frame);
    av_frame_free(&frame);

    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        m_last_error = std::string("avcodec_send_frame failed: ") + errbuf;
        return false;
    }

    return drain_packets(mux);
}

bool AudioEncodeContext::drain_packets(FFmpegMuxContext& mux)
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
            stream->time_base);

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
