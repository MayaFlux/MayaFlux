#pragma once

#include "FFmpegMuxContext.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
struct AVAudioFifo;
struct AVStream;
struct SwrContext;
}

namespace MayaFlux::IO {

/**
 * @class AudioEncodeContext
 * @brief RAII owner of one audio stream's encoder, resampler, and sample FIFO.
 *
 * Encapsulates all audio-stream-specific FFmpeg objects on the write path:
 * - AVCodecContext for the selected or inferred encoder
 * - SwrContext converting AV_SAMPLE_FMT_DBL (engine canonical) to the encoder's
 *   native sample format
 * - AVAudioFifo absorbing arbitrary-size input chunks and emitting fixed-size
 *   frames required by block-aligned encoders (AAC: 1024, Vorbis: variable)
 * - AVStream registered inside the supplied FFmpegMuxContext
 *
 * Does NOT own AVFormatContext — that belongs to FFmpegMuxContext.
 *
 * Destruction order (enforced in destructor):
 *   fifo → swr_context → codec_context
 * The associated FFmpegMuxContext must outlive this object.
 *
 * Open sequence:
 *   1. FFmpegMuxContext::open()
 *   2. AudioEncodeContext::open()      ← adds stream, configures codec+swr+fifo
 *   3. FFmpegMuxContext::write_header()
 *   4. encode_frames() loop
 *   5. drain()                         ← flushes fifo remainder + encoder delay
 *   6. FFmpegMuxContext::close()       ← writes trailer
 */
class MAYAFLUX_API AudioEncodeContext {
public:
    AudioEncodeContext() = default;
    ~AudioEncodeContext();

    AudioEncodeContext(const AudioEncodeContext&) = delete;
    AudioEncodeContext& operator=(const AudioEncodeContext&) = delete;
    AudioEncodeContext(AudioEncodeContext&&) = delete;
    AudioEncodeContext& operator=(AudioEncodeContext&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Open the encoder and register an audio stream in the mux context.
     *
     * Selects the encoder: if codec_id is AV_CODEC_ID_NONE the container's
     * default audio codec is used (WAV → PCM_S16LE, MP4/MKV → AAC,
     * OGG → Vorbis, FLAC → FLAC). Allocates and opens AVCodecContext,
     * copies parameters to the new AVStream, initialises SwrContext
     * (AV_SAMPLE_FMT_DBL → encoder native format), and allocates AVAudioFifo
     * sized to two encoder frames.
     *
     * Must be called after FFmpegMuxContext::open() and before
     * FFmpegMuxContext::write_header().
     *
     * @param mux         Open mux context that will own the output stream.
     * @param sample_rate PCM sample rate in Hz.
     * @param channels    Number of interleaved channels.
     * @param codec_id    Encoder override; AV_CODEC_ID_NONE = container default.
     * @return True on success; false sets the internal error string.
     */
    bool open(FFmpegMuxContext& mux,
        uint32_t sample_rate,
        uint32_t channels,
        AVCodecID codec_id);

    /**
     * @brief Release all owned resources.
     *        Safe to call multiple times or on a context that never opened.
     */
    void close();

    /**
     * @brief True if codec, resampler, and FIFO are all ready.
     */
    [[nodiscard]] bool is_valid() const
    {
        return codec_context && swr_context && fifo && stream_index >= 0;
    }

    // =========================================================================
    // Encoding
    // =========================================================================

    /**
     * @brief Encode a block of interleaved double-precision PCM frames.
     *
     * Converts input via SwrContext, writes converted samples into the FIFO,
     * then drains the FIFO in encoder-frame-sized chunks. For encoders with
     * frame_size == 0 (PCM, some raw formats) the entire FIFO content is
     * submitted as one frame per call.
     *
     * Non-blocking: returns false only on a hard encode error, not on FIFO
     * underrun. Safe to call with any number of frames per call.
     *
     * @param interleaved Interleaved samples in double precision.
     * @param num_frames  Number of PCM frames (samples / channels).
     * @param mux         Mux context to receive encoded packets.
     * @return True on success.
     */
    bool encode_frames(std::span<const double> interleaved,
        uint32_t num_frames,
        FFmpegMuxContext& mux);

    /**
     * @brief Flush the FIFO remainder and encoder internal delay to the mux.
     *
     * Pads the final partial FIFO frame with silence if needed, sends it,
     * then sends a null frame to signal EOS to the encoder and drains all
     * buffered output packets. Call once before FFmpegMuxContext::close().
     *
     * @param mux Mux context to receive remaining packets.
     * @return True if all remaining packets were written successfully.
     */
    bool drain(FFmpegMuxContext& mux);

    // =========================================================================
    // Error
    // =========================================================================

    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    // =========================================================================
    // Owned handles
    // =========================================================================

    AVCodecContext* codec_context = nullptr; ///< Owned; freed in destructor.
    SwrContext* swr_context = nullptr; ///< Owned; freed in destructor.
    AVAudioFifo* fifo = nullptr; ///< Owned; freed in destructor.
    AVStream* stream = nullptr; ///< Owned by mux; pointer cached here.

    int stream_index = -1;
    uint32_t sample_rate = 0;
    uint32_t channels = 0;

private:
    int64_t m_pts = 0;
    std::string m_last_error;

    bool setup_resampler();
    bool setup_fifo();

    /**
     * @brief Pull one frame from the FIFO and send it to the encoder.
     *        If the FIFO has fewer samples than frame_size, pads with silence.
     *        Drains all output packets into the mux after sending.
     */
    bool send_fifo_frame(FFmpegMuxContext& mux, bool pad_to_frame_size);

    /**
     * @brief Drain all packets currently available from the encoder into mux.
     */
    bool drain_packets(FFmpegMuxContext& mux);
};

} // namespace MayaFlux::IO
