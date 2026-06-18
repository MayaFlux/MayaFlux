#include "VideoFileWriter.hpp"

#include "FFmpegMuxContext.hpp"
#include "VideoEncodeContext.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace MayaFlux::IO {

namespace {

    AVPixelFormat vk_format_to_avpixfmt(uint32_t vk_fmt_uint)
    {
        switch (static_cast<vk::Format>(vk_fmt_uint)) {
        case vk::Format::eR8G8B8A8Unorm:
        case vk::Format::eR8G8B8A8Srgb:
            return AV_PIX_FMT_RGBA;
        case vk::Format::eB8G8R8A8Unorm:
        case vk::Format::eB8G8R8A8Srgb:
            return AV_PIX_FMT_BGRA;
        case vk::Format::eR16G16B16A16Sfloat:
            return AV_PIX_FMT_RGBA64LE;
        default:
            return AV_PIX_FMT_BGRA;
        }
    }

    AVPixelFormat image_format_to_avpixfmt(Portal::Graphics::ImageFormat fmt)
    {
        using F = Portal::Graphics::ImageFormat;
        switch (fmt) {
        case F::RGBA8:
        case F::RGBA8_SRGB:
            return AV_PIX_FMT_RGBA;
        case F::BGRA8:
        case F::BGRA8_SRGB:
            return AV_PIX_FMT_BGRA;
        case F::RGB8:
            return AV_PIX_FMT_RGB24;
        case F::RGBA16F:
        case F::RGBA16:
            return AV_PIX_FMT_RGBA64LE;
        case F::RGBA32F:
            return AV_PIX_FMT_RGBAF32LE;
        default:
            return AV_PIX_FMT_NONE;
        }
    }

} // namespace

// =========================================================================
// Constructor / destructor
// =========================================================================

VideoFileWriter::VideoFileWriter()
    : m_queue(std::make_unique<Memory::LockFreeQueue<WorkItem, k_queue_capacity>>())
{
}

VideoFileWriter::~VideoFileWriter()
{
    if (m_observer_id.load(std::memory_order_acquire) != 0) {
        stop_recording().get();
    } else if (m_open.load(std::memory_order_acquire) && !m_closing.exchange(true)) {
        auto fut = close();
        if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "VideoFileWriter destructor timed out; worker detached, file may be incomplete");
            m_worker.detach();
            return;
        }
    }
    if (m_worker.joinable())
        m_worker.join();
}

// =========================================================================
// Screen capture — untouched, works
// =========================================================================

bool VideoFileWriter::record(const std::shared_ptr<Core::Window>& window,
    const std::string& filepath,
    double frame_rate,
    AVCodecID codec_id)
{
    if (!window) {
        set_error("record: null window");
        return false;
    }

    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::DisplayService>();
    if (!svc || !svc->register_frame_observer) {
        set_error("record: DisplayService unavailable");
        return false;
    }

    if (m_observer_id.load(std::memory_order_acquire) != 0)
        stop_recording().get();

    m_capture_filepath = filepath;
    m_capture_frame_rate = frame_rate;
    m_capture_codec_id = codec_id;
    m_capture_window = window;
    m_capture_opened.store(false, std::memory_order_release);

    if (!window->is_capture_enabled()) {
        window->set_capture_enabled(true);
        m_capture_did_enable = true;
    }

    auto handle = std::static_pointer_cast<void>(window);

    uint32_t obs_id = svc->register_frame_observer(handle,
        [this](const std::shared_ptr<std::vector<uint8_t>>& buf,
            uint32_t w, uint32_t h, uint32_t vk_fmt) {
            if (!buf || buf->empty())
                return;

            if (!m_capture_opened.exchange(true, std::memory_order_acq_rel)) {
                const AVPixelFormat av_fmt = vk_format_to_avpixfmt(vk_fmt);
                if (!open(m_capture_filepath, w, h,
                        m_capture_frame_rate, av_fmt, m_capture_codec_id)) {
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "[VideoFileWriter] record: failed to open encoder for "
                        "'{}': {}",
                        m_capture_filepath, last_error());
                    m_capture_opened.store(false, std::memory_order_release);
                    return;
                }
            }

            if (!m_open.load(std::memory_order_acquire))
                return;

            post(RawFrame {
                .pixels = std::vector<uint8_t>(buf->begin(), buf->end()),
                .width = w,
                .height = h });
        });

    if (obs_id == 0) {
        set_error("record: register_frame_observer returned 0 — "
                  "capture not yet active for this window");
        if (m_capture_did_enable) {
            window->set_capture_enabled(false);
            m_capture_did_enable = false;
        }
        m_capture_window.reset();
        return false;
    }

    m_observer_id.store(obs_id, std::memory_order_release);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoFileWriter] record: observer {} registered for '{}' -> '{}'",
        obs_id, window->get_create_info().title, filepath);

    return true;
}

