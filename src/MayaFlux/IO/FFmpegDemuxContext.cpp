#include "FFmpegDemuxContext.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace MayaFlux::IO {

// =========================================================================
// Static members
// =========================================================================

std::atomic<bool> FFmpegDemuxContext::s_ffmpeg_initialized { false };
std::mutex FFmpegDemuxContext::s_ffmpeg_init_mutex;

// =========================================================================
// Lifecycle
// =========================================================================

FFmpegDemuxContext::~FFmpegDemuxContext()
{
    close();
}

void FFmpegDemuxContext::init_ffmpeg()
{
    std::lock_guard<std::mutex> lock(s_ffmpeg_init_mutex);
    if (!s_ffmpeg_initialized.exchange(true)) {
        av_log_set_level(AV_LOG_WARNING);
    }
}

bool FFmpegDemuxContext::open(const std::string& filepath)
{
    close();
    init_ffmpeg();

    if (avformat_open_input(&format_context, filepath.c_str(), nullptr, nullptr) < 0) {
        m_last_error = "avformat_open_input failed: " + filepath;
        return false;
    }

    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        avformat_close_input(&format_context);
        m_last_error = "avformat_find_stream_info failed: " + filepath;
        return false;
    }

    return true;
}

void FFmpegDemuxContext::close()
{
    if (format_context) {
        avformat_close_input(&format_context);
        format_context = nullptr;
    }
    m_last_error.clear();
}

// =========================================================================
// Stream discovery
// =========================================================================

int FFmpegDemuxContext::find_best_stream(int media_type, const void** out_codec) const
{
    if (!format_context)
        return -1;

    const AVCodec* codec = nullptr;
    int idx = av_find_best_stream(
        format_context,
        static_cast<AVMediaType>(media_type),
        -1, -1,
        &codec,
        0);

    if (out_codec)
        *out_codec = codec;

    return idx;
}

AVStream* FFmpegDemuxContext::get_stream(int index) const
{
    if (!format_context || index < 0 || static_cast<unsigned>(index) >= format_context->nb_streams)
        return nullptr;
    return format_context->streams[index];
}

unsigned int FFmpegDemuxContext::stream_count() const
{
    return format_context ? format_context->nb_streams : 0U;
}

// =========================================================================
// Seeking
// =========================================================================

bool FFmpegDemuxContext::seek(int stream_index, int64_t timestamp)
{
    if (!format_context)
        return false;

    int ret = av_seek_frame(format_context, stream_index, timestamp, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        m_last_error = "av_seek_frame failed";
        return false;
    }
    return true;
}

void FFmpegDemuxContext::flush()
{
    // No-op at format level; codec flush is the stream context's responsibility.
    // Provided here so callers have a symmetric flush call site.
}

// =========================================================================
// Metadata / regions
// =========================================================================

void FFmpegDemuxContext::extract_container_metadata(FileMetadata& out) const
{
    if (!format_context)
        return;

    out.format = format_context->iformat->name;
    out.mime_type = format_context->iformat->mime_type
        ? format_context->iformat->mime_type
        : "application/" + std::string(format_context->iformat->name);

    if (format_context->duration != AV_NOPTS_VALUE)
        out.attributes["duration_seconds"] = (double)format_context->duration / static_cast<double>(AV_TIME_BASE);

    out.attributes["bit_rate"] = static_cast<int64_t>(format_context->bit_rate);

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(format_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        out.attributes[std::string("tag_") + tag->key] = std::string(tag->value);
}

std::vector<FileRegion> FFmpegDemuxContext::extract_chapter_regions() const
{
    std::vector<FileRegion> regions;
    if (!format_context)
        return regions;

    for (unsigned i = 0; i < format_context->nb_chapters; ++i) {
        const AVChapter* ch = format_context->chapters[i];
        FileRegion r;
        r.type = "chapter";
        r.name = "chapter_" + std::to_string(i);

        AVDictionaryEntry* title = av_dict_get(ch->metadata, "title", nullptr, 0);
        if (title)
            r.name = title->value;

        double tb = av_q2d(ch->time_base);
        r.start_coordinates = { static_cast<uint64_t>(ch->start * tb * 1000) };
        r.end_coordinates = { static_cast<uint64_t>(ch->end * tb * 1000) };
        r.attributes["chapter_index"] = static_cast<int>(i);

        regions.push_back(std::move(r));
    }
    return regions;
}

double FFmpegDemuxContext::duration_seconds() const
{
    if (!format_context || format_context->duration == AV_NOPTS_VALUE)
        return 0.0;
    return static_cast<double>(format_context->duration) / static_cast<double>(AV_TIME_BASE);
}

} // namespace MayaFlux::IO
