#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

/**
 * @file SileroVadModel.hpp
 * @brief The Silero voice activity network run on the GPU, one probability per 512 sample chunk.
 */

namespace MayaFlux::Yantra {

class GpuDispatchCore;
class GpuDispatchRecorder;

/**
 * @struct StreamFraming
 * @brief How a stream classifier cuts its input: sample rate, chunk length, the context carried into each chunk, and the recurrent state width.
 */
struct StreamFraming {
    /** Sample rate in Hz the network runs at. */
    uint32_t sample_rate;
    /** Samples in one chunk, one output per chunk. */
    uint32_t chunk_samples;
    /** Samples before a chunk that are carried in as context. */
    uint32_t context_samples;
    /** Width of the recurrent state kept for each chunk. */
    uint32_t hidden_width;
};

/**
 * @class SileroVadModel
 * @brief Silero VAD at 16 kHz as a stream: samples in, one speech probability per chunk out.
 *
 * Built from the 15 tensors of silero_vad_16k.safetensors. Each chunk is 512
 * samples. The 64 samples before a chunk are prepended as context and the
 * frame is reflect padded by 64 on the right. A 258 channel strided
 * convolution is the transform, its two channel halves give a magnitude, four
 * convolutions with ReLU encode it, an LSTM cell carries the state from chunk
 * to chunk, and a ReLU, a 1x1 convolution and a sigmoid give the probability.
 *
 * Everything after the upload runs on the GPU in one submission per call. The
 * pad, transform and encoder do not depend on earlier chunks, so a call over
 * many chunks runs them as one batch and only the recurrence is sequential.
 * The recurrent state, the 64 sample context and a partial chunk are kept
 * between calls, so the model continues a stream. It runs on native float and
 * agrees with a double reference only to float precision. The result of a
 * chunk is available only after the chunk is complete, 32 ms of audio plus
 * the compute time, and the model must not run on the audio thread.
 */
class MAYAFLUX_API SileroVadModel {
public:
    static constexpr uint32_t chunk_samples = 512;
    static constexpr uint32_t context_samples = 64;
    /** Sample rate in Hz the network was trained and runs at. */
    static constexpr uint32_t sample_rate = 16000;
    /** Width of the recurrent state the network holds for each chunk. */
    static constexpr uint32_t hidden_width = 128;

    /** @brief The framing of this network. VadAnalyzer reads its window, hop and rate from it. */
    [[nodiscard]] static StreamFraming framing() noexcept;

    /**
     * @brief Check that a tensor set holds the weights the network needs.
     * @param weights Candidate weights.
     * @return Empty when usable, otherwise the first missing tensor or shape mismatch.
     */
    [[nodiscard]] static std::string diagnose(const Kakshya::TensorSet& weights);

    /**
     * @brief Upload the weights and prepare the GPU resources.
     * @param weights A set that diagnose accepts. Other element types than float32 are converted.
     * @throws std::invalid_argument When diagnose rejects the set, or a tensor cannot become float32. Raised before any device is touched.
     * @throws std::runtime_error When no device is available.
     */
    explicit SileroVadModel(const Kakshya::TensorSet& weights);

    /**
     * @brief As above, appending to a recording the caller shares with other work.
     *
     * With a shared recording the network and a consumer such as the interval
     * extractor go to the device in one submission. The recording must be
     * compute isolated.
     *
     * @param weights A set that diagnose accepts.
     * @param recording Recording the model records into. A null recording gives the model its own.
     */
    SileroVadModel(const Kakshya::TensorSet& weights, std::shared_ptr<GpuDispatchRecorder> recording);
    ~SileroVadModel();

    SileroVadModel(const SileroVadModel&) = delete;
    SileroVadModel& operator=(const SileroVadModel&) = delete;
    SileroVadModel(SileroVadModel&&) = delete;
    SileroVadModel& operator=(SileroVadModel&&) = delete;

    /**
     * @brief Append samples and run every chunk they complete.
     * @param samples Mono samples at 16 kHz, continuing the previous call.
     * @return One probability per completed chunk, in order. Empty while a chunk is incomplete.
     */
    [[nodiscard]] std::vector<float> process(std::span<const float> samples);

    /**
     * @brief Where the probabilities of the latest run are on the device.
     *
     * The buffer holds float32 probabilities, one per chunk, and is overwritten
     * by the next run. A consumer on the same recording imports it with
     * import_shared. It follows the model when the buffer grows.
     */
    struct ProbabilityBuffer {
        const GpuDispatchCore* context { nullptr };
        uint32_t set { 0 };
        size_t binding { 0 };
    };

    /** @brief The device buffer of the probabilities. It exists from this call on. */
    [[nodiscard]] ProbabilityBuffer probability_buffer();

    /**
     * @brief Run every chunk the samples complete, without waiting for the result.
     *
     * The probabilities stay on the device, in probability_buffer, and are
     * recorded into the model's recording; nothing is submitted. Submit the
     * recording, for instance by recording a consumer into it and submitting
     * that, then use resident_count for how many probabilities there are. At
     * most 4096 chunks run per call and the rest stay pending, so a consumer
     * must read the buffer before the next call.
     *
     * @param samples Mono samples at 16 kHz, continuing the previous call.
     * @return The number of chunks recorded.
     */
    [[nodiscard]] size_t record(std::span<const float> samples);

    /** @brief Probabilities the latest run left in probability_buffer. */
    [[nodiscard]] size_t resident_count() const noexcept;

    /**
     * @brief Where the recurrent output of the latest run is on the device.
     *
     * The buffer holds hidden_width float32 values per chunk, chunk after
     * chunk: what the network knows about the stream at each chunk, from
     * which its last layer derives the probability. It was trained only to
     * tell speech from other sound, so nothing else is claimed of it. It is
     * overwritten by the next run and follows the model when it grows.
     */
    [[nodiscard]] ProbabilityBuffer hidden_buffer();

    /**
     * @brief Keep the recurrent output of every chunk process and flush run, for take_hidden.
     * @param keep Off by default. Turning it off discards what was kept.
     */
    void keep_hidden(bool keep);

    /**
     * @brief The recurrent output kept since the last call, hidden_width values per chunk, chunk after chunk.
     * @return Empty unless keep_hidden is on. Chunks run by record are not kept.
     */
    [[nodiscard]] std::vector<float> take_hidden();

    /**
     * @brief Run the partial chunk, padded with zeros to a whole one.
     * @return One probability, or nothing when no samples are pending.
     */
    [[nodiscard]] std::vector<float> flush();

    /**
     * @brief Forget the recurrent state, the context and any pending samples.
     */
    void reset();

    /** @brief Samples received but not yet part of a completed chunk. */
    [[nodiscard]] size_t pending() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace MayaFlux::Yantra
