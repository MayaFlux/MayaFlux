#include "FFmpegMuxContext.hpp"
#include "FFmpegDemuxContext.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

namespace MayaFlux::IO {

// =========================================================================
// Destructor
// =========================================================================

FFmpegMuxContext::~FFmpegMuxContext()
{
    close();
}

// =========================================================================
// Lifecycle
// =========================================================================

bool FFmpegMuxContext::open(const std::string& filepath,
    const std::string& explicit_format)
{
    close();
    FFmpegDemuxContext::init_ffmpeg();

    const char* fmt = explicit_format.empty() ? nullptr : explicit_format.c_str();

    int ret = avformat_alloc_output_context2(
        &format_context, nullptr, fmt, filepath.c_str());

    if (ret < 0 || !format_context) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        m_last_error = "avformat_alloc_output_context2 failed for \""
            + filepath + "\": " + errbuf;
        return false;
    }

    if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_context->pb, filepath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            m_last_error = "avio_open failed for \""
                + filepath + "\": " + errbuf;
            avformat_free_context(format_context);
            format_context = nullptr;
            return false;
        }
    }

    return true;
}

void FFmpegMuxContext::close()
{
    if (!format_context)
        return;

    if (m_header_written) {
        av_write_trailer(format_context);
        m_header_written = false;
    }

    if (format_context->pb && !(format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&format_context->pb);

    avformat_free_context(format_context);
    format_context = nullptr;
    m_last_error.clear();
}

// =========================================================================
// Stream registration
// =========================================================================

AVStream* FFmpegMuxContext::add_stream()
{
    if (!format_context) {
        m_last_error = "add_stream called on unopened context";
        return nullptr;
    }
    AVStream* st = avformat_new_stream(format_context, nullptr);
    if (!st)
        m_last_error = "avformat_new_stream failed";
    return st;
}

// =========================================================================
// Header / packet writing
// =========================================================================

bool FFmpegMuxContext::write_header()
{
    if (!format_context) {
        m_last_error = "write_header called on unopened context";
        return false;
    }
    if (m_header_written) {
        m_last_error = "write_header called more than once";
        return false;
    }

    int ret = avformat_write_header(format_context, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        m_last_error = std::string("avformat_write_header failed: ") + errbuf;
        return false;
    }

    m_header_written = true;
    return true;
}

bool FFmpegMuxContext::write_packet(AVPacket* pkt)
{
    if (!format_context || !m_header_written) {
        m_last_error = "write_packet called before header was written";
        return false;
    }

    int ret = av_interleaved_write_frame(format_context, pkt);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        m_last_error = std::string("av_interleaved_write_frame failed: ") + errbuf;
        return false;
    }

    return true;
}

} // namespace MayaFlux::IO
