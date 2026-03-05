#pragma once

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
 * the actual negotiated values are available from CameraReader after open().
 */
struct MAYAFLUX_API CameraConfig {
    std::string device_name; ///< Platform device string.
    uint32_t target_width { 1920 }; ///< Requested width in pixels.
    uint32_t target_height { 1080 }; ///< Requested height in pixels.
    double target_fps { 30.0 }; ///< Hint only; device may ignore.
    std::string format_override; ///< Leave empty to use CAMERA_FORMAT for current platform.
};

/**
 * @class CameraReader
 * @brief FFmpeg device reader for live camera input with background decode.
 *
 * Owns the FFmpeg demux and video codec contexts for a single camera device.
 * Decodes frames on a dedicated thread signalled by IOService::request_frame,
 * writing RGBA pixels directly into CameraContainer::mutable_frame_ptr() and
 * marking the container READY. The graphics thread is never blocked by device
 * I/O.
 *
 * Two integration paths:
 *   - Managed:    IOManager::open_camera() handles registration, reader_id
 *                 assignment, container wiring, and avdevice initialisation.
 *   - Standalone: open() → create_container() → setup_io_service(id) →
 *                 set_container() → close(). Caller is responsible for
 *                 avdevice_register_all() before open().
 *
 * Unlike VideoFileReader there is no ring buffer, no seek, and no batch
 * decode — the device is a live unbounded source. One frame is pulled per
 * process cycle, demand-driven by CameraContainer::process_default().
 */
class MAYAFLUX_API CameraReader {
public:
    CameraReader();
    ~CameraReader();

    CameraReader(const CameraReader&) = delete;
    CameraReader& operator=(const CameraReader&) = delete;
    CameraReader(CameraReader&&) = delete;
    CameraReader& operator=(CameraReader&&) = delete;

    /**
     * @brief Open a camera device using the supplied config.
     * @param config Device name, resolution hint, fps hint, format override.
     * @return True on success.
     */
    [[nodiscard]] bool open(const CameraConfig& config);

    /**
     * @brief Release codec, demux, and scratch buffer resources.
     */
    void close();

    /**
     * @brief True if the device is open and codec is ready.
     */
    [[nodiscard]] bool is_open() const;

    /**
     * @brief Create a CameraContainer sized to the negotiated device resolution.
     * @return Initialised container with slot-0 allocated, ready for pull_frame().
     */
    [[nodiscard]] std::shared_ptr<Kakshya::CameraContainer> create_container() const;

    /**
     * @brief Decode one frame from the device into the container's m_data[0].
     *
     * Pumps packets until one decoded frame is available, converts to RGBA
     * via swscale into container->mutable_frame_ptr(), then calls
     * container->mark_ready_for_processing(true). Returns false on EAGAIN
     * (no frame yet) — not an error, just no data available this cycle.
     *
     * The caller is responsible for invoking this once per graphics cycle,
     * before the container's process_default() is triggered by downstream
     * consumers. Typical integration: register as a graphics pre_process_hook
     * or call explicitly before buffer processing.
     *
     * @param container Target CameraContainer.
     * @return True if a new frame was written, false if no frame was available.
     */
    bool pull_frame(const std::shared_ptr<Kakshya::CameraContainer>& container);

    /** @brief Negotiated output width in pixels. */
    [[nodiscard]] uint32_t width() const;

    /** @brief Negotiated output height in pixels. */
    [[nodiscard]] uint32_t height() const;

    /** @brief Negotiated frame rate in fps. */
    [[nodiscard]] double frame_rate() const;

    /**
     * @brief Store a weak reference to the container for IOService dispatch.
     *
     * Called by IOManager::open_camera() after create_container(). Enables
     * pull_frame_all() to resolve the container without an explicit argument.
     *
     * @param container CameraContainer created by this reader.
     */
    void set_container(const std::shared_ptr<Kakshya::CameraContainer>& container);

    /**
     * @brief Signal the background decode thread to pull one frame.
     *
     * Non-blocking. Called by IOManager::dispatch_frame_request() or the
     * standalone IOService lambda. The decode thread wakes, calls pull_frame(),
     * writes pixels into the container, marks it READY, then sleeps until the
     * next signal. Safe to call from any thread.
     */
    void pull_frame_all();

    /** @brief Last error string, empty if no error. */
    [[nodiscard]] const std::string& last_error() const;

    /**
     * @brief Setup an IOService for this reader with the given reader_id.
     * @param reader_id Globally unique ID assigned to this reader.
     *
     * This is method is called when working outside of IOManager for self registration.
     * IOManager::open_camera() handles this automatically for managed readers.
     */
    void setup_io_service(uint64_t reader_id);

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
};

} // namespace MayaFlux::IO
