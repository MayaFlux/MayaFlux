#include "FFmpegCameraReader.hpp"

#include "MayaFlux/Kakshya/Source/CameraContainer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/IOService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace MayaFlux::IO {

FFmpegCameraReader::FFmpegCameraReader()
    : m_demux(std::make_shared<FFmpegDemuxContext>())
    , m_video(std::make_shared<VideoStreamContext>())
{
}

void FFmpegCameraReader::setup_io_service(uint64_t reader_id)
{
    m_standalone_reader_id = reader_id;

    if (!Registry::BackendRegistry::instance()
            .get_service<Registry::Service::IOService>()) {

        m_standalone_service = std::make_shared<Registry::Service::IOService>();
        m_standalone_service->request_frame = [this](uint64_t reader_id) {
            if (reader_id == m_standalone_reader_id)
                pull_frame_all();
        };

        Registry::BackendRegistry::instance()
            .register_service<Registry::Service::IOService>(
                [this]() -> void* { return m_standalone_service.get(); });

        m_owns_service = true;
    }
}

FFmpegCameraReader::~FFmpegCameraReader()
{
    close();

    if (m_owns_service) {
        Registry::BackendRegistry::instance()
            .unregister_service<Registry::Service::IOService>();
    }
}

#ifdef MAYAFLUX_PLATFORM_MACOS
double macos_camera_max_fps(const std::string& device_name);
#endif

namespace {

    constexpr std::string_view PREFERRED_INPUT_FORMAT =
#ifdef MAYAFLUX_PLATFORM_MACOS
        "";
#else
        "mjpeg";
#endif

    struct OpenHints {
        bool format;
        bool size;
        bool fps;
    };

    AVDictionary* build_options(const CameraConfig& config,
        const std::string& input_format, const OpenHints& hints)
    {
        AVDictionary* opts = nullptr;

        if (hints.fps && config.target_fps > 0.0)
            av_dict_set(&opts, "framerate", std::to_string(config.target_fps).c_str(), 0);
#ifdef MAYAFLUX_PLATFORM_MACOS
        else if (const double native = macos_camera_max_fps(config.device_name); native > 0.0)
            av_dict_set(&opts, "framerate", std::to_string(native).c_str(), 0);
#endif

        if (hints.size && config.target_width > 0 && config.target_height > 0) {
            const std::string size_str = std::to_string(config.target_width)
                + "x"
                + std::to_string(config.target_height);
            av_dict_set(&opts, "video_size", size_str.c_str(), 0);
        }

        if (hints.format && !input_format.empty()) {
#if defined(MAYAFLUX_PLATFORM_LINUX)
            av_dict_set(&opts, "input_format", input_format.c_str(), 0);
#elif defined(MAYAFLUX_PLATFORM_WINDOWS)
            const bool raw = av_get_pix_fmt(input_format.c_str()) != AV_PIX_FMT_NONE;
            av_dict_set(&opts, raw ? "pixel_format" : "vcodec", input_format.c_str(), 0);
#endif
        }

        return opts;
    }

}

bool FFmpegCameraReader::open(const CameraConfig& config)
{
    close();

    const std::string fmt_name = config.format_override.empty()
        ? std::string(CAMERA_FORMAT)
        : config.format_override;

    const std::string input_format = config.input_format.empty()
        ? std::string(PREFERRED_INPUT_FORMAT)
        : config.input_format;

    constexpr std::array<OpenHints, 4> ladder { {
        { .format = true, .size = true, .fps = true },
        { .format = false, .size = true, .fps = true },
        { .format = false, .size = true, .fps = false },
        { .format = false, .size = false, .fps = false },
    } };

    m_requested_pixel_format = config.pixel_format;

    for (size_t step = 0; step < ladder.size(); ++step) {
        const OpenHints& hints = ladder.at(step);
        if (hints.format && input_format.empty())
            continue;

        AVDictionary* opts = build_options(config, input_format, hints);
        const bool opened = m_demux->open_device(config.device_name, fmt_name, &opts);
        av_dict_free(&opts);

        if (!opened) {
            m_last_error = "Device open failed: " + m_demux->last_error();
            continue;
        }

        if (!m_video->open_device(*m_demux,
                hints.size ? config.target_width : 0,
                hints.size ? config.target_height : 0,
                config.pixel_format)) {
            m_last_error = "Video stream open failed: " + m_video->last_error();
            m_demux->close();
            continue;
        }

        m_sws_buf.clear();
        m_scaler_ready = false;

        if (step > (input_format.empty() ? 1U : 0U)) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "FFmpegCameraReader::open: '{}' did not accept the requested mode, "
                "opened with fallback step {}",
                config.device_name, step + 1);
        }

        MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
            "FFmpegCameraReader: opened '{}' via {}: {}x{} @{:.1f}fps (scaler deferred)",
            config.device_name, fmt_name,
            m_video->width, m_video->height, m_video->frame_rate);

        return true;
    }

    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
        "FFmpegCameraReader::open: {}", m_last_error);
    return false;
}

void FFmpegCameraReader::close()
{
    stop_decode_thread();

    std::unique_lock lock(m_ctx_mutex);
    m_video->close();
    m_demux->close();
    m_scaler_ready = false;
    m_sws_buf.clear();
    m_sws_buf.shrink_to_fit();
    m_last_error.clear();
}

