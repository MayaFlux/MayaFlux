#include "VideoFileReader.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <cstddef>
}

namespace MayaFlux::IO {

// =========================================================================
// Construction / Destruction
// =========================================================================

VideoFileReader::VideoFileReader()
{
    FFmpegDemuxContext::init_ffmpeg();
}

VideoFileReader::~VideoFileReader()
{
    close();
}

// =========================================================================
// File operations
// =========================================================================

bool VideoFileReader::can_read(const std::string& filepath) const
{
    FFmpegDemuxContext probe;
    if (!probe.open(filepath))
        return false;

    const AVCodec* codec = nullptr;
    int idx = probe.find_best_stream(AVMEDIA_TYPE_VIDEO,
        reinterpret_cast<const void**>(&codec));
    return idx >= 0 && codec != nullptr;
}

bool VideoFileReader::open(const std::string& filepath, FileReadOptions options)
{
    std::unique_lock lock(m_context_mutex);

    m_demux.reset();
    m_video.reset();
    m_audio.reset();
    m_audio_container.reset();
    m_current_frame_position = 0;
    {
        std::lock_guard ml(m_metadata_mutex);
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }
    clear_error();

    m_filepath = filepath;
    m_options = options;

    auto demux = std::make_shared<FFmpegDemuxContext>();
    if (!demux->open(filepath)) {
        set_error(demux->last_error());
        return false;
    }

    auto video = std::make_shared<VideoStreamContext>();
    if (!video->open(*demux, m_target_width, m_target_height, m_target_pixel_format)) {
        set_error(video->last_error());
        return false;
    }

    m_demux = std::move(demux);
    m_video = std::move(video);

    bool want_audio = (m_video_options & VideoReadOptions::EXTRACT_AUDIO) != VideoReadOptions::NONE;
    if (want_audio) {
        auto audio = std::make_shared<AudioStreamContext>();
        if (audio->open(*m_demux, false, m_target_sample_rate))
            m_audio = std::move(audio);
    }

    if ((options & FileReadOptions::EXTRACT_METADATA) != FileReadOptions::NONE)
        build_metadata(m_demux, m_video, m_audio);

    if ((options & FileReadOptions::EXTRACT_REGIONS) != FileReadOptions::NONE)
        build_regions(m_demux, m_video, m_audio);

    return true;
}

void VideoFileReader::close()
{
    std::unique_lock lock(m_context_mutex);
    m_audio.reset();
    m_video.reset();
    m_demux.reset();
    m_audio_container.reset();
    m_current_frame_position = 0;
    m_filepath.clear();
    {
        std::lock_guard ml(m_metadata_mutex);
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }
}

bool VideoFileReader::is_open() const
{
    std::shared_lock lock(m_context_mutex);
    return m_demux && m_demux->is_open() && m_video && m_video->is_valid();
}

// =========================================================================
// Metadata and Regions
// =========================================================================

std::optional<FileMetadata> VideoFileReader::get_metadata() const
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_video)
        return std::nullopt;

    {
        std::lock_guard ml(m_metadata_mutex);
        if (m_cached_metadata)
            return m_cached_metadata;
    }

    build_metadata(m_demux, m_video, m_audio);

    std::lock_guard ml(m_metadata_mutex);
    return m_cached_metadata;
}

std::vector<FileRegion> VideoFileReader::get_regions() const
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_video)
        return {};

    {
        std::lock_guard ml(m_metadata_mutex);
        if (!m_cached_regions.empty())
            return m_cached_regions;
    }

    build_regions(m_demux, m_video, m_audio);

    std::lock_guard ml(m_metadata_mutex);
    return m_cached_regions;
}

