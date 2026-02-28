#include "VideoFileReader.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <cstddef>
}

namespace MayaFlux::IO {

// =========================================================================
// Construction / destruction
// =========================================================================

VideoFileReader::VideoFileReader() = default;

VideoFileReader::~VideoFileReader()
{
    close();
}

// =========================================================================
// open / close / is_open
// =========================================================================

bool VideoFileReader::can_read(const std::string& filepath) const
{
    static const std::vector<std::string> exts = {
        "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v", "ts", "mts"
    };
    auto dot = filepath.rfind('.');
    if (dot == std::string::npos)
        return false;
    std::string ext = filepath.substr(dot + 1);
    std::ranges::transform(ext, ext.begin(), ::tolower);
    return std::ranges::find(exts, ext) != exts.end();
}

bool VideoFileReader::open(const std::string& filepath, FileReadOptions options)
{
    close();

    m_filepath = filepath;
    m_options = options;

    auto demux = std::make_shared<FFmpegDemuxContext>();
    if (!demux->open(filepath)) {
        set_error(demux->last_error());
        return false;
    }

    auto video = std::make_shared<VideoStreamContext>();
    if (!video->open(*demux, m_target_width, m_target_height)) {
        set_error(video->last_error());
        return false;
    }

    std::shared_ptr<AudioStreamContext> audio;
    if ((m_video_options & VideoReadOptions::EXTRACT_AUDIO) != VideoReadOptions::NONE) {
        audio = std::make_shared<AudioStreamContext>();
        if (!audio->open(*demux, false, m_target_sample_rate)) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VideoFileReader: no audio stream found or audio open failed");
            audio.reset();
        }
    }

    {
        std::unique_lock lock(m_context_mutex);
        m_demux = std::move(demux);
        m_video = std::move(video);
        m_audio = std::move(audio);
    }

    if ((m_options & FileReadOptions::EXTRACT_METADATA) != FileReadOptions::NONE)
        build_metadata(m_demux, m_video);
    if ((m_options & FileReadOptions::EXTRACT_REGIONS) != FileReadOptions::NONE)
        build_regions(m_demux, m_video);

    return true;
}

void VideoFileReader::close()
{
    stop_decode_thread();
    m_container_ref.reset();

    std::unique_lock ctx_lock(m_context_mutex);

    if (m_audio) {
        m_audio->close();
        m_audio.reset();
    }
    if (m_video) {
        m_video->close();
        m_video.reset();
    }
    if (m_demux) {
        m_demux->close();
        m_demux.reset();
    }

    m_audio_container.reset();
    m_sws_buf.clear();
    m_sws_buf.shrink_to_fit();

    {
        std::lock_guard lock(m_metadata_mutex);
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }

    m_decode_head.store(0);
    clear_error();
}

bool VideoFileReader::is_open() const
{
    std::shared_lock lock(m_context_mutex);
    return m_demux && m_video && m_video->is_valid();
}

// =========================================================================
// Metadata / regions
// =========================================================================

std::optional<FileMetadata> VideoFileReader::get_metadata() const
{
    std::lock_guard lock(m_metadata_mutex);
    return m_cached_metadata;
}

std::vector<FileRegion> VideoFileReader::get_regions() const
{
    std::lock_guard lock(m_metadata_mutex);
    return m_cached_regions;
}

void VideoFileReader::build_metadata(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& video) const
{
    FileMetadata meta;
    meta.mime_type = "video";
    demux->extract_container_metadata(meta);
    video->extract_stream_metadata(*demux, meta);

    std::lock_guard lock(m_metadata_mutex);
    m_cached_metadata = std::move(meta);
}

void VideoFileReader::build_regions(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<VideoStreamContext>& video) const
{
    std::vector<FileRegion> regions;

    auto chapters = demux->extract_chapter_regions();
    regions.insert(regions.end(),
        std::make_move_iterator(chapters.begin()),
        std::make_move_iterator(chapters.end()));

    auto keyframes = video->extract_keyframe_regions(*demux);
    regions.insert(regions.end(),
        std::make_move_iterator(keyframes.begin()),
        std::make_move_iterator(keyframes.end()));

    std::lock_guard lock(m_metadata_mutex);
    m_cached_regions = std::move(regions);
}

// =========================================================================
// FileReader interface — read_all / read_region (not supported in streaming mode)
// =========================================================================

std::vector<Kakshya::DataVariant> VideoFileReader::read_all()
{
    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
        "VideoFileReader::read_all() is not supported; "
        "use create_container() + load_into_container()");
    return {};
}