bool FFmpegCameraReader::is_open() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_demux->is_open() && m_video->is_codec_valid();
}

std::shared_ptr<Kakshya::CameraContainer>
FFmpegCameraReader::create_container() const
{
    std::shared_lock lock(m_ctx_mutex);
    if (!m_video->is_codec_valid()) {
        m_last_error = "Cannot create container: reader not open";
        return nullptr;
    }

    const auto fmt = to_image_format(m_requested_pixel_format);
    if (!fmt) {
        m_last_error = "Requested pixel format has no ImageFormat equivalent";
        return nullptr;
    }

    return std::make_shared<Kakshya::CameraContainer>(
        m_video->out_width,
        m_video->out_height,
        *fmt,
        m_video->frame_rate);
}

bool FFmpegCameraReader::pull_frame(
    const std::shared_ptr<Kakshya::CameraContainer>& container)
{
    if (!container)
        return false;

    std::shared_lock lock(m_ctx_mutex);
    if (!m_video->is_codec_valid())
        return false;

    uint8_t* dest = container->mutable_frame_ptr();
    if (!dest)
        return false;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
        return false;

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        av_packet_free(&pkt);
        return false;
    }

    bool got_frame = false;

    while (!got_frame) {
        int ret = av_read_frame(m_demux->format_context, pkt);
        if (ret < 0)
            break;

        if (pkt->stream_index != m_video->stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(m_video->codec_context, pkt);
        av_packet_unref(pkt);

        if (ret < 0 && ret != AVERROR(EAGAIN))
            break;

        ret = avcodec_receive_frame(m_video->codec_context, frame);
        if (ret == AVERROR(EAGAIN))
            continue;
        if (ret < 0)
            break;

        if (!m_scaler_ready) {
            if (!m_video->rebuild_scaler_from_frame(
                    frame,
                    static_cast<uint32_t>(frame->width),
                    static_cast<uint32_t>(frame->height))) {
                MF_ERROR(Journal::Component::IO, Journal::Context::Runtime,
                    "FFmpegCameraReader: scaler init failed: {}",
                    m_video->last_error());
                break;
            }
            const size_t buf_bytes = static_cast<size_t>(m_video->out_linesize) * m_video->out_height;
            m_sws_buf.assign(buf_bytes, 0);
            m_scaler_ready = true;
        }

        uint8_t* sws_dst[1] = { m_sws_buf.data() };
        int sws_stride[1] = { m_video->out_linesize };

        sws_scale(m_video->sws_context,
            frame->data, frame->linesize,
            0, static_cast<int>(m_video->height),
            sws_dst, sws_stride);

        const int packed_stride = static_cast<int>(m_video->out_width * m_video->out_bytes_per_pixel);

        if (m_video->out_linesize == packed_stride) {
            std::memcpy(dest, m_sws_buf.data(),
                static_cast<size_t>(packed_stride) * m_video->out_height);
        } else {
            for (uint32_t row = 0; row < m_video->out_height; ++row) {
                std::memcpy(
                    dest + static_cast<size_t>(row) * packed_stride,
                    m_sws_buf.data() + static_cast<size_t>(row) * m_video->out_linesize,
                    static_cast<size_t>(packed_stride));
            }
        }

        container->mark_ready_for_processing(true);
        got_frame = true;
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    return got_frame;
}

uint32_t FFmpegCameraReader::width() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->out_width;
}

uint32_t FFmpegCameraReader::height() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->out_height;
}

double FFmpegCameraReader::frame_rate() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->frame_rate;
}

void FFmpegCameraReader::set_container(
    const std::shared_ptr<Kakshya::CameraContainer>& container)
{
    m_container_ref = container;
    start_decode_thread();
}

void FFmpegCameraReader::pull_frame_all()
{
    {
        std::lock_guard lock(m_decode_mutex);
        m_frame_requested.store(true, std::memory_order_relaxed);
    }
    m_decode_cv.notify_one();
}

const std::string& FFmpegCameraReader::last_error() const
{
    return m_last_error;
}

void FFmpegCameraReader::start_decode_thread()
{
    stop_decode_thread();

    m_decode_stop.store(false);
    m_decode_active.store(true);
    m_decode_thread = std::thread(&FFmpegCameraReader::decode_thread_func, this);
}

void FFmpegCameraReader::stop_decode_thread()
{
    if (!m_decode_active.load())
        return;

    m_decode_stop.store(true);
    m_decode_cv.notify_all();

    if (m_decode_thread.joinable())
        m_decode_thread.join();

    m_decode_active.store(false);
}

void FFmpegCameraReader::decode_thread_func()
{
    while (!m_decode_stop.load()) {
        {
            std::unique_lock lock(m_decode_mutex);
            m_decode_cv.wait(lock, [this] {
                return m_decode_stop.load()
                    || m_frame_requested.load(std::memory_order_relaxed);
            });
        }

        if (m_decode_stop.load())
            break;

        m_frame_requested.store(false, std::memory_order_relaxed);

        auto container = m_container_ref.lock();
        if (container)
            pull_frame(container);
    }

    m_decode_active.store(false);
}

} // namespace MayaFlux::IO