void VideoFileReader::build_metadata(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& video,
    const std::shared_ptr<AudioStreamContext>& audio) const
{
    FileMetadata meta;
    demux->extract_container_metadata(meta);
    video->extract_stream_metadata(*demux, meta);

    if (audio && audio->is_valid())
        audio->extract_stream_metadata(*demux, meta);

    meta.file_size = std::filesystem::file_size(m_filepath);
    auto ftime = std::filesystem::last_write_time(m_filepath);
    meta.modification_time = std::chrono::system_clock::time_point(
        std::chrono::seconds(
            std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch())));

    std::lock_guard ml(m_metadata_mutex);
    m_cached_metadata = std::move(meta);
}

void VideoFileReader::build_regions(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& /*video*/,
    const std::shared_ptr<AudioStreamContext>& audio) const
{
    auto chapters = demux->extract_chapter_regions();

    std::vector<FileRegion> all;
    all.reserve(chapters.size());
    all.insert(all.end(), chapters.begin(), chapters.end());

    if (audio && audio->is_valid()) {
        auto cues = audio->extract_cue_regions(*demux);
        all.insert(all.end(), cues.begin(), cues.end());
    }

    std::lock_guard ml(m_metadata_mutex);
    m_cached_regions = std::move(all);
}

// =========================================================================
// Reading
// =========================================================================

std::vector<Kakshya::DataVariant> VideoFileReader::read_all()
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_video) {
        set_error("File not open");
        return {};
    }
    return decode_video_frames(m_demux, m_video, m_video->total_frames);
}

std::vector<Kakshya::DataVariant> VideoFileReader::read_region(const FileRegion& region)
{
    if (region.start_coordinates.empty() || region.end_coordinates.empty()) {
        set_error("Invalid region coordinates");
        return {};
    }
    uint64_t start = region.start_coordinates[0];
    uint64_t end = region.end_coordinates[0];
    uint64_t n = (end > start) ? (end - start) : 1;
    return read_frames(n, start);
}

std::vector<Kakshya::DataVariant> VideoFileReader::read_frames(uint64_t num_frames,
    uint64_t offset)
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_video) {
        set_error("File not open");
        return {};
    }

    if (offset != m_current_frame_position.load()) {
        lock.unlock();
        std::unique_lock wlock(m_context_mutex);
        if (!m_demux || !m_video) {
            set_error("File closed during operation");
            return {};
        }
        if (!seek_internal(m_demux, m_video, offset))
            return {};
        wlock.unlock();
        lock.lock();
        if (!m_demux || !m_video) {
            set_error("File closed during operation");
            return {};
        }
    }

    return decode_video_frames(m_demux, m_video, num_frames);
}

// =========================================================================
// Container operations
// =========================================================================

std::shared_ptr<Kakshya::SignalSourceContainer> VideoFileReader::create_container()
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_video) {
        set_error("File not open");
        return nullptr;
    }
    return std::make_shared<Kakshya::VideoFileContainer>();
}

bool VideoFileReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container) {
        set_error("Invalid container");
        return false;
    }

    auto vc = std::dynamic_pointer_cast<Kakshya::VideoFileContainer>(container);
    if (!vc) {
        set_error("Container is not a VideoFileContainer");
        return false;
    }

    std::shared_ptr<VideoStreamContext> video;
    std::shared_ptr<AudioStreamContext> audio;
    std::shared_ptr<FFmpegDemuxContext> demux;
    {
        std::shared_lock lock(m_context_mutex);
        if (!m_demux || !m_video) {
            set_error("File not open");
            return false;
        }
        video = m_video;
        audio = m_audio;
        demux = m_demux;
    }

    vc->setup(video->total_frames,
        video->out_width,
        video->out_height,
        video->out_bytes_per_pixel,
        video->frame_rate);

    auto data = read_all();
    if (data.empty()) {
        set_error("Failed to decode video data");
        return false;
    }

    vc->set_raw_data(data);

    auto regions = get_regions();
    auto region_groups = regions_to_groups(regions);
    for (const auto& [name, group] : region_groups)
        vc->add_region_group(group);

    vc->create_default_processor();
    vc->mark_ready_for_processing(true);

    bool want_audio = (m_video_options & VideoReadOptions::EXTRACT_AUDIO) != VideoReadOptions::NONE;
    if (want_audio && audio && audio->is_valid()) {
        {
            std::unique_lock lock(m_context_mutex);
            demux->seek(audio->stream_index, 0);
            audio->flush_codec();
            audio->drain_resampler_init();
        }

        SoundFileReader audio_reader;
        audio_reader.set_audio_options(m_audio_options);

        if (audio_reader.open_from_demux(demux, audio, m_filepath, m_options)) {
            auto sc = audio_reader.create_container();
            if (audio_reader.load_into_container(sc)) {
                m_audio_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(sc);
            } else {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "VideoFileReader: audio open succeeded but load failed: {}",
                    audio_reader.get_last_error());
            }
        } else {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VideoFileReader: open_from_demux failed: {}",
                audio_reader.get_last_error());
        }
    }

    return true;
}

