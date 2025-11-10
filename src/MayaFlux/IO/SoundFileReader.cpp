#include "SoundFileReader.hpp"

#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace MayaFlux::IO {

// ============================================================================
// FFmpegContext Implementation
// ============================================================================

FFmpegContext::~FFmpegContext()
{
    // Cleanup order matters: resampler -> codec -> format
    if (swr_context) {
        swr_free(&swr_context);
        swr_context = nullptr;
    }

    if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
    }

    if (format_context) {
        avformat_close_input(&format_context);
        format_context = nullptr;
    }

    audio_stream_index = -1;
}

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
// Static Members
// ============================================================================

std::atomic<bool> SoundFileReader::s_ffmpeg_initialized { false };
std::mutex SoundFileReader::s_ffmpeg_init_mutex;

// ============================================================================
// Constructor/Destructor
// ============================================================================

SoundFileReader::SoundFileReader()
{
    initialize_ffmpeg();
}

SoundFileReader::~SoundFileReader()
{
    close();
}

void SoundFileReader::initialize_ffmpeg()
{
    std::lock_guard<std::mutex> lock(s_ffmpeg_init_mutex);
    if (!s_ffmpeg_initialized.exchange(true)) {
        av_log_set_level(AV_LOG_WARNING);
    }
}

// ============================================================================
// File Operations
// ============================================================================

bool SoundFileReader::can_read(const std::string& filepath) const
{
    AVFormatContext* format_ctx = nullptr;
    int ret = avformat_open_input(&format_ctx, filepath.c_str(), nullptr, nullptr);

    if (ret < 0) {
        return false;
    }

    bool has_audio = false;
    if (avformat_find_stream_info(format_ctx, nullptr) >= 0) {
        int audio_stream = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        has_audio = (audio_stream >= 0);
    }

    avformat_close_input(&format_ctx);
    return has_audio;
}

bool SoundFileReader::open(const std::string& filepath, FileReadOptions options)
{
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);

    if (m_context) {
        m_context.reset();
        m_current_frame_position = 0;
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }

    m_filepath = filepath;
    m_options = options;
    clear_error();

    auto ctx = std::make_shared<FFmpegContext>();
    if (avformat_open_input(&ctx->format_context, filepath.c_str(), nullptr, nullptr) < 0) {
        set_error("Failed to open file: " + filepath);
        return false;
    }

    if (avformat_find_stream_info(ctx->format_context, nullptr) < 0) {
        set_error("Failed to find stream info");
        return false;
    }

    const AVCodec* codec = nullptr;
    ctx->audio_stream_index = av_find_best_stream(
        ctx->format_context, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

    if (ctx->audio_stream_index < 0 || !codec) {
        set_error("No audio stream found");
        return false;
    }

    ctx->codec_context = avcodec_alloc_context3(codec);
    if (!ctx->codec_context) {
        set_error("Failed to allocate codec context");
        return false;
    }

    AVStream* stream = ctx->format_context->streams[ctx->audio_stream_index];
    if (avcodec_parameters_to_context(ctx->codec_context, stream->codecpar) < 0) {
        set_error("Failed to copy codec parameters");
        return false;
    }

    if (avcodec_open2(ctx->codec_context, codec, nullptr) < 0) {
        set_error("Failed to open codec");
        return false;
    }

    if (stream->duration != AV_NOPTS_VALUE && stream->time_base.num && stream->time_base.den) {
        double duration_seconds = stream->duration * av_q2d(stream->time_base);
        ctx->total_frames = static_cast<uint64_t>(duration_seconds * ctx->codec_context->sample_rate);
    } else if (ctx->format_context->duration != AV_NOPTS_VALUE) {
        double duration_seconds = ctx->format_context->duration / static_cast<double>(AV_TIME_BASE);
        ctx->total_frames = static_cast<uint64_t>(duration_seconds * ctx->codec_context->sample_rate);
    } else {
        ctx->total_frames = 0;
    }

    ctx->sample_rate = ctx->codec_context->sample_rate;
    ctx->channels = ctx->codec_context->ch_layout.nb_channels;
    if (!setup_resampler(ctx)) {
        set_error("Failed to setup resampler");
        return false;
    }

    if (!ctx->is_valid()) {
        set_error("Invalid context after initialization");
        return false;
    }

    if ((options & FileReadOptions::EXTRACT_METADATA) != FileReadOptions::NONE) {
        extract_metadata(ctx);
    }

    if ((options & FileReadOptions::EXTRACT_REGIONS) != FileReadOptions::NONE) {
        extract_regions(ctx);
    }

    m_context = std::move(ctx);
    m_current_frame_position = 0;
    return true;
}