std::vector<Kakshya::DataVariant> VideoFileReader::read_region(const FileRegion& /*region*/)
{
    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
        "VideoFileReader::read_region() is not supported; "
        "use the container API to access regions");
    return {};
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

    const uint64_t total = video->total_frames;
    if (total == 0) {
        set_error("Video stream reports 0 frames");
        return false;
    }

    // -----------------------------------------------------------------
    // Ring buffer allocation
    // -----------------------------------------------------------------

    const uint32_t ring_cap = std::min(
        m_ring_capacity,
        static_cast<uint32_t>(total));

    vc->setup_ring(total,
        ring_cap,
        video->out_width,
        video->out_height,
        video->out_bytes_per_pixel,
        video->frame_rate);

    m_sws_buf.resize(
        static_cast<size_t>(video->out_linesize) * video->out_height);

    // -----------------------------------------------------------------
    // Audio extraction FIRST — before video decode touches the demuxer
    // -----------------------------------------------------------------

    bool want_audio = (m_video_options & VideoReadOptions::EXTRACT_AUDIO)
        != VideoReadOptions::NONE;

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
                    "VideoFileReader: audio load failed: {}",
                    audio_reader.get_last_error());
            }
        } else {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VideoFileReader: open_from_demux failed: {}",
                audio_reader.get_last_error());
        }

        {
            std::unique_lock lock(m_context_mutex);
            demux->seek(video->stream_index, 0);
            video->flush_codec();
        }
    }

    // -----------------------------------------------------------------
    // Synchronous preload: decode first batch into the ring
    // -----------------------------------------------------------------

    m_decode_head.store(0);
    m_container_ref = vc;

    const uint64_t preload = std::min(
        static_cast<uint64_t>(m_decode_batch_size),
        total);

    uint64_t decoded = decode_batch(*vc, preload);

    if (decoded == 0) {
        set_error("Failed to decode any frames during preload");
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VideoFileReader: preloaded {}/{} frames ({}x{}, {:.1f} fps, ring={})",
        decoded, total,
        video->out_width, video->out_height,
        video->frame_rate, ring_cap);

    // -----------------------------------------------------------------
    // Regions and processor
    // -----------------------------------------------------------------

    auto regions = get_regions();
    auto region_groups = regions_to_groups(regions);
    for (const auto& [name, group] : region_groups)
        vc->add_region_group(group);

    vc->create_default_processor();
    vc->mark_ready_for_processing(true);

    // -----------------------------------------------------------------
    // Start background decode thread
    // -----------------------------------------------------------------

    if (decoded < total)
        start_decode_thread();

    return true;
}

// =========================================================================
// Decode: batch (multiple frames per wake cycle)
// =========================================================================

uint64_t VideoFileReader::decode_batch(
    Kakshya::VideoFileContainer& vc, uint64_t batch_size)
{
    std::shared_lock ctx_lock(m_context_mutex);
    if (!m_demux || !m_video || !m_video->is_valid())
        return 0;

    const size_t frame_bytes = vc.get_frame_byte_size();
    const int packed_stride = static_cast<int>(
        m_video->out_width * m_video->out_bytes_per_pixel);

    uint64_t decoded = 0;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        return 0;
    }

    uint8_t* sws_dst[1] = { m_sws_buf.data() };
    int sws_stride[1] = { m_video->out_linesize };

    auto write_frame_to_ring = [&]() -> bool {
        uint64_t idx = m_decode_head.load();
        if (idx >= vc.get_total_source_frames())
            return false;

        uint8_t* dest = vc.mutable_slot_ptr(idx);
        if (!dest)
            return false;

        sws_scale(m_video->sws_context,
            frame->data, frame->linesize,
            0, static_cast<int>(m_video->height),
            sws_dst, sws_stride);

        if (m_video->out_linesize == packed_stride) {
            std::memcpy(dest, m_sws_buf.data(), frame_bytes);
        } else {
            for (uint32_t row = 0; row < m_video->out_height; ++row) {
                std::memcpy(
                    dest + static_cast<size_t>(row) * packed_stride,
                    m_sws_buf.data() + static_cast<size_t>(row) * m_video->out_linesize,
                    static_cast<size_t>(packed_stride));
            }
        }

        vc.commit_frame(idx);
        m_decode_head.fetch_add(1);
        ++decoded;

        av_frame_unref(frame);
        return true;
    };

    while (decoded < batch_size) {
        int ret = av_read_frame(m_demux->format_context, pkt);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                avcodec_send_packet(m_video->codec_context, nullptr);
            } else {
                break;
            }
        } else if (pkt->stream_index != m_video->stream_index) {
            av_packet_unref(pkt);
            continue;
        } else {
            ret = avcodec_send_packet(m_video->codec_context, pkt);
            av_packet_unref(pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;
        }

        while (decoded < batch_size) {
            ret = avcodec_receive_frame(m_video->codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            if (!write_frame_to_ring())
                goto done;
        }

        if (ret == AVERROR_EOF)
            break;
    }

done:
    av_packet_free(&pkt);
    av_frame_free(&frame);
    return decoded;
}

