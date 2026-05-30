#pragma once

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include <future>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Kakshya {
class TextureContainer;
class VideoStreamContainer;
}

namespace MayaFlux::Buffers {
class TextureBuffer;
}

namespace MayaFlux::IO {

/**
 * @class VideoFileWriter
 * @brief Asynchronous video file encoder with a lock-free work queue.
 *
 * Accepts pixel data from multiple source types and encodes it to a file on a
 * dedicated worker thread. The public API is fully non-blocking: every write
 * call copies the pixel data, posts a work item to an SPSC queue, and returns
 * immediately. The worker thread owns all FFmpeg state and is the only thread
 * that touches it.
 *
 * Supported input paths:
 * - record() / stop_recording(): swapchain readback via DisplayService frame
 *   observer; encoder opened lazily on first frame.
 * - Raw packed uint8_t pixels (span): caller supplies dimensions matching open().
 * - TextureContainer: reads pixel_bytes(layer); dimensions from container.
 * - VideoStreamContainer / CameraContainer: reads get_frame_pixels(); always RGBA.
 * - TextureBuffer: CPU pixels via get_pixel_data() when present, otherwise
 *   GPU download via download_data_async() on the worker thread.
 *
 * Source dimensions may differ from the encoder output dimensions; SwsContext
 * rescales automatically. The format passed to open() must match the source
 * pixel layout for all write() calls on that instance.
 *
 * close() returns future<bool>; caller must .get() before process exit or the
 * container trailer will not be written.
 */
class MAYAFLUX_API VideoFileWriter {
public:
    VideoFileWriter();
    ~VideoFileWriter();

    VideoFileWriter(const VideoFileWriter&) = delete;
    VideoFileWriter& operator=(const VideoFileWriter&) = delete;
    VideoFileWriter(VideoFileWriter&&) = delete;
    VideoFileWriter& operator=(VideoFileWriter&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool open(const std::string& filepath,
        uint32_t width,
        uint32_t height,
        double frame_rate,
        AVPixelFormat src_pixel_format,
        AVCodecID explicit_codec = AV_CODEC_ID_NONE);

    std::future<bool> close();

    [[nodiscard]] bool is_open() const { return m_open.load(std::memory_order_acquire); }

    // =========================================================================
    // Screen capture
    // =========================================================================

    bool record(const std::shared_ptr<Core::Window>& window,
        const std::string& filepath,
        double frame_rate,
        AVCodecID codec_id = AV_CODEC_ID_NONE);

    std::future<bool> stop_recording();

    [[nodiscard]] bool is_recording() const
    {
        return m_observer_id.load(std::memory_order_acquire) != 0;
    }

    // =========================================================================
    // Write
    // =========================================================================

    void write(const uint8_t* pixels, size_t size);
    void write(std::span<const uint8_t> pixels);

    void write(const std::shared_ptr<Kakshya::TextureContainer>& container,
        uint32_t layer = 0);

    void write(const std::shared_ptr<Kakshya::VideoStreamContainer>& container,
        uint64_t frame_index = 0);

    void write(const std::shared_ptr<Buffers::TextureBuffer>& buffer);

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] std::string last_error() const;

private:
    struct RawFrame {
        std::vector<uint8_t> pixels;
        uint32_t width {};
        uint32_t height {};
    };

    struct DownloadCmd {
        std::shared_ptr<Buffers::TextureBuffer> buffer;
    };

    struct CloseCmd { };

    using WorkItem = std::variant<RawFrame, DownloadCmd, CloseCmd>;

    static constexpr size_t k_queue_capacity = 512;
    std::unique_ptr<Memory::LockFreeQueue<WorkItem, k_queue_capacity>> m_queue;

    std::thread m_worker;
    std::atomic<bool> m_open { false };
    std::atomic<bool> m_closing { false };
    std::promise<bool> m_close_promise;

    mutable std::mutex m_error_mutex;
    std::string m_last_error;

    uint32_t m_width {};
    uint32_t m_height {};
    AVPixelFormat m_src_fmt { AV_PIX_FMT_BGRA };

    std::shared_ptr<Core::Window> m_capture_window;
    std::atomic<uint32_t> m_observer_id { 0 };
    std::atomic<bool> m_capture_opened { false };
    std::string m_capture_filepath;
    double m_capture_frame_rate {};
    AVCodecID m_capture_codec_id { AV_CODEC_ID_NONE };
    bool m_capture_did_enable { false };

    void worker_loop(const std::string& filepath,
        uint32_t width,
        uint32_t height,
        double frame_rate,
        AVPixelFormat src_fmt,
        AVCodecID codec_id);

    void set_error(std::string msg);
    bool post(const WorkItem& item);
};

} // namespace MayaFlux::IO
