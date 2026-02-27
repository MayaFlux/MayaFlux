#include "SoundFileReader.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace MayaFlux::IO {

// ============================================================================
// FileRegion Implementation
// ============================================================================

Kakshya::Region FileRegion::to_region() const
{
    if (start_coordinates.size() == 1 && end_coordinates.size() == 1) {
        if (start_coordinates[0] == end_coordinates[0]) {
            return Kakshya::Region::time_point(start_coordinates[0], name);
        }
        return Kakshya::Region::time_span(start_coordinates[0], end_coordinates[0], name);
    }

    Kakshya::Region region(start_coordinates, end_coordinates);
    region.set_attribute("label", name);
    region.set_attribute("type", type);

    for (const auto& [key, value] : attributes) {
        region.set_attribute(key, value);
    }
    return region;
}

std::unordered_map<std::string, Kakshya::RegionGroup>
FileReader::regions_to_groups(const std::vector<FileRegion>& regions)
{
    std::unordered_map<std::string, Kakshya::RegionGroup> groups;

    for (const auto& region : regions) {
        auto& group = groups[region.type];
        group.name = region.type;
        group.add_region(region.to_region());
    }

    return groups;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

SoundFileReader::SoundFileReader()
{
    FFmpegDemuxContext::init_ffmpeg();
}

SoundFileReader::~SoundFileReader()
{
    close();
}

// ============================================================================
// File Operations
// ============================================================================

bool SoundFileReader::can_read(const std::string& filepath) const
{
    FFmpegDemuxContext probe;
    if (!probe.open(filepath))
        return false;

    const AVCodec* codec = nullptr;
    int idx = probe.find_best_stream(AVMEDIA_TYPE_AUDIO,
        reinterpret_cast<const void**>(&codec));
    return idx >= 0 && codec != nullptr;
}

// =========================================================================
// FileReader — open / close / is_open
// =========================================================================

bool SoundFileReader::open(const std::string& filepath, FileReadOptions options)
{
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);

    m_demux.reset();
    m_audio.reset();
    m_current_frame_position = 0;
    {
        std::lock_guard<std::mutex> ml(m_metadata_mutex);
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

    bool planar = (m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE;
    auto audio = std::make_shared<AudioStreamContext>();
    if (!audio->open(*demux, planar, m_target_sample_rate)) {
        set_error(audio->last_error());
        return false;
    }

    m_demux = std::move(demux);
    m_audio = std::move(audio);

    if ((options & FileReadOptions::EXTRACT_METADATA) != FileReadOptions::NONE)
        build_metadata(m_demux, m_audio);

    if ((options & FileReadOptions::EXTRACT_REGIONS) != FileReadOptions::NONE)
        build_regions(m_demux, m_audio);

    return true;
}

bool SoundFileReader::open_from_demux(
    std::shared_ptr<FFmpegDemuxContext> demux,
    std::shared_ptr<AudioStreamContext> audio,
    const std::string& filepath,
    FileReadOptions options)
{
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);

    m_demux.reset();
    m_audio.reset();
    m_current_frame_position = 0;
    {
        std::lock_guard<std::mutex> ml(m_metadata_mutex);
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }
    clear_error();

    if (!demux || !demux->is_open()) {
        set_error("open_from_demux: demux context is null or not open");
        return false;
    }

    if (!audio || !audio->is_valid()) {
        set_error("open_from_demux: audio stream context is null or not valid");
        return false;
    }

    m_filepath = filepath;
    m_demux = std::move(demux);
    m_audio = std::move(audio);

    if ((options & FileReadOptions::EXTRACT_METADATA) != FileReadOptions::NONE)
        build_metadata(m_demux, m_audio);

    if ((options & FileReadOptions::EXTRACT_REGIONS) != FileReadOptions::NONE)
        build_regions(m_demux, m_audio);

    return true;
}

void SoundFileReader::close()
{
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);
    m_audio.reset();
    m_demux.reset();
    m_current_frame_position = 0;
    m_filepath.clear();
    {
        std::lock_guard<std::mutex> ml(m_metadata_mutex);
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }
}