std::future<bool> VideoFileWriter::stop_recording()
{
    const uint32_t obs_id = m_observer_id.exchange(0, std::memory_order_acq_rel);

    if (obs_id != 0) {
        auto* svc = Registry::BackendRegistry::instance()
                        .get_service<Registry::Service::DisplayService>();
        if (svc && svc->unregister_frame_observer && m_capture_window) {
            svc->unregister_frame_observer(
                std::static_pointer_cast<void>(m_capture_window), obs_id);
        }

        if (m_capture_did_enable && m_capture_window) {
            m_capture_window->set_capture_enabled(false);
            m_capture_did_enable = false;
        }

        m_capture_window.reset();
    }

    if (m_open.load(std::memory_order_acquire))
        return close();

    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
}

// =========================================================================
// Lifecycle
// =========================================================================

bool VideoFileWriter::open(const std::string& filepath,
    uint32_t width,
    uint32_t height,
    double frame_rate,
    AVPixelFormat src_pixel_format,
    AVCodecID explicit_codec)
{
    if (m_open.load(std::memory_order_acquire)) {
        set_error("open() called while already open");
        return false;
    }

    m_width = width;
    m_height = height;
    m_src_fmt = src_pixel_format;
    m_close_promise = std::promise<bool> {};
    m_closing.store(false, std::memory_order_release);

    m_worker = std::thread(&VideoFileWriter::worker_loop, this,
        filepath, width, height, frame_rate, src_pixel_format, explicit_codec);

    constexpr int k_spin_ms = 500;
    constexpr int k_sleep_us = 500;
    for (int i = 0; i < (k_spin_ms * 1000 / k_sleep_us); ++i) {
        if (m_open.load(std::memory_order_acquire))
            return true;
        std::this_thread::sleep_for(std::chrono::microseconds(k_sleep_us));
    }

    if (m_worker.joinable())
        m_worker.join();
    return false;
}

std::future<bool> VideoFileWriter::close()
{
    if (!m_closing.exchange(true))
        post(CloseCmd {});
    return m_close_promise.get_future();
}

// =========================================================================
// Write — raw pixels (capture path lands here; m_width/m_height from open())
// =========================================================================

void VideoFileWriter::write(const uint8_t* pixels, size_t size)
{
    if (!m_open.load(std::memory_order_acquire) || !pixels || size == 0)
        return;
    post(RawFrame {
        .pixels = std::vector<uint8_t>(pixels, pixels + size),
        .width = m_width,
        .height = m_height });
}

void VideoFileWriter::write(std::span<const uint8_t> pixels)
{
    if (!m_open.load(std::memory_order_acquire) || pixels.empty())
        return;
    post(RawFrame {
        .pixels = std::vector<uint8_t>(pixels.begin(), pixels.end()),
        .width = m_width,
        .height = m_height });
}

// =========================================================================
// Write — TextureContainer
// =========================================================================

void VideoFileWriter::write(const std::shared_ptr<Kakshya::TextureContainer>& container,
    uint32_t layer)
{
    if (!m_open.load(std::memory_order_acquire) || !container)
        return;

    auto span = container->pixel_bytes(layer);
    if (span.empty()) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "VideoFileWriter::write(TextureContainer): pixel_bytes empty for layer {}",
            layer);
        return;
    }

    post(RawFrame {
        .pixels = std::vector<uint8_t>(span.begin(), span.end()),
        .width = container->get_width(),
        .height = container->get_height() });
}

// =========================================================================
// Write — VideoStreamContainer / CameraContainer
// =========================================================================

void VideoFileWriter::write(const std::shared_ptr<Kakshya::VideoStreamContainer>& container,
    uint64_t frame_index)
{
    if (!m_open.load(std::memory_order_acquire) || !container)
        return;

    auto span = container->get_frame_pixels(frame_index);
    if (span.empty()) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "VideoFileWriter::write(VideoStreamContainer): get_frame_pixels({}) returned empty",
            frame_index);
        return;
    }

    post(RawFrame {
        .pixels = std::vector<uint8_t>(span.begin(), span.end()),
        .width = container->get_width(),
        .height = container->get_height() });
}

// =========================================================================
// Write — TextureBuffer
// =========================================================================

void VideoFileWriter::write(const std::shared_ptr<Buffers::TextureBuffer>& buffer)
{
    if (!m_open.load(std::memory_order_acquire) || !buffer)
        return;

    if (buffer->get_pixel_data().empty() && !buffer->has_texture()) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "VideoFileWriter::write(TextureBuffer): no CPU pixels and no GPU texture");
        return;
    }

    post(DownloadCmd { .buffer = buffer });
}

