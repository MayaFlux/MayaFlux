#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

/**
 * @file SileroVadModel.hpp
 * @brief The Silero voice activity network run on the GPU, one probability per 512 sample chunk.
 */

namespace MayaFlux::Yantra {

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