bool SoundFileReader::is_open() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    return m_demux && m_demux->is_open() && m_audio && m_audio->is_valid();
}

// ============================================================================
// Metadata and Regions
// ============================================================================

std::optional<FileMetadata> SoundFileReader::get_metadata() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_demux || !m_audio)
        return std::nullopt;

    {
        std::lock_guard<std::mutex> ml(m_metadata_mutex);
        if (m_cached_metadata)
            return m_cached_metadata;
    }

    build_metadata(m_demux, m_audio);

    std::lock_guard<std::mutex> ml(m_metadata_mutex);
    return m_cached_metadata;
}

std::vector<FileRegion> SoundFileReader::get_regions() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_demux || !m_audio)
        return {};

    {
        std::lock_guard<std::mutex> ml(m_metadata_mutex);
        if (!m_cached_regions.empty())
            return m_cached_regions;
    }

    build_regions(m_demux, m_audio);

    std::lock_guard<std::mutex> ml(m_metadata_mutex);
    return m_cached_regions;
}

void SoundFileReader::build_metadata(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<AudioStreamContext>& audio) const
{
    FileMetadata meta;
    demux->extract_container_metadata(meta);
    audio->extract_stream_metadata(*demux, meta);

    meta.file_size = std::filesystem::file_size(m_filepath);
    auto ftime = std::filesystem::last_write_time(m_filepath);
    meta.modification_time = std::chrono::system_clock::time_point(
        std::chrono::seconds(
            std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch())));

    std::lock_guard<std::mutex> ml(m_metadata_mutex);
    m_cached_metadata = std::move(meta);
}

void SoundFileReader::build_regions(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<AudioStreamContext>& audio) const
{
    auto chapters = demux->extract_chapter_regions();
    auto cues = audio->extract_cue_regions(*demux);

    std::vector<FileRegion> all;
    all.reserve(chapters.size() + cues.size());
    all.insert(all.end(), chapters.begin(), chapters.end());
    all.insert(all.end(), cues.begin(), cues.end());

    std::lock_guard<std::mutex> ml(m_metadata_mutex);
    m_cached_regions = std::move(all);
}

// ============================================================================
// Reading Operations
// ============================================================================

std::vector<Kakshya::DataVariant> SoundFileReader::read_all()
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_demux || !m_audio) {
        set_error("File not open");
        return {};
    }
    return decode_frames(m_demux, m_audio, m_audio->total_frames, 0);
}

std::vector<Kakshya::DataVariant> SoundFileReader::read_region(const FileRegion& region)
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

std::vector<Kakshya::DataVariant> SoundFileReader::read_frames(uint64_t num_frames,
    uint64_t offset)
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_demux || !m_audio) {
        set_error("File not open");
        return {};
    }

    if (offset != m_current_frame_position.load()) {
        lock.unlock();
        std::unique_lock<std::shared_mutex> wlock(m_context_mutex);
        if (!m_demux || !m_audio) {
            set_error("File closed during operation");
            return {};
        }
        if (!seek_internal(m_demux, m_audio, offset))
            return {};
        wlock.unlock();
        lock.lock();
        if (!m_demux || !m_audio) {
            set_error("File closed during operation");
            return {};
        }
    }

    return decode_frames(m_demux, m_audio, num_frames, offset);
}

// ============================================================================
// Seeking
// ============================================================================

std::vector<uint64_t> SoundFileReader::get_read_position() const
{
    return { m_current_frame_position.load() };
}

bool SoundFileReader::seek(const std::vector<uint64_t>& position)
{
    if (position.empty()) {
        set_error("Empty position vector");
        return false;
    }
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_demux || !m_audio) {
        set_error("File not open");
        return false;
    }
    return seek_internal(m_demux, m_audio, position[0]);
}