// =========================================================================
// Error / post
// =========================================================================

std::string VideoFileWriter::last_error() const
{
    std::lock_guard lock(m_error_mutex);
    return m_last_error;
}

void VideoFileWriter::set_error(std::string msg)
{
    std::lock_guard lock(m_error_mutex);
    m_last_error = std::move(msg);
}

bool VideoFileWriter::post(const WorkItem& item)
{
    return m_queue->push(item);
}

// =========================================================================
// Worker loop
// =========================================================================

void VideoFileWriter::worker_loop(const std::string& filepath,
    uint32_t width,
    uint32_t height,
    double frame_rate,
    AVPixelFormat src_fmt,
    AVCodecID codec_id)
{
    FFmpegMuxContext mux;
    VideoEncodeContext enc;

    auto fail = [&](std::string msg) {
        set_error(std::move(msg));
        m_open.store(false, std::memory_order_release);
        m_close_promise.set_value(false);
    };

    if (!mux.open(filepath)) {
        fail(mux.last_error());
        return;
    }
    if (!enc.open(mux, width, height, frame_rate, src_fmt, codec_id)) {
        fail(enc.last_error());
        return;
    }
    if (!mux.write_header()) {
        fail(mux.last_error());
        return;
    }

    m_open.store(true, std::memory_order_release);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoFileWriter] worker started: '{}' {}x{} @{:.3f}fps",
        filepath, width, height, frame_rate);

    while (true) {
        auto item_opt = m_queue->pop();
        if (!item_opt) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        bool done = std::visit([&](auto& cmd) -> bool {
            using T = std::decay_t<decltype(cmd)>;

            if constexpr (std::is_same_v<T, RawFrame>) {
                if (!enc.encode_frame(cmd.pixels.data(), cmd.pixels.size(),
                        cmd.width, cmd.height, mux)) {
                    set_error(enc.last_error());
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "[VideoFileWriter] encode_frame failed: {}", enc.last_error());
                }
                return false;
            }

            if constexpr (std::is_same_v<T, DownloadCmd>) {
                const auto img_fmt = cmd.buffer->get_format();
                const AVPixelFormat av_fmt = image_format_to_avpixfmt(img_fmt);
                if (av_fmt == AV_PIX_FMT_NONE) {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "[VideoFileWriter] DownloadCmd: unsupported ImageFormat {}",
                        static_cast<int>(img_fmt));
                    return false;
                }

                const auto& cpu = cmd.buffer->get_pixel_data();
                if (!cpu.empty()) {
                    if (!enc.encode_frame(cpu.data(), cpu.size(),
                            cmd.buffer->get_width(), cmd.buffer->get_height(), mux)) {
                        set_error(enc.last_error());
                        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                            "[VideoFileWriter] encode_frame (cpu) failed: {}", enc.last_error());
                    }
                    return false;
                }

                auto tex = cmd.buffer->get_texture();
                if (!tex) {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "[VideoFileWriter] DownloadCmd: no CPU pixels and no GPU texture");
                    return false;
                }

                using Portal::Graphics::TextureLoom;
                const size_t mip0_bytes = static_cast<size_t>(tex->get_width())
                    * tex->get_height()
                    * TextureLoom::get_bytes_per_pixel(img_fmt);

                if (mip0_bytes == 0)
                    return false;

                std::vector<uint8_t> pixels(mip0_bytes);

                if (!m_staging_buffer) {
                    m_staging_buffer = Buffers::create_image_staging_buffer(mip0_bytes);
                }

                TextureLoom::instance().download_data(tex, pixels.data(), mip0_bytes, m_staging_buffer, true);

                if (!enc.encode_frame(pixels.data(), pixels.size(),
                        tex->get_width(), tex->get_height(), mux)) {
                    set_error(enc.last_error());
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "[VideoFileWriter] encode_frame (gpu) failed: {}", enc.last_error());
                }
                return false;
            }

            return static_cast<bool>(std::is_same_v<T, CloseCmd>);
        },
            *item_opt);

        if (done)
            break;
    }

    bool ok = enc.drain(mux);
    if (!ok) {
        set_error(enc.last_error());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "[VideoFileWriter] drain failed: {}", enc.last_error());
    }

    mux.close();
    m_open.store(false, std::memory_order_release);
    m_close_promise.set_value(ok);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "[VideoFileWriter] worker finished: '{}' status={}",
        filepath, ok ? "ok" : "error");
}

} // namespace MayaFlux::IO
