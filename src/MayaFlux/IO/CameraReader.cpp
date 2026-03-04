#include "CameraReader.hpp"

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

// std::once_flag CameraReader::s_avdevice_init;

CameraReader::CameraReader()
    : m_demux(std::make_shared<FFmpegDemuxContext>())
    , m_video(std::make_shared<VideoStreamContext>())
{
}

void CameraReader::setup_io_service(uint64_t reader_id)
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

CameraReader::~CameraReader()
{
    close();

    if (m_owns_service) {
        Registry::BackendRegistry::instance()
            .unregister_service<Registry::Service::IOService>();
    }
}

bool CameraReader::open(const CameraConfig& config)
{
    close();

    const std::string fmt_name = config.format_override.empty()
        ? std::string(CAMERA_FORMAT)
        : config.format_override;

    AVDictionary* opts = nullptr;

    std::string fps_str = std::to_string(static_cast<int>(config.target_fps));
    av_dict_set(&opts, "framerate", fps_str.c_str(), 0);

    std::string size_str = std::to_string(config.target_width)
        + "x"
        + std::to_string(config.target_height);
    av_dict_set(&opts, "video_size", size_str.c_str(), 0);

    if (!m_demux->open_device(config.device_name, fmt_name, &opts)) {
        m_last_error = "Device open failed: " + m_demux->last_error();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "CameraReader::open — {}", m_last_error);
        return false;
    }

    if (!m_video->open(*m_demux,
            config.target_width,
            config.target_height)) {
        m_last_error = "Video stream open failed: " + m_video->last_error();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "CameraReader::open — {}", m_last_error);
        m_demux->close();
        return false;
    }

    const size_t buf_bytes = static_cast<size_t>(m_video->out_width)
        * m_video->out_height * 4;
    m_sws_buf.resize(buf_bytes);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "CameraReader: opened '{}' via {} — {}x{} @{:.1f}fps",
        config.device_name, fmt_name,
        m_video->out_width, m_video->out_height, m_video->frame_rate);

    return true;
}

void CameraReader::close()
{
    stop_decode_thread();

    std::unique_lock lock(m_ctx_mutex);
    m_video->close();
    m_demux->close();
    m_sws_buf.clear();
    m_last_error.clear();
}

bool CameraReader::is_open() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_demux->is_open() && m_video->is_valid();
}

std::shared_ptr<Kakshya::CameraContainer>
CameraReader::create_container() const
{
    std::shared_lock lock(m_ctx_mutex);
    if (!m_video->is_valid()) {
        m_last_error = "Cannot create container: reader not open";
        return nullptr;
    }

    return std::make_shared<Kakshya::CameraContainer>(
        m_video->out_width,
        m_video->out_height,
        4,
        m_video->frame_rate);
}

bool CameraReader::pull_frame(
    const std::shared_ptr<Kakshya::CameraContainer>& container)
{
    if (!container)
        return false;

    std::shared_lock lock(m_ctx_mutex);
    if (!m_video->is_valid())
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

        uint8_t* dst_data[1] = { dest };
        int dst_linesize[1] = { m_video->out_linesize };

        sws_scale(m_video->sws_context,
            frame->data, frame->linesize,
            0, static_cast<int>(m_video->height),
            dst_data, dst_linesize);

        container->mark_ready_for_processing(true);
        got_frame = true;
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    return got_frame;
}

uint32_t CameraReader::width() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->out_width;
}

uint32_t CameraReader::height() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->out_height;
}

double CameraReader::frame_rate() const
{
    std::shared_lock lock(m_ctx_mutex);
    return m_video->frame_rate;
}

void CameraReader::set_container(
    const std::shared_ptr<Kakshya::CameraContainer>& container)
{
    m_container_ref = container;
    start_decode_thread();
}

void CameraReader::pull_frame_all()
{
    {
        std::lock_guard lock(m_decode_mutex);
        m_frame_requested.store(true, std::memory_order_relaxed);
    }
    m_decode_cv.notify_one();
}

const std::string& CameraReader::last_error() const
{
    return m_last_error;
}

void CameraReader::start_decode_thread()
{
    stop_decode_thread();

    m_decode_stop.store(false);
    m_decode_active.store(true);
    m_decode_thread = std::thread(&CameraReader::decode_thread_func, this);
}

void CameraReader::stop_decode_thread()
{
    if (!m_decode_active.load())
        return;

    m_decode_stop.store(true);
    m_decode_cv.notify_all();

    if (m_decode_thread.joinable())
        m_decode_thread.join();

    m_decode_active.store(false);
}

void CameraReader::decode_thread_func()
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