void SoundFileReader::close()
{
    std::unique_lock<std::shared_mutex> lock(m_context_mutex);

    if (m_context) {
        m_context.reset();
        m_current_frame_position = 0;
        m_filepath.clear();
        m_cached_metadata.reset();
        m_cached_regions.clear();
    }
}

bool SoundFileReader::is_open() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);
    return m_context && m_context->is_valid();
}

// ============================================================================
// Resampler Setup
// ============================================================================

bool SoundFileReader::setup_resampler(const std::shared_ptr<FFmpegContext>& ctx)
{
    if (!ctx || !ctx->codec_context) {
        return false;
    }

    AVChannelLayout out_ch_layout;
    av_channel_layout_copy(&out_ch_layout, &ctx->codec_context->ch_layout);

    uint32_t out_sample_rate = m_target_sample_rate > 0 ? m_target_sample_rate : ctx->codec_context->sample_rate;

    AVSampleFormat out_sample_fmt = (m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE
        ? AV_SAMPLE_FMT_DBLP
        : AV_SAMPLE_FMT_DBL;

    int ret = swr_alloc_set_opts2(&ctx->swr_context,
        &out_ch_layout, out_sample_fmt, out_sample_rate,
        &ctx->codec_context->ch_layout, ctx->codec_context->sample_fmt,
        ctx->codec_context->sample_rate,
        0, nullptr);

    av_channel_layout_uninit(&out_ch_layout);

    if (ret < 0 || !ctx->swr_context) {
        set_error("Failed to allocate resampler");
        return false;
    }

    if (swr_init(ctx->swr_context) < 0) {
        set_error("Failed to initialize resampler");
        return false;
    }

    return true;
}

// ============================================================================
// Metadata and Regions
// ============================================================================

std::optional<FileMetadata> SoundFileReader::get_metadata() const
{
    std::shared_lock<std::shared_mutex> ctx_lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        return std::nullopt;
    }

    {
        std::lock_guard<std::mutex> meta_lock(m_metadata_mutex);
        if (m_cached_metadata) {
            return m_cached_metadata;
        }
    }

    FileMetadata metadata;
    auto ctx = m_context;

    metadata.format = ctx->format_context->iformat->name;
    metadata.mime_type = ctx->format_context->iformat->mime_type
        ? ctx->format_context->iformat->mime_type
        : "audio/" + std::string(ctx->format_context->iformat->name);

    metadata.file_size = std::filesystem::file_size(m_filepath);
    auto ftime = std::filesystem::last_write_time(m_filepath);
    metadata.modification_time = std::chrono::system_clock::time_point(
        std::chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(
            ftime.time_since_epoch())));

    metadata.attributes["codec"] = avcodec_get_name(ctx->codec_context->codec_id);
    metadata.attributes["codec_long_name"] = ctx->codec_context->codec->long_name;
    metadata.attributes["total_frames"] = ctx->total_frames;
    metadata.attributes["sample_rate"] = ctx->sample_rate;
    metadata.attributes["channels"] = ctx->channels;

    char layout_desc[256];
    av_channel_layout_describe(&ctx->codec_context->ch_layout, layout_desc, sizeof(layout_desc));
    metadata.attributes["channel_layout"] = std::string(layout_desc);
    metadata.attributes["bit_rate"] = ctx->codec_context->bit_rate;

    if (ctx->format_context->duration != AV_NOPTS_VALUE) {
        metadata.attributes["duration_seconds"] = ctx->format_context->duration / static_cast<double>(AV_TIME_BASE);
    } else if (ctx->total_frames > 0) {
        metadata.attributes["duration_seconds"] = ctx->total_frames / static_cast<double>(ctx->sample_rate);
    }

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(ctx->format_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        metadata.attributes[std::string("tag_") + tag->key] = tag->value;
    }

    AVStream* stream = ctx->format_context->streams[ctx->audio_stream_index];
    tag = nullptr;
    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        metadata.attributes[std::string("stream_") + tag->key] = tag->value;
    }

    {
        std::lock_guard<std::mutex> meta_lock(m_metadata_mutex);
        m_cached_metadata = metadata;
    }

    return metadata;
}