bool SoundFileReader::seek_internal(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<AudioStreamContext>& audio,
    uint64_t frame_position)
{
    if (frame_position > audio->total_frames)
        frame_position = audio->total_frames;

    if (audio->sample_rate == 0) {
        set_error("Invalid sample rate");
        return false;
    }

    AVStream* stream = demux->get_stream(audio->stream_index);
    if (!stream) {
        set_error("Invalid stream index");
        return false;
    }

    int64_t ts = av_rescale_q(
        static_cast<int64_t>(frame_position),
        AVRational { 1, static_cast<int>(audio->sample_rate) },
        stream->time_base);

    if (!demux->seek(audio->stream_index, ts)) {
        set_error(demux->last_error());
        return false;
    }

    audio->flush_codec();
    audio->drain_resampler_init();
    m_current_frame_position = frame_position;
    return true;
}

// =========================================================================
// Decode loop
// =========================================================================

std::vector<Kakshya::DataVariant> SoundFileReader::decode_frames(
    const std::shared_ptr<FFmpegDemuxContext>& demux,
    const std::shared_ptr<AudioStreamContext>& audio,
    uint64_t num_frames,
    uint64_t /*offset*/)
{
    if (!audio->is_valid()) {
        set_error("Invalid audio context for decoding");
        return {};
    }

    bool use_planar = (m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE;
    int ch = static_cast<int>(audio->channels);

    std::vector<Kakshya::DataVariant> output;
    if (use_planar) {
        output.resize(ch);
        for (auto& v : output) {
            v = std::vector<double>();
            std::get<std::vector<double>>(v).reserve(num_frames);
        }
    } else {
        output.resize(1);
        output[0] = std::vector<double>();
        std::get<std::vector<double>>(output[0]).reserve(num_frames * static_cast<size_t>(ch));
    }

    uint64_t decoded = 0;
    bool eof_reached = false;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        set_error("Failed to allocate packet/frame");
        return {};
    }

    uint32_t out_rate = m_target_sample_rate > 0 ? m_target_sample_rate : audio->sample_rate;
    int max_resampled = static_cast<int>(av_rescale_rnd(
        static_cast<int64_t>(num_frames), out_rate, audio->sample_rate, AV_ROUND_UP));

    AVSampleFormat tgt_fmt = use_planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
    uint8_t** resample_buf = nullptr;
    int linesize = 0;

    if (av_samples_alloc_array_and_samples(
            &resample_buf, &linesize, ch, max_resampled, tgt_fmt, 0)
        < 0) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        set_error("Failed to allocate resample buffer");
        return {};
    }

    while (decoded < num_frames) {
        if (!eof_reached) {
            int ret = av_read_frame(demux->format_context, pkt);
            if (ret == AVERROR_EOF) {
                eof_reached = true;
                avcodec_send_packet(audio->codec_context, nullptr);
            } else if (ret < 0) {
                eof_reached = true;
            } else if (pkt->stream_index == audio->stream_index) {
                avcodec_send_packet(audio->codec_context, pkt);
                av_packet_unref(pkt);
            } else {
                av_packet_unref(pkt);
            }
        }

        int receive_ret = 0;
        while (decoded < num_frames) {
            receive_ret = avcodec_receive_frame(audio->codec_context, frame);

            if (receive_ret == AVERROR(EAGAIN))
                break;
            if (receive_ret == AVERROR_EOF) {
                // decoded = num_frames;
                break;
            }
            if (receive_ret < 0)
                break;

            int out_samples = swr_convert(
                audio->swr_context,
                resample_buf, max_resampled,
                const_cast<const uint8_t**>(frame->data),
                frame->nb_samples);

            if (out_samples > 0) {
                uint64_t to_copy = std::min(static_cast<uint64_t>(out_samples),
                    num_frames - decoded);
                if (use_planar) {
                    for (int c = 0; c < ch; ++c) {
                        auto* src = reinterpret_cast<double*>(resample_buf[c]);
                        auto& dst = std::get<std::vector<double>>(output[c]);
                        dst.insert(dst.end(), src, src + to_copy);
                    }
                } else {
                    auto* src = reinterpret_cast<double*>(resample_buf[0]);
                    auto& dst = std::get<std::vector<double>>(output[0]);
                    dst.insert(dst.end(), src, src + to_copy * static_cast<uint64_t>(ch));
                }
                decoded += to_copy;
            }
            av_frame_unref(frame);
        }

        if (eof_reached && receive_ret == AVERROR_EOF)
            break;
    }

    while (true) {
        int n = swr_convert(audio->swr_context, resample_buf, max_resampled, nullptr, 0);
        if (n <= 0)
            break;

        uint64_t to_copy = std::min(static_cast<uint64_t>(n),
            (num_frames > decoded) ? (num_frames - decoded) : 0);

        if (to_copy > 0) {
            if (use_planar) {
                for (int c = 0; c < ch; ++c) {
                    auto* src = reinterpret_cast<double*>(resample_buf[c]);
                    auto& dst = std::get<std::vector<double>>(output[c]);
                    dst.insert(dst.end(), src, src + to_copy);
                }
            } else {
                auto* src = reinterpret_cast<double*>(resample_buf[0]);
                auto& dst = std::get<std::vector<double>>(output[0]);
                dst.insert(dst.end(), src, src + to_copy * static_cast<uint64_t>(ch));
            }
            decoded += to_copy;
        } else {
            break;
        }
    }

    av_freep(&resample_buf[0]);
    av_freep(&resample_buf);
    av_packet_free(&pkt);
    av_frame_free(&frame);

    m_current_frame_position += decoded;
    return output;
}