// =========================================================================
// Seeking
// =========================================================================

std::vector<uint64_t> VideoFileReader::get_read_position() const
{
    return { m_current_frame_position.load() };
}

bool VideoFileReader::seek(const std::vector<uint64_t>& position)
{
    if (position.empty()) {
        set_error("Empty position vector");
        return false;
    }
    std::unique_lock lock(m_context_mutex);
    if (!m_demux || !m_video) {
        set_error("File not open");
        return false;
    }
    return seek_internal(m_demux, m_video, position[0]);
}

bool VideoFileReader::seek_internal(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& video,
    uint64_t frame_position)
{
    if (frame_position > video->total_frames)
        frame_position = video->total_frames;

    if (video->frame_rate <= 0.0) {
        set_error("Invalid frame rate for seeking");
        return false;
    }

    AVStream* stream = demux->get_stream(video->stream_index);
    if (!stream) {
        set_error("Invalid stream index");
        return false;
    }

    double target_seconds = static_cast<double>(frame_position) / video->frame_rate;
    auto ts = static_cast<int64_t>(target_seconds / av_q2d(stream->time_base));

    if (!demux->seek(video->stream_index, ts)) {
        set_error(demux->last_error());
        return false;
    }

    video->flush_codec();
    m_current_frame_position = frame_position;
    return true;
}

// =========================================================================
// Video decode loop
// =========================================================================

std::vector<Kakshya::DataVariant> VideoFileReader::decode_video_frames(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& video,
    uint64_t num_frames)
{
    if (!video->is_valid()) {
        set_error("Invalid video context for decoding");
        return {};
    }

    const size_t frame_bytes = static_cast<size_t>(video->out_linesize) * video->out_height;

    std::vector<uint8_t> pixels;
    pixels.reserve(num_frames * frame_bytes);

    uint64_t decoded = 0;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        set_error("Failed to allocate packet/frame");
        return {};
    }

    std::vector<uint8_t> row_buf(static_cast<size_t>(video->out_linesize) * video->out_height);
    uint8_t* dst_data[1] = { row_buf.data() };
    int dst_linesize[1] = { video->out_linesize };

    while (decoded < num_frames) {
        int ret = av_read_frame(demux->format_context, pkt);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                avcodec_send_packet(video->codec_context, nullptr);
            } else {
                break;
            }
        } else if (pkt->stream_index != video->stream_index) {
            av_packet_unref(pkt);
            continue;
        } else {
            ret = avcodec_send_packet(video->codec_context, pkt);
            av_packet_unref(pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;
        }

        while (decoded < num_frames) {
            ret = avcodec_receive_frame(video->codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            sws_scale(video->sws_context,
                frame->data, frame->linesize,
                0, static_cast<int>(video->height),
                dst_data, dst_linesize);

            pixels.insert(pixels.end(), row_buf.begin(), row_buf.end());
            ++decoded;

            av_frame_unref(frame);
        }

        if (ret == AVERROR_EOF)
            break;
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);

    m_current_frame_position += decoded;

    if (pixels.empty())
        return {};

    std::vector<Kakshya::DataVariant> output(1);
    output[0] = std::move(pixels);
    return output;
}

