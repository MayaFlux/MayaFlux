#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include <future>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Kakshya {
class SoundStreamContainer;
}

namespace MayaFlux::IO {

/**
 * @class SoundFileWriter
 * @brief Asynchronous audio file encoder with a lock-free work queue.
 *
 * Accepts audio from multiple source types and encodes it to a file on a
 * dedicated worker thread. The public API is fully non-blocking: every write
 * call posts a work item to an SPSC queue and returns immediately. The worker
 * thread owns all FFmpeg state and is the only thread that touches it.
 *
 * Supported input paths:
 * - Raw interleaved double frames (span or vector, for RT/coroutine callers)
 * - AudioBuffer          — drains get_data() as interleaved doubles
 * - SoundStreamContainer — drains get_data_as_double() (interleaved span)
 *
 * Threading model:
 *   Caller thread  →  push work items to m_queue (lock-free)
 *   Worker thread  →  drain queue, encode via AudioEncodeContext + FFmpegMuxContext
 *
 * Lifetime:
 *   open() spawns the worker. close() posts a CloseCmd and returns a
 *   future<bool> the caller may wait on. The destructor issues close() if
 *   still open and joins with a 5-second timeout; on timeout it detaches and
 *   logs CRITICAL.
 *
 * Error handling:
 *   Encode errors set last_error() and cause the worker to post false to the
 *   close promise. Subsequent write calls after an encode error are silently
 *   dropped (is_open() returns false).
 */
class MAYAFLUX_API SoundFileWriter {
public:
    SoundFileWriter();
    ~SoundFileWriter();

    SoundFileWriter(const SoundFileWriter&) = delete;
    SoundFileWriter& operator=(const SoundFileWriter&) = delete;
    SoundFileWriter(SoundFileWriter&&) = delete;
    SoundFileWriter& operator=(SoundFileWriter&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open the output file and spawn the worker thread.
     *
     * Format is inferred from the filepath extension (wav, flac, ogg, mp4, mkv…).
     * Pass explicit_codec to override the container's default audio codec.
     *
     * @param filepath       Destination file path. Extension determines container.
     * @param channels       Number of interleaved channels in all submitted data.
     * @param sample_rate    PCM sample rate in Hz.
     * @param explicit_codec Encoder override; AV_CODEC_ID_NONE = container default.
     * @return True if the worker started successfully.
     */
    bool open(const std::string& filepath,
        uint32_t channels,
        uint32_t sample_rate,
        AVCodecID explicit_codec);

    /**
     * @brief Post a close command to the worker.
     *
     * The worker drains the FIFO, flushes the encoder, writes the container
     * trailer, and fulfils the returned future. The caller may co_await or
     * .get() the future, or discard it if completion is not needed.
     *
     * Safe to call multiple times; subsequent calls return a future that is
     * immediately ready with false.
     *
     * @return future<bool>: true = file complete; false = encode error.
     */
    std::future<bool> close();

    /**
     * @brief True if the worker is running and the encoder is healthy.
     */
    [[nodiscard]] bool is_open() const { return m_open.load(std::memory_order_acquire); }

    // =========================================================================
    // Write
    // =========================================================================

    /**
     * @brief Post interleaved double-precision frames to the work queue.
     *
     * @param interleaved Interleaved PCM frames in double precision.
     * @param num_frames  Frame count (samples / channels). If 0, inferred as
     *                    interleaved.size() / channels.
     */
    void write(std::span<const double> interleaved, uint32_t num_frames = 0);

    /**
     * @brief Post planar per-channel data to the work queue.
     *
     * Accepts the DataVariant vector directly from AudioOutputContainer::get_processed_data().
     * Interleaving is deferred to the worker thread.
     *
     * @param planar Per-channel vectors; each element must be vector<double>.
     */
    void write(const std::vector<Kakshya::DataVariant>& planar);

    /**
     * @brief Post one AudioBuffer's mono sample data to the work queue.
     */
    void write(const std::shared_ptr<Buffers::AudioBuffer>& buffer);

    /**
     * @brief Post a SoundStreamContainer's planar channel data to the work queue.
     *
     * Reads get_data() on the caller thread, copies, and posts as PlanarChunk.
     */
    void write(const std::shared_ptr<Kakshya::SoundStreamContainer>& container);

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] std::string last_error() const;

private:
    // -------------------------------------------------------------------------
    // Work item types
    // -------------------------------------------------------------------------

    struct FrameChunk {
        std::vector<double> samples;
        uint32_t num_frames;
    };

    struct PlanarChunk {
        std::vector<std::vector<double>> channels;
        uint32_t num_frames;
    };

    struct CloseCmd { };

    using WorkItem = std::variant<FrameChunk, PlanarChunk, CloseCmd>;

    // -------------------------------------------------------------------------
    // Queue
    // -------------------------------------------------------------------------

    static constexpr size_t k_queue_capacity = 4096;
    std::unique_ptr<Memory::LockFreeQueue<WorkItem, k_queue_capacity>> m_queue;

    // -------------------------------------------------------------------------
    // Worker
    // -------------------------------------------------------------------------

    std::thread m_worker;
    std::atomic<bool> m_open { false };
    std::atomic<bool> m_closing { false };
    std::promise<bool> m_close_promise;
    std::shared_future<bool> m_close_future;

    mutable std::mutex m_error_mutex;
    std::string m_last_error;

    uint32_t m_channels {};

    void worker_loop(const std::string& filepath, uint32_t channels, uint32_t sample_rate,
        AVCodecID codec_id);

    void set_error(std::string msg);
    bool post(const WorkItem& item);
};

} // namespace MayaFlux::IO
