#include "AudioStreamContext.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace MayaFlux::IO {

// =========================================================================
// Destructor
// =========================================================================

AudioStreamContext::~AudioStreamContext()
{
    close();
}

void AudioStreamContext::close()
{
    if (swr_context) {
        swr_free(&swr_context);
        swr_context = nullptr;
    }
    if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
    }
    stream_index = -1;
    total_frames = 0;
    sample_rate = 0;
    channels = 0;
    m_last_error.clear();
}

// =========================================================================
// Open
// =========================================================================

bool AudioStreamContext::open(const FFmpegDemuxContext& demux,
    bool planar_output,
    uint32_t target_rate)
{
    close();
    FFmpegDemuxContext::init_ffmpeg();

    if (!demux.is_open()) {
        m_last_error = "Demux context is not open";
        return false;
    }

    const AVCodec* codec = nullptr;
    stream_index = demux.find_best_stream(AVMEDIA_TYPE_AUDIO,
        reinterpret_cast<const void**>(&codec));
    if (stream_index < 0 || !codec) {
        m_last_error = "No audio stream found";
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

    sample_rate = static_cast<uint32_t>(codec_context->sample_rate);
    channels = static_cast<uint32_t>(codec_context->ch_layout.nb_channels);

    if (stream->duration > 0) {
        total_frames = av_rescale_q(stream->duration, stream->time_base,
            AVRational { 1, (int)sample_rate });
    } else if (demux.format_context->duration != AV_NOPTS_VALUE) {
        double dur = (double)demux.format_context->duration / AV_TIME_BASE;
        total_frames = static_cast<uint64_t>(dur * sample_rate);
    }

    if (!setup_resampler(planar_output, target_rate)) {
        close();
        return false;
    }

    drain_resampler_init();

    return true;
}

// =========================================================================
// Resampler
// =========================================================================

bool AudioStreamContext::setup_resampler(bool planar_output, uint32_t target_rate)
{
    if (!codec_context)
        return false;

    AVChannelLayout out_layout;
    av_channel_layout_copy(&out_layout, &codec_context->ch_layout);

    uint32_t out_rate = target_rate > 0 ? target_rate : sample_rate;
    AVSampleFormat out_fmt = planar_output ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;

    int ret = swr_alloc_set_opts2(
        &swr_context,
        &out_layout, out_fmt, static_cast<int>(out_rate),
        &codec_context->ch_layout, codec_context->sample_fmt, codec_context->sample_rate,
        0, nullptr);

    av_channel_layout_uninit(&out_layout);

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
// Codec flush
// =========================================================================

void AudioStreamContext::flush_codec()
{
    if (codec_context)
        avcodec_flush_buffers(codec_context);
}

void AudioStreamContext::drain_resampler_init()
{
    if (!swr_context || channels == 0)
        return;

    constexpr int k_drain_samples = 2048;
    uint8_t** buf = nullptr;
    int linesize = 0;

    int alloc = av_samples_alloc_array_and_samples(
        &buf, &linesize,
        static_cast<int>(channels), k_drain_samples,
        AV_SAMPLE_FMT_DBL, 0);

    if (alloc < 0 || !buf)
        return;

    while (swr_convert(swr_context, buf, k_drain_samples, nullptr, 0) > 0) { }

    av_freep(&buf[0]);
    av_freep(&buf);
}

// =========================================================================
// Metadata
// =========================================================================

void AudioStreamContext::extract_stream_metadata(const FFmpegDemuxContext& demux,
    FileMetadata& out) const
{
    if (!codec_context || stream_index < 0)
        return;

    out.attributes["codec"] = std::string(avcodec_get_name(codec_context->codec_id));
    out.attributes["codec_long_name"] = std::string(codec_context->codec->long_name);
    out.attributes["total_frames"] = total_frames;
    out.attributes["sample_rate"] = sample_rate;
    out.attributes["channels"] = channels;
    out.attributes["bit_rate"] = codec_context->bit_rate;

    char layout_desc[256] = {};
    av_channel_layout_describe(&codec_context->ch_layout, layout_desc, sizeof(layout_desc));
    out.attributes["channel_layout"] = std::string(layout_desc);

    AVStream* stream = demux.get_stream(stream_index);
    if (!stream)
        return;

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        out.attributes[std::string("stream_") + tag->key] = std::string(tag->value);
}

std::vector<FileRegion> AudioStreamContext::extract_cue_regions(
    const FFmpegDemuxContext& demux) const
{
    std::vector<FileRegion> regions;
    if (stream_index < 0 || sample_rate == 0)
        return regions;

    AVStream* stream = demux.get_stream(stream_index);
    if (!stream)
        return regions;

    AVDictionaryEntry* tag = nullptr;
    int idx = 0;
    while ((tag = av_dict_get(stream->metadata, "cue", tag, AV_DICT_IGNORE_SUFFIX))) {
        FileRegion r;
        r.type = "cue";
        r.name = tag->value;
        r.start_coordinates = { static_cast<uint64_t>(idx) };
        r.end_coordinates = { static_cast<uint64_t>(idx) };
        r.attributes["label"] = std::string(tag->value);
        regions.push_back(std::move(r));
        ++idx;
    }
    return regions;
}

} // namespace MayaFlux::IO
