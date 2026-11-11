#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

/**
 * @file HubertPhonemeModel.hpp
 * @brief A HuBERT base network with a phoneme head run on the GPU, one phoneme distribution per 20 ms frame.
 */

namespace MayaFlux::Yantra {

class GpuDispatchCore;
class GpuDispatchRecorder;

/**
 * @class HubertPhonemeModel
 * @brief HuBERT base fine tuned for English phonemes at 16 kHz: an utterance in, a distribution over phonemes per frame out.
 *
 * Built from the 212 tensors of hubert_base_phoneme_en.safetensors. Seven
 * convolutions turn the waveform into one 512 wide feature every 320 samples,
 * each seeing 400 samples. A convolution over 128 frames adds position, twelve
 * attention layers let every frame read every other frame of the utterance,
 * and a final layer gives 43 scores per frame: 39 ARPABET phonemes, an unknown
 * class, the blank class and two unused markers.
 *
 * The network was trained to emit each phoneme once, on one frame, and the
 * blank class on the frames between, so the distribution says which phoneme is
 * recognised near a frame, not where it starts or ends. The 768 wide state
 * before the final layer is kept as well: frames that sound alike in context
 * lie close together in it.
 *
 * One call is one utterance. Every frame depends on the whole call, the input
 * is brought to zero mean and unit variance over the call, and nothing is
 * carried to the next call. An utterance is at most max_samples long. It runs
 * on native float and agrees with a double reference only to float precision.
 * Trained on read English speech; it must not run on the audio thread.
 */
class MAYAFLUX_API HubertPhonemeModel {
public:
    /** Sample rate in Hz the network was trained and runs at. */
    static constexpr uint32_t sample_rate = 16000;
    /** Samples between the starts of successive frames. */
    static constexpr uint32_t frame_hop = 320;
    /** Samples one frame's features are computed from. */
    static constexpr uint32_t frame_span = 400;
    /** Scores per frame. */
    static constexpr uint32_t classes = 43;
    /** Classes 0 to phonemes - 1 are phonemes. */
    static constexpr uint32_t phonemes = 39;
    /** Class the network emits between phonemes. */
    static constexpr uint32_t blank_class = 40;
    /** Width of the state before the final layer. */
    static constexpr uint32_t hidden_width = 768;
    /** Longest utterance one call accepts, 30 s. */
    static constexpr size_t max_samples = 480000;

    /**
     * @brief Check that a tensor set holds the weights the network needs.
     * @param weights Candidate weights.
     * @return Empty when usable, otherwise the first missing tensor or shape mismatch.
     */
    [[nodiscard]] static std::string diagnose(const Kakshya::TensorSet& weights);

    /**
     * @brief Name of a class: the ARPABET symbol of a phoneme, or a bracketed name for the others.
     * @param index Class, 0 to classes - 1. Anything else gives an empty name.
     */
    [[nodiscard]] static std::string_view label(uint32_t index) noexcept;

    /** @brief Whether a class is a vowel phoneme. Diphthongs and the r coloured vowel count. */
    [[nodiscard]] static bool is_vowel(uint32_t index) noexcept;

    /** @brief Frames an utterance of this many samples gives, 0 below frame_span samples. */
    [[nodiscard]] static uint32_t frame_count(size_t samples) noexcept;

    /**
     * @brief Upload the weights and prepare the GPU resources.
     * @param weights A set that diagnose accepts. Other element types than float32 are converted.
     * @throws std::invalid_argument When diagnose rejects the set, raised before any device is touched, or a tensor cannot become float32.
     * @throws std::runtime_error When no device is available.
     */
    explicit HubertPhonemeModel(const Kakshya::TensorSet& weights);

    /**
     * @brief As above, appending to a recording the caller shares with other work.
     * @param weights A set that diagnose accepts.
     * @param recording Recording the model records into, compute isolated. A null recording gives the model its own.
     */
    HubertPhonemeModel(const Kakshya::TensorSet& weights, std::shared_ptr<GpuDispatchRecorder> recording);
    ~HubertPhonemeModel();

    HubertPhonemeModel(const HubertPhonemeModel&) = delete;
    HubertPhonemeModel& operator=(const HubertPhonemeModel&) = delete;
    HubertPhonemeModel(HubertPhonemeModel&&) = delete;
    HubertPhonemeModel& operator=(HubertPhonemeModel&&) = delete;

    /**
     * @brief Run one utterance.
     * @param samples Mono samples at 16 kHz.
     * @return The distribution of every frame in turn, frame_count(samples.size()) * classes values, each frame summing to 1. Empty below frame_span samples.
     * @throws std::invalid_argument When the utterance is longer than max_samples.
     */
    [[nodiscard]] std::vector<float> process(std::span<const float> samples);

    /**
     * @brief The state before the final layer for every frame of the latest utterance.
     * @return resident_frames() * hidden_width values, frame after frame.
     */
    [[nodiscard]] std::vector<float> hidden_states();

    /** @brief Frames the latest utterance left on the device. */
    [[nodiscard]] size_t resident_frames() const noexcept;

    /**
     * @brief Where a result of the latest utterance is on the device.
     *
     * The buffer holds float32 values, one row of resident_frames() values
     * per class or per state channel, and is overwritten by the next call. A
     * consumer on the same recording imports it with import_shared. It follows
     * the model when the buffer grows.
     */
    struct DeviceBuffer {
        const GpuDispatchCore* context { nullptr };
        uint32_t set { 0 };
        size_t binding { 0 };
    };

    /** @brief The device buffer of the distributions, classes rows of frames. It exists from this call on. */
    [[nodiscard]] DeviceBuffer posterior_buffer();

    /** @brief The device buffer of the states, hidden_width rows of frames. It exists from this call on. */
    [[nodiscard]] DeviceBuffer hidden_buffer();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace MayaFlux::Yantra