void SoundFileReader::extract_metadata(const std::shared_ptr<FFmpegContext>& ctx)
{
    if (!ctx || !ctx->is_valid()) {
        return;
    }

    std::lock_guard<std::mutex> meta_lock(m_metadata_mutex);

    FileMetadata metadata;

    metadata.format = ctx->format_context->iformat->name;
    metadata.mime_type = ctx->format_context->iformat->mime_type
        ? ctx->format_context->iformat->mime_type
        : "audio/" + std::string(ctx->format_context->iformat->name);

    metadata.file_size = std::filesystem::file_size(m_filepath);
    auto ftime = std::filesystem::last_write_time(m_filepath);
    metadata.modification_time = std::chrono::system_clock::time_point(
        std::chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(
            ftime.time_since_epoch())));

    metadata.attributes["codec"] = avcodec_get_name(ctx->codec_context->codec_id);
    metadata.attributes["codec_long_name"] = ctx->codec_context->codec->long_name;
    metadata.attributes["total_frames"] = ctx->total_frames;
    metadata.attributes["sample_rate"] = ctx->sample_rate;
    metadata.attributes["channels"] = ctx->channels;

    char layout_desc[256];
    av_channel_layout_describe(&ctx->codec_context->ch_layout, layout_desc, sizeof(layout_desc));
    metadata.attributes["channel_layout"] = std::string(layout_desc);
    metadata.attributes["bit_rate"] = ctx->codec_context->bit_rate;

    if (ctx->format_context->duration != AV_NOPTS_VALUE) {
        metadata.attributes["duration_seconds"] = ctx->format_context->duration / static_cast<double>(AV_TIME_BASE);
    } else if (ctx->total_frames > 0) {
        metadata.attributes["duration_seconds"] = ctx->total_frames / static_cast<double>(ctx->sample_rate);
    }

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(ctx->format_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        metadata.attributes[std::string("tag_") + tag->key] = tag->value;
    }

    AVStream* stream = ctx->format_context->streams[ctx->audio_stream_index];
    tag = nullptr;
    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        metadata.attributes[std::string("stream_") + tag->key] = tag->value;
    }

    m_cached_metadata = metadata;
}

void SoundFileReader::extract_regions(const std::shared_ptr<FFmpegContext>& ctx)
{
    if (!ctx || !ctx->is_valid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_metadata_mutex);
    m_cached_regions.clear();

    for (unsigned int i = 0; i < ctx->format_context->nb_chapters; i++) {
        AVChapter* chapter = ctx->format_context->chapters[i];

        FileRegion region;
        region.type = "chapter";

        uint64_t start = av_rescale_q(chapter->start, chapter->time_base,
            AVRational { 1, static_cast<int>(ctx->sample_rate) });
        uint64_t end = av_rescale_q(chapter->end, chapter->time_base,
            AVRational { 1, static_cast<int>(ctx->sample_rate) });

        region.start_coordinates = { start };
        region.end_coordinates = { end };

        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(chapter->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            if (strcmp(entry->key, "title") == 0) {
                region.name = entry->value;
            } else {
                region.attributes[entry->key] = entry->value;
            }
        }

        if (region.name.empty()) {
            region.name = "Chapter " + std::to_string(i + 1);
        }

        m_cached_regions.push_back(region);
    }

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(ctx->format_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        std::string key = tag->key;
        if (key.find("cue") != std::string::npos || key.find("CUE") != std::string::npos) {
            FileRegion region;
            region.type = "cue";
            region.name = key;
            region.attributes["description"] = tag->value;

            try {
                uint64_t position = std::stoull(tag->value);
                region.start_coordinates = { position };
                region.end_coordinates = { position };
            } catch (...) {
                region.start_coordinates = { 0 };
                region.end_coordinates = { 0 };
                region.attributes["value"] = tag->value;
            }
            m_cached_regions.push_back(region);
        }

        if (key.find("loop") != std::string::npos || key.find("LOOP") != std::string::npos) {
            FileRegion region;
            region.type = "loop";
            region.name = key;
            region.attributes["value"] = tag->value;
            region.start_coordinates = { 0 };
            region.end_coordinates = { 0 };
            m_cached_regions.push_back(region);
        }
    }

    /* tag = nullptr;
    while ((tag = av_dict_get(ctx->format_context->streams[m_audio_stream_index]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        std::string key = tag->key;

        if (key.find("marker") != std::string::npos || key.find("MARKER") != std::string::npos) {
            FileRegion region;
            region.type = "marker";
            region.name = key;
            region.attributes["value"] = tag->value;
            region.start_coordinates = { 0 };
            region.end_coordinates = { 0 };
            m_cached_regions.push_back(region);
        }
    } */
}