// =========================================================================
// Background decode thread
// =========================================================================

void VideoFileReader::start_decode_thread()
{
    stop_decode_thread();

    m_decode_stop.store(false);
    m_decode_active.store(true);
    m_decode_thread = std::thread(&VideoFileReader::decode_thread_func, this);
}

void VideoFileReader::stop_decode_thread()
{
    if (!m_decode_active.load())
        return;

    m_decode_stop.store(true);
    m_decode_cv.notify_all();

    if (m_decode_thread.joinable())
        m_decode_thread.join();

    m_decode_active.store(false);
}

void VideoFileReader::decode_thread_func()
{
    auto vc = m_container_ref.lock();
    if (!vc) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "VideoFileReader: decode thread — container expired");
        m_decode_active.store(false);
        return;
    }

    const uint64_t total = vc->get_total_source_frames();
    const uint32_t ring_cap = vc->get_ring_capacity();
    const uint32_t threshold = (m_refill_threshold > 0)
        ? m_refill_threshold
        : ring_cap / 4;

    while (!m_decode_stop.load()) {
        uint64_t head = m_decode_head.load();

        if (head >= total)
            break;

        uint64_t read_pos = vc->get_read_position()[0];
        uint64_t buffered_ahead = (head > read_pos) ? (head - read_pos) : 0;

        if (buffered_ahead >= ring_cap) {
            std::unique_lock lock(m_decode_mutex);
            m_decode_cv.wait_for(lock, std::chrono::milliseconds(50), [&] {
                if (m_decode_stop.load())
                    return true;

                uint64_t rp = vc->get_read_position()[0];
                uint64_t h = m_decode_head.load();
                uint64_t ahead = (h > rp) ? (h - rp) : 0;
                return ahead < (ring_cap - threshold);
            });
            continue;
        }

        uint64_t batch = std::min(
            static_cast<uint64_t>(m_decode_batch_size),
            total - head);

        uint64_t decoded = decode_batch(*vc, batch);

        if (decoded == 0)
            break;
    }

    m_decode_active.store(false);
}

// =========================================================================
// Seeking
// =========================================================================

std::vector<uint64_t> VideoFileReader::get_read_position() const
{
    return { m_decode_head.load() };
}

bool VideoFileReader::seek(const std::vector<uint64_t>& position)
{
    if (position.empty())
        return false;

    const uint64_t target_frame = position[0];

    stop_decode_thread();

    std::shared_ptr<VideoStreamContext> video;
    std::shared_ptr<FFmpegDemuxContext> demux;
    {
        std::shared_lock lock(m_context_mutex);
        if (!m_demux || !m_video || !m_video->is_valid()) {
            set_error("Cannot seek: reader not open");
            return false;
        }
        video = m_video;
        demux = m_demux;
    }

    if (!seek_internal(demux, video, target_frame))
        return false;

    m_decode_head.store(target_frame);

    auto vc = m_container_ref.lock();
    if (!vc)
        return true;

    vc->invalidate_ring();
    vc->set_read_position({ target_frame });

    const uint64_t total = vc->get_total_source_frames();
    const uint64_t batch = std::min(
        static_cast<uint64_t>(m_decode_batch_size),
        total > target_frame ? total - target_frame : 0UL);

    decode_batch(*vc, batch);

    if (m_decode_head.load() < total)
        start_decode_thread();

    return true;
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
    return true;
}

// =========================================================================
// Dimension queries
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
    return {
        m_video->total_frames,
        m_video->out_height,
        m_video->out_width,
        m_video->out_bytes_per_pixel
    };
}

std::vector<std::string> VideoFileReader::get_supported_extensions() const
{
    return { "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v", "ts", "mts" };
}

// =========================================================================
// Error
// =========================================================================

std::string VideoFileReader::get_last_error() const
{
    std::lock_guard lock(m_error_mutex);
    return m_last_error;
}

void VideoFileReader::set_error(const std::string& msg) const
{
    std::lock_guard lock(m_error_mutex);
    m_last_error = msg;
    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
        "VideoFileReader: {}", msg);
}

void VideoFileReader::clear_error() const
{
    std::lock_guard lock(m_error_mutex);
    m_last_error.clear();
}

} // namespace MayaFlux::IO
