#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Kinesis/Discrete/Spectral.hpp"
#include "Transformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum SpectralOperation
 * @brief Frequency-domain operations applied by SpectralTransformer
 */
enum class SpectralOperation : uint8_t {
    FREQUENCY_SHIFT, ///< Bandpass repositioning (approximate frequency shift)
    PITCH_SHIFT, ///< Phase-vocoder pitch shift, duration preserved
    SPECTRAL_FILTER, ///< Hard bandpass filter
    HARMONIC_ENHANCE, ///< Linear spectral tilt
    SPECTRAL_GATE, ///< Hard magnitude gate
};

/**
 * @struct SpectralTransformerConfig
 * @brief Settings of SpectralTransformer, stored whole in the operation's facets.
 */
struct SpectralTransformerConfig : TransformerConfig {
    /** Spectral operation to apply. */
    Reflect::EnumByte<SpectralOperation> operation { SpectralOperation::FREQUENCY_SHIFT };
    /** FREQUENCY_SHIFT: shift applied to the passband, in Hz. */
    double shift_hz { 0.0 };
    /** PITCH_SHIFT: frequency ratio, 2.0 is one octave up. */
    double pitch_ratio { 1.0 };
    /** SPECTRAL_FILTER: lower edge of the passband, in Hz. */
    double low_freq { 20.0 };
    /** SPECTRAL_FILTER: upper edge of the passband, in Hz. */
    double high_freq { 20000.0 };
    /** HARMONIC_ENHANCE: gain at Nyquist relative to DC. */
    double enhancement_factor { 2.0 };
    /** SPECTRAL_GATE: magnitude threshold in dB. */
    double threshold { -40.0 };
    /** Sample rate of the input, in Hz, used to map bins to frequencies. */
    double sample_rate { 48000.0 };
    /** STFT window length in samples. */
    uint32_t window_size { 1024 };
    /** STFT hop length in samples. */
    uint32_t hop_size { 512 };
};

MF_DESCRIBE(SpectralTransformerConfig)

/**
 * @class SpectralTransformer
 * @brief Applies one STFT-based operation to every channel, as set by its config.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class SpectralTransformer final : public Transformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using supports = Supports<SpectralTransformerConfig>;

    /**
     * @brief Construct a spectral transformer from its settings.
     * @param config Operation and its parameters.
     */
    explicit SpectralTransformer(SpectralTransformerConfig config = {})
    {
        this->set_config(std::move(config));
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::SPECTRAL;
    }

    /**
     * @brief The stored settings, or defaults when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

protected:
    output_type transform_implementation(input_type& input) override
    {
        const auto& cfg = this->template config<SpectralTransformerConfig>();
        const auto window = cfg.window_size;
        const auto hop = cfg.hop_size;
        const auto sample_rate = cfg.sample_rate;

        switch (static_cast<SpectralOperation>(cfg.operation)) {
        case SpectralOperation::FREQUENCY_SHIFT: {
            const double shift = cfg.shift_hz;
            const double lo = std::max(0.0, shift);
            const double hi = sample_rate / 2.0 + shift;
            return apply_per_channel(input,
                [lo, hi, sample_rate, window, hop](std::span<double> ch) {
                    return D::spectral_filter(ch, lo, hi, sample_rate, window, hop);
                });
        }
        case SpectralOperation::PITCH_SHIFT: {
            const double semitones = 12.0 * std::log2(cfg.pitch_ratio);
            return apply_per_channel(input,
                [semitones, window, hop](std::span<double> ch) {
                    return D::pitch_shift(ch, semitones, window, hop);
                });
        }
        case SpectralOperation::SPECTRAL_FILTER: {
            const double lo = cfg.low_freq;
            const double hi = cfg.high_freq;
            return apply_per_channel(input,
                [lo, hi, sample_rate, window, hop](std::span<double> ch) {
                    return D::spectral_filter(ch, lo, hi, sample_rate, window, hop);
                });
        }
        case SpectralOperation::HARMONIC_ENHANCE: {
            const double factor = cfg.enhancement_factor;
            return apply_per_channel(input,
                [factor, window, hop](std::span<double> ch) {
                    return D::harmonic_enhance(ch, factor, window, hop);
                });
        }
        case SpectralOperation::SPECTRAL_GATE: {
            const double threshold = cfg.threshold;
            return apply_per_channel(input,
                [threshold, window, hop](std::span<double> ch) {
                    return D::spectral_gate(ch, threshold, window, hop);
                });
        }
        default:
            return create_output(input);
        }
    }

private:
    mutable std::vector<std::vector<double>> m_working_buffer;

    template <typename Func>
    output_type apply_per_channel(input_type& input, Func&& func)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(input);
        m_working_buffer.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i)
            m_working_buffer[i] = func(channels[i]);
        if (this->is_in_place())
            for (size_t i = 0; i < channels.size(); ++i)
                std::ranges::copy(m_working_buffer[i], channels[i].begin());
        return create_output(
            OperationHelper::reconstruct_from_double<input_type>(m_working_buffer, structure_info));
    }

    output_type create_output(const input_type& input)
    {
        if constexpr (std::is_same_v<InputType, OutputType>) {
            return input;
        } else {
            auto [result_data, metadata] = OperationHelper::extract_structured_double(input);
            return this->convert_result(result_data, metadata);
        }
    }
};

}
#include "SpectralTransformer.describe.hpp"