std::vector<FileRegion> SoundFileReader::get_regions() const
{
    std::shared_lock<std::shared_mutex> ctx_lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        return {};
    }

    std::lock_guard<std::mutex> meta_lock(m_metadata_mutex);
    return m_cached_regions;
}

// ============================================================================
// Reading Operations
// ============================================================================

std::vector<Kakshya::DataVariant> SoundFileReader::read_all()
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        set_error("File not open");
        return {};
    }

    auto ctx = m_context;
    lock.unlock();

    return read_frames(ctx->total_frames, 0);
}

std::vector<Kakshya::DataVariant> SoundFileReader::read_frames(uint64_t num_frames, uint64_t offset)
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        set_error("File not open");
        return {};
    }

    auto ctx = m_context;

    if (offset != m_current_frame_position.load()) {
        lock.unlock();
        std::unique_lock<std::shared_mutex> write_lock(m_context_mutex);

        if (!m_context || !m_context->is_valid()) {
            set_error("File closed during operation");
            return {};
        }

        ctx = m_context;
        if (!seek_internal(ctx, offset)) {
            return {};
        }

        write_lock.unlock();
        lock.lock();

        if (!m_context || !m_context->is_valid()) {
            set_error("File closed during operation");
            return {};
        }

        ctx = m_context;
    }

    return decode_frames(ctx, num_frames, offset);
}