// =========================================================================
// Audio decode loop (for embedded audio extraction)
// =========================================================================

std::vector<Kakshya::DataVariant> VideoFileReader::decode_audio_frames(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<AudioStreamContext>& audio)
{
    if (!audio->is_valid()) {
        set_error("Invalid audio context for decoding");
        return {};
    }

    int ch = static_cast<int>(audio->channels);

    std::vector<Kakshya::DataVariant> output(1);
    output[0] = std::vector<double>();
    auto& dst = std::get<std::vector<double>>(output[0]);
    dst.reserve(audio->total_frames * static_cast<size_t>(ch));

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        set_error("Failed to allocate packet/frame for audio");
        return {};
    }

    int max_resampled = static_cast<int>(audio->total_frames + 8192);

    uint8_t** resample_buf = nullptr;
    int linesize = 0;
    if (av_samples_alloc_array_and_samples(
            &resample_buf, &linesize, ch, max_resampled, AV_SAMPLE_FMT_DBL, 0)
        < 0) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        set_error("Failed to allocate resample buffer");
        return {};
    }

    while (true) {
        int ret = av_read_frame(demux->format_context, pkt);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                avcodec_send_packet(audio->codec_context, nullptr);
            } else {
                break;
            }
        } else if (pkt->stream_index != audio->stream_index) {
            av_packet_unref(pkt);
            continue;
        } else {
            ret = avcodec_send_packet(audio->codec_context, pkt);
            av_packet_unref(pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;
        }

        while (true) {
            ret = avcodec_receive_frame(audio->codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            int out_samples = swr_convert(
                audio->swr_context,
                resample_buf, max_resampled,
                const_cast<const uint8_t**>(frame->data),
                frame->nb_samples);

            if (out_samples > 0) {
                auto* src = reinterpret_cast<double*>(resample_buf[0]);
                dst.insert(dst.end(), src, src + static_cast<ptrdiff_t>(out_samples * ch));
            }

            av_frame_unref(frame);
        }

        if (ret == AVERROR_EOF)
            break;
    }

    while (true) {
        int n = swr_convert(audio->swr_context, resample_buf, max_resampled, nullptr, 0);
        if (n <= 0)
            break;
        auto* src = reinterpret_cast<double*>(resample_buf[0]);
        dst.insert(dst.end(), src, src + static_cast<ptrdiff_t>(n * ch));
    }

    av_freep(&resample_buf[0]);
    av_freep(&resample_buf);
    av_packet_free(&pkt);
    av_frame_free(&frame);

    return output;
}

// =========================================================================
// Utility
// =========================================================================

size_t VideoFileReader::get_num_dimensions() const
{
    return 4;
}

std::vector<uint64_t> VideoFileReader::get_dimension_sizes() const
{
    std::shared_lock lock(m_context_mutex);
    if (!m_video)
        return { 0, 0, 0, 0 };
    return { m_video->total_frames,
        m_video->out_height,
        m_video->out_width,
        m_video->out_bytes_per_pixel };
}

std::vector<std::string> VideoFileReader::get_supported_extensions() const
{
    return {
        "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "mpg", "mpeg",
        "m4v", "3gp", "3g2", "mxf", "ts", "m2ts", "vob", "ogv", "rm",
        "rmvb", "asf", "f4v", "divx", "dv", "mts", "m2v", "y4m"
    };
}

std::string VideoFileReader::get_last_error() const
{
    std::lock_guard lock(m_error_mutex);
    return m_last_error;
}

void VideoFileReader::set_error(const std::string& err) const
{
    std::lock_guard lock(m_error_mutex);
    m_last_error = err;
    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "VideoFileReader: {}", err);
}

void VideoFileReader::clear_error() const
{
    std::lock_guard lock(m_error_mutex);
    m_last_error.clear();
}

} // namespace MayaFlux::IO
