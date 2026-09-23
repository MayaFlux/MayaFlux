#pragma once

#include "CameraSource.hpp"
#include "FFmpegDemuxContext.hpp"
#include "VideoStreamContext.hpp"

#include <condition_variable>

namespace MayaFlux::Kakshya {
class CameraContainer;
}

namespace MayaFlux::Registry::Service {
class IOService;
}

namespace MayaFlux::IO {

/**
 * @brief Platform-specific FFmpeg input format string for camera devices.
 */
#if defined(MAYAFLUX_PLATFORM_LINUX)
inline constexpr std::string_view CAMERA_FORMAT = "v4l2";
#elif defined(MAYAFLUX_PLATFORM_MACOS)
inline constexpr std::string_view CAMERA_FORMAT = "avfoundation";
#elif defined(MAYAFLUX_PLATFORM_WINDOWS)
inline constexpr std::string_view CAMERA_FORMAT = "dshow";
#endif

/**
 * @struct CameraConfig
 * @brief Configuration for opening a camera device via FFmpeg.
 *
 * Platform device string conventions:
 *   - Linux:   "/dev/video0", "/dev/video1", …
 *   - macOS:   "0" (AVFoundation device index) or "FaceTime HD Camera"
 *   - Windows: "video=Integrated Camera" (DirectShow filter name)
 *
 * Resolution and frame rate values are hints passed to the device driver
 * via AVDictionary options. The device may negotiate different parameters;
 * the actual negotiated values are available from FFmpegCameraReader after
 * open().
 *
 * This type is FFmpeg-specific by design and does not appear anywhere in
 * CameraSource. Another backend defines its own config type entirely.
 */
struct MAYAFLUX_API CameraConfig {
    std::string device_name; ///< Platform device string.
    uint32_t target_width { 1920 }; ///< Requested width in pixels.
    uint32_t target_height { 1080 }; ///< Requested height in pixels.
    double target_fps { 30.0 }; ///< Hint only; device may ignore.
    std::string format_override; ///< Leave empty to use CAMERA_FORMAT for current platform.
    int pixel_format { -1 }; ///< Target AVPixelFormat as int; negative selects AV_PIX_FMT_RGBA.
};

/**
 * @class FFmpegCameraReader
 * @brief FFmpeg device reader for live camera input with background decode.
 *
 * Owns the FFmpeg demux and video codec contexts for a single camera device.
 * Decodes frames on a dedicated thread signalled by IOService::request_frame,
 * writing RGBA pixels directly into CameraContainer::mutable_frame_ptr() and
 * marking the container READY. The graphics thread is never blocked by device
 * I/O.
 *
 * Implements CameraSource for the post-open lifecycle; see CameraSource for
 * the shared contract (demand-driven single-frame pulls, Managed/Standalone
 * integration paths). open()/close() and CameraConfig are specific to this
 * backend — IOManager and CameraContainer only ever see this class through
 * the CameraSource interface once open.
 *
 * IOManager::open_camera() drives the Managed path and calls
 * avdevice_register_all() once via std::call_once. A Standalone caller is
 * responsible for that call itself before open().
 */
class MAYAFLUX_API FFmpegCameraReader : public CameraSource {
public:
    FFmpegCameraReader();
    ~FFmpegCameraReader() override;

    FFmpegCameraReader(const FFmpegCameraReader&) = delete;
    FFmpegCameraReader& operator=(const FFmpegCameraReader&) = delete;
    FFmpegCameraReader(FFmpegCameraReader&&) = delete;
    FFmpegCameraReader& operator=(FFmpegCameraReader&&) = delete;

    /**
     * @brief Open a camera device using the supplied config.
     * @param config Device name, resolution hint, fps hint, format override.
     * @return True on success.
     */
    [[nodiscard]] bool open(const CameraConfig& config);

    void close() override;
    [[nodiscard]] bool is_open() const override;

    [[nodiscard]] std::shared_ptr<Kakshya::CameraContainer> create_container() const override;

    bool pull_frame(const std::shared_ptr<Kakshya::CameraContainer>& container) override;

    [[nodiscard]] uint32_t width() const override;
    [[nodiscard]] uint32_t height() const override;
    [[nodiscard]] double frame_rate() const override;

    void set_container(const std::shared_ptr<Kakshya::CameraContainer>& container) override;
    void pull_frame_all() override;

    [[nodiscard]] const std::string& last_error() const override;

    /**
     * @brief Setup an IOService for this reader with the given reader_id.
     * @param reader_id Globally unique ID assigned to this reader.
     *
     * This is method is called when working outside of IOManager for self registration.
     * IOManager::open_camera() handles this automatically for managed readers.
     */
    void setup_io_service(uint64_t reader_id) override;

private:
    std::shared_ptr<FFmpegDemuxContext> m_demux;
    std::shared_ptr<VideoStreamContext> m_video;
    mutable std::shared_mutex m_ctx_mutex;
    std::vector<uint8_t> m_sws_buf;
    mutable std::string m_last_error;
    std::weak_ptr<Kakshya::CameraContainer> m_container_ref;

    std::shared_ptr<Registry::Service::IOService> m_standalone_service;
    uint64_t m_standalone_reader_id {};
    bool m_owns_service {};
    bool m_scaler_ready {};

    std::thread m_decode_thread;
    std::mutex m_decode_mutex;
    std::condition_variable m_decode_cv;
    std::atomic<bool> m_decode_stop { false };
    std::atomic<bool> m_decode_active { false };
    std::atomic<bool> m_frame_requested { false };

    void start_decode_thread();
    void stop_decode_thread();
    void decode_thread_func();

    /**
     * @brief Pixel format requested at open() time, as an AVPixelFormat int.
     *
     * Negative selects RGBA, matching VideoStreamContext::setup_scaler's
     * default. Read by create_container(), which runs before the scaler is
     * built and therefore cannot consult the negotiated format.
     */
    int m_requested_pixel_format { -1 };
};

} // namespace MayaFlux::IO