std::vector<Kakshya::DataVariant> SoundFileReader::decode_frames(
    std::shared_ptr<FFmpegContext> ctx,
    uint64_t num_frames,
    uint64_t offset)
{
    if (!ctx || !ctx->is_valid() || !ctx->swr_context) {
        set_error("Invalid context for decoding");
        return {};
    }

    std::vector<Kakshya::DataVariant> output_data;
    uint64_t frames_decoded = 0;

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    if (!packet || !frame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        set_error("Failed to allocate packet/frame");
        return {};
    }

    int channels = ctx->channels;
    bool use_planar = (m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE;

    if (use_planar) {
        output_data.resize(channels);
        for (auto& channel_vector : output_data) {
            channel_vector = std::vector<double>();
            std::get<std::vector<double>>(channel_vector).reserve(num_frames);
        }
    } else {
        output_data.resize(1);
        output_data[0] = std::vector<double>();
        std::get<std::vector<double>>(output_data[0]).reserve(num_frames * channels);
    }

    uint8_t** resample_buffer = nullptr;
    int resample_linesize = 0;

    int max_resample_samples = av_rescale_rnd(
        num_frames,
        m_target_sample_rate > 0 ? m_target_sample_rate : ctx->sample_rate,
        ctx->sample_rate,
        AV_ROUND_UP);

    AVSampleFormat target_format = use_planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;

    int alloc_ret = av_samples_alloc_array_and_samples(
        &resample_buffer, &resample_linesize,
        channels, max_resample_samples, target_format, 0);

    if (alloc_ret < 0 || !resample_buffer) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        set_error("Failed to allocate resample buffer");
        return {};
    }

    while (frames_decoded < num_frames) {
        int ret = av_read_frame(ctx->format_context, packet);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                avcodec_send_packet(ctx->codec_context, nullptr);
            } else {
                break;
            }
        } else if (packet->stream_index != ctx->audio_stream_index) {
            av_packet_unref(packet);
            continue;
        } else {
            ret = avcodec_send_packet(ctx->codec_context, packet);
            av_packet_unref(packet);

            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                continue;
            }
        }

        while (ret >= 0 && frames_decoded < num_frames) {
            ret = avcodec_receive_frame(ctx->codec_context, frame);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }

            int out_samples = swr_convert(ctx->swr_context,
                resample_buffer, max_resample_samples,
                (const uint8_t**)frame->data, frame->nb_samples);

            if (out_samples > 0) {
                uint64_t samples_to_copy = std::min(
                    static_cast<uint64_t>(out_samples),
                    num_frames - frames_decoded);

                if (use_planar) {
                    for (int ch = 0; ch < channels; ++ch) {
                        double* channel_data = reinterpret_cast<double*>(resample_buffer[ch]);
                        auto& channel_vector = std::get<std::vector<double>>(output_data[ch]);
                        channel_vector.insert(channel_vector.end(),
                            channel_data, channel_data + samples_to_copy);
                    }
                } else {
                    double* interleaved_data = reinterpret_cast<double*>(resample_buffer[0]);
                    auto& interleaved_vector = std::get<std::vector<double>>(output_data[0]);
                    interleaved_vector.insert(interleaved_vector.end(),
                        interleaved_data, interleaved_data + samples_to_copy * channels);
                }

                frames_decoded += samples_to_copy;
            }

            av_frame_unref(frame);
        }

        if (ret == AVERROR_EOF) {
            break;
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);

    if (resample_buffer) {
        av_freep(&resample_buffer[0]);
        av_freep(&resample_buffer);
    }

    m_current_frame_position = offset + frames_decoded;
    return output_data;
}

std::vector<Kakshya::DataVariant> SoundFileReader::read_region(const FileRegion& region)
{
    if (region.start_coordinates.empty()) {
        set_error("Invalid region");
        return {};
    }

    uint64_t start = region.start_coordinates[0];
    uint64_t end = region.end_coordinates.empty() ? start : region.end_coordinates[0];
    uint64_t num_frames = (end > start) ? (end - start) : 1;

    return read_frames(num_frames, start);
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

    if (!m_context || !m_context->is_valid()) {
        set_error("File not open");
        return false;
    }

    return seek_internal(m_context, position[0]);
}

bool SoundFileReader::seek_internal(std::shared_ptr<FFmpegContext>& ctx, uint64_t frame_position)
{
    if (!ctx || !ctx->is_valid()) {
        set_error("Invalid context for seeking");
        return false;
    }

    if (frame_position > ctx->total_frames) {
        frame_position = ctx->total_frames;
    }

    if (ctx->sample_rate == 0) {
        set_error("Invalid sample rate");
        return false;
    }

    if (ctx->audio_stream_index < 0 || ctx->audio_stream_index >= static_cast<int>(ctx->format_context->nb_streams)) {
        set_error("Invalid audio stream index");
        return false;
    }

    AVStream* stream = ctx->format_context->streams[ctx->audio_stream_index];

    int64_t timestamp = av_rescale_q(
        frame_position,
        AVRational { 1, static_cast<int>(ctx->sample_rate) },
        stream->time_base);

    int ret = av_seek_frame(
        ctx->format_context,
        ctx->audio_stream_index,
        timestamp,
        AVSEEK_FLAG_BACKWARD);

    if (ret < 0) {
        set_error("Seek operation failed");
        return false;
    }

    avcodec_flush_buffers(ctx->codec_context);

    if (ctx->swr_context) {
        uint8_t** dummy = nullptr;
        int linesize = 0;

        int alloc_ret = av_samples_alloc_array_and_samples(
            &dummy, &linesize,
            ctx->channels, 2048, AV_SAMPLE_FMT_DBL, 0);

        if (alloc_ret >= 0 && dummy) {
            while (swr_convert(ctx->swr_context, dummy, 2048, nullptr, 0) > 0) {
            }

            av_freep(&dummy[0]);
            av_freep(&dummy);
        }
    }

    m_current_frame_position = frame_position;
    return true;
}