// =========================================================================
// Container Operations
// =========================================================================

std::shared_ptr<Kakshya::SignalSourceContainer> SoundFileReader::create_container()
{
    std::shared_lock lock(m_context_mutex);
    if (!m_demux || !m_audio) {
        set_error("File not open");
        return nullptr;
    }

    return std::make_shared<Kakshya::SoundFileContainer>();
}

bool SoundFileReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container) {
        set_error("Invalid container");
        return false;
    }

    auto sc = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sc) {
        set_error("Container is not a SoundFileContainer");
        return false;
    }

    std::shared_ptr<AudioStreamContext> audio;
    {
        std::shared_lock lock(m_context_mutex);
        if (!m_demux || !m_audio) {
            set_error("File not open");
            return false;
        }
        audio = m_audio;
    }

    sc->setup(audio->total_frames, audio->sample_rate, audio->channels);

    bool planar = (m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE;
    sc->get_structure().organization = planar
        ? Kakshya::OrganizationStrategy::PLANAR
        : Kakshya::OrganizationStrategy::INTERLEAVED;

    auto data = read_all();
    if (data.empty()) {
        set_error("Failed to read audio data");
        return false;
    }

    sc->set_raw_data(data);

    auto regions = get_regions();
    auto region_groups = regions_to_groups(regions);
    for (const auto& [name, group] : region_groups)
        sc->add_region_group(group);

    sc->create_default_processor();
    sc->mark_ready_for_processing(true);
    return true;
}

// ============================================================================
// Utility Methods
// ============================================================================

size_t SoundFileReader::get_num_dimensions() const
{
    return 2; // time × channels
}

std::vector<uint64_t> SoundFileReader::get_dimension_sizes() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    if (!m_audio)
        return { 0, 0 };
    return { m_audio->total_frames, m_audio->channels };
}

std::vector<std::string> SoundFileReader::get_supported_extensions() const
{
    return {
        "wav", "flac", "mp3", "m4a", "aac", "ogg", "opus", "wma",
        "aiff", "aif", "ape", "wv", "tta", "mka", "ac3", "dts",
        "mp2", "mp4", "webm", "caf", "amr", "au", "voc", "w64",
        "mpc", "mp+", "m4b", "m4r", "3gp", "3g2", "asf", "rm",
        "ra", "avi", "mov", "mkv", "ogv", "ogx", "oga", "spx",
        "f4a", "f4b", "f4v", "m4v", "asx", "wvx", "wax"
    };
}

std::string SoundFileReader::get_last_error() const
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    return m_last_error;
}

void SoundFileReader::set_error(const std::string& err) const
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_last_error = err;
    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "SoundFileReader: {}", err);
}

void SoundFileReader::clear_error() const
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_last_error.clear();
}

} // namespace MayaFlux::IO