// ============================================================================
// Container Operations
// ============================================================================

std::shared_ptr<Kakshya::SignalSourceContainer> SoundFileReader::create_container()
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        set_error("File not open");
        return nullptr;
    }

    auto container = std::make_shared<Kakshya::SoundFileContainer>();
    lock.unlock();

    if (!load_into_container(container)) {
        return nullptr;
    }

    return container;
}

bool SoundFileReader::load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container) {
        set_error("Invalid container");
        return false;
    }

    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sound_container) {
        set_error("Container is not a SoundFileContainer");
        return false;
    }

    auto metadata = get_metadata();
    if (!metadata) {
        set_error("Failed to get metadata");
        return false;
    }

    auto total_frames = metadata->get_attribute<uint64_t>("total_frames").value_or(0);
    auto sample_rate = metadata->get_attribute<uint32_t>("sample_rate").value_or(48000);
    auto channels = metadata->get_attribute<uint32_t>("channels").value_or(2);

    sound_container->setup(total_frames, sample_rate, channels);

    if ((m_audio_options & AudioReadOptions::DEINTERLEAVE) != AudioReadOptions::NONE) {
        sound_container->get_structure().organization = Kakshya::OrganizationStrategy::PLANAR;
    } else {
        sound_container->get_structure().organization = Kakshya::OrganizationStrategy::INTERLEAVED;
    }

    std::vector<Kakshya::DataVariant> audio_data = read_all();

    if (audio_data.empty()) {
        set_error("Failed to read audio data");
        return false;
    }

    sound_container->set_raw_data(audio_data);

    auto regions = get_regions();
    auto region_groups = regions_to_groups(regions);
    for (const auto& [name, group] : region_groups) {
        sound_container->add_region_group(group);
    }

    sound_container->create_default_processor();
    sound_container->mark_ready_for_processing(true);

    return true;
}

// ============================================================================
// Utility Methods
// ============================================================================

uint64_t SoundFileReader::get_preferred_chunk_size() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (m_context && m_context->codec_context && m_context->codec_context->codec) {
        if (m_context->codec_context->frame_size > 0) {
            return m_context->codec_context->frame_size * 4;
        }
    }
    return 4096;
}

size_t SoundFileReader::get_num_dimensions() const
{
    return 2; // time × channels
}

std::vector<uint64_t> SoundFileReader::get_dimension_sizes() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (!m_context || !m_context->is_valid()) {
        return {};
    }

    return { m_context->total_frames, static_cast<uint64_t>(m_context->channels) };
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
    std::lock_guard<std::mutex> lock(m_metadata_mutex);
    return m_last_error;
}

bool SoundFileReader::supports_streaming() const
{
    std::shared_lock<std::shared_mutex> lock(m_context_mutex);

    if (!m_context || !m_context->format_context) {
        return false;
    }

    return m_context->format_context->pb && m_context->format_context->pb->seekable;
}

void SoundFileReader::set_error(const std::string& error) const
{
    std::lock_guard<std::mutex> lock(m_metadata_mutex);
    m_last_error = error;
}

void SoundFileReader::clear_error() const
{
    std::lock_guard<std::mutex> lock(m_metadata_mutex);
    m_last_error.clear();
}

std::vector<std::vector<double>> SoundFileReader::deinterleave_data(
    const std::vector<double>& interleaved, uint32_t channels)
{
    if (channels == 1) {
        return { interleaved };
    }

    std::vector<std::vector<double>> deinterleaved(channels);

    size_t samples_per_channel = interleaved.size() / channels;

    for (uint32_t ch = 0; ch < channels; ch++) {
        deinterleaved[ch].reserve(samples_per_channel);
        for (size_t i = 0; i < samples_per_channel; i++) {
            deinterleaved[ch].push_back(interleaved[i * channels + ch]);
        }
    }

    return deinterleaved;
}

} // namespace MayaFlux::IO
