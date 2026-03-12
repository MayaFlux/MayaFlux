#pragma once

#include "MayaFlux/Kinesis/Discrete/Spectral.hpp"
#include "UniversalTransformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum SpectralOperation
 * @brief Spectral operations supported by SpectralTransformer
 */
enum class SpectralOperation : uint8_t {
    FREQUENCY_SHIFT, ///< Bandpass repositioning (approximate frequency shift)
    PITCH_SHIFT, ///< Phase-vocoder pitch shift, duration preserved
    SPECTRAL_FILTER, ///< Hard bandpass filter
    HARMONIC_ENHANCE, ///< Linear spectral tilt
    SPECTRAL_GATE, ///< Hard magnitude gate
};

/**
 * @class SpectralTransformer
 * @brief Concrete transformer for frequency-domain operations
 *
 * Handles transformations in the spectral domain:
 * - Spectral filtering and shaping
 * - Pitch shifting and harmonics
 * - Spectral morphing and cross-synthesis
 * - Frequency analysis and manipulation
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API SpectralTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Constructs a SpectralTransformer with specified operation
     * @param op The spectral operation to perform (default: FREQUENCY_SHIFT)
     */
    explicit SpectralTransformer(SpectralOperation op = SpectralOperation::FREQUENCY_SHIFT)
        : m_operation(op)
    {
        set_default_parameters();
    }

    /**
     * @brief Gets the transformation type
     * @return TransformationType::SPECTRAL
     */
    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::SPECTRAL;
    }

    /**
     * @brief Gets the transformer name including the operation type
     * @return String representation of the transformer name
     */
    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("SpectralTransformer_").append(Reflect::enum_to_string(m_operation));
    }

protected:
    /**
     * @brief Applies the configured spectral operation
     * @param input Input data
     * @return Transformed output
     *
     * Extracts per-channel double spans, calls the corresponding
     * Kinesis::Discrete::Spectral primitive on each channel, then
     * reconstructs the typed output via OperationHelper.
     *
     * FREQUENCY_SHIFT is implemented as a bandpass repositioning matching
     * the prior SpectralHelper behaviour. True single-sideband frequency
     * shift (complex exponential modulation) is a separate primitive.
     */
    output_type transform_implementation(input_type& input) override
    {
        switch (m_operation) {

        case SpectralOperation::FREQUENCY_SHIFT: {
            const auto shift_hz = get_parameter_or<double>("shift_hz", 0.0);
            const auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            const auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            const auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);
            const double lo = std::max(0.0, shift_hz);
            const double hi = sample_rate / 2.0 + shift_hz;

            return apply_per_channel(input,
                [lo, hi, sample_rate, window_size, hop_size](std::span<double> ch) {
                    return D::spectral_filter(ch, lo, hi, sample_rate, window_size, hop_size);
                });
        }

        case SpectralOperation::PITCH_SHIFT: {
            const auto pitch_ratio = get_parameter_or<double>("pitch_ratio", 1.0);
            const auto window_size = get_parameter_or<uint32_t>("window_size", 2048);
            const auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            const double semitones = 12.0 * std::log2(pitch_ratio);

            return apply_per_channel(input,
                [semitones, window_size, hop_size](std::span<double> ch) {
                    return D::pitch_shift(ch, semitones, window_size, hop_size);
                });
        }

        case SpectralOperation::SPECTRAL_FILTER: {
            const auto lo_freq = get_parameter_or<double>("low_freq", 20.0);
            const auto hi_freq = get_parameter_or<double>("high_freq", 20000.0);
            const auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);
            const auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            const auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            return apply_per_channel(input,
                [lo_freq, hi_freq, sample_rate, window_size, hop_size](std::span<double> ch) {
                    return D::spectral_filter(ch, lo_freq, hi_freq, sample_rate, window_size, hop_size);
                });
        }

        case SpectralOperation::HARMONIC_ENHANCE: {
            const auto factor = get_parameter_or<double>("enhancement_factor", 2.0);
            const auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            const auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            return apply_per_channel(input,
                [factor, window_size, hop_size](std::span<double> ch) {
                    return D::harmonic_enhance(ch, factor, window_size, hop_size);
                });
        }

        case SpectralOperation::SPECTRAL_GATE: {
            const auto threshold = get_parameter_or<double>("threshold", -40.0);
            const auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            const auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            return apply_per_channel(input,
                [threshold, window_size, hop_size](std::span<double> ch) {
                    return D::spectral_gate(ch, threshold, window_size, hop_size);
                });
        }

        default:
            return create_output(input);
        }
    }

    /**
     * @brief Sets transformation parameters
     * @param name Parameter name
     * @param value Parameter value
     *
     * Handles setting of spectral operation type and delegates other parameters to base class.
     * Supports both enum and string values for the "operation" parameter.
     *
     * Common parameters include:
     * - "shift_hz": Frequency shift in Hz
     * - "pitch_ratio": Pitch scaling factor (1.0 = no change)
     * - "low_freq", "high_freq": Filter frequency bounds
     * - "enhancement_factor": Harmonic enhancement strength
     * - "threshold": Spectral gate threshold in dB
     * - "window_size", "hop_size": FFT processing parameters
     */
    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto r = safe_any_cast<SpectralOperation>(value)) {
                m_operation = *r.value;
                return;
            }
            if (auto r = safe_any_cast<std::string>(value)) {
                if (auto e = Reflect::string_to_enum_case_insensitive<SpectralOperation>(*r.value)) {
                    m_operation = *e;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    SpectralOperation m_operation; ///< Current spectral operation
    mutable std::vector<std::vector<double>> m_working_buffer; ///< Buffer for out-of-place spectral operations

    /**
     * @brief Extracts per-channel spans, applies func to each, and reconstructs
     * @tparam Func Callable matching std::vector<double>(std::span<double>)
     */
    template <typename Func>
    output_type apply_per_channel(input_type& input, Func&& func)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(input);
        m_working_buffer.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i)
            m_working_buffer[i] = func(channels[i]);
        return create_output(
            OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
    }

    /**
     * @brief Sets default parameter values for all spectral operations
     *
     * Initializes all possible parameters with sensible defaults to ensure
     * the transformer works correctly regardless of the selected operation.
     * Default values are chosen for typical audio processing scenarios.
     */
    void set_default_parameters()
    {
        this->set_parameter("shift_hz", 0.0);
        this->set_parameter("pitch_ratio", 1.0);
        this->set_parameter("low_freq", 20.0);
        this->set_parameter("high_freq", 20000.0);
        this->set_parameter("enhancement_factor", 2.0);
        this->set_parameter("threshold", -40.0);
        this->set_parameter("time_stretch", 1.0);
        this->set_parameter("window_size", uint32_t { 1024 });
        this->set_parameter("hop_size", uint32_t { 512 });
    }

    /**
     * @brief Gets a parameter value with fallback to default
     * @tparam T Parameter type
     * @param name Parameter name
     * @param default_value Default value if parameter not found or wrong type
     * @return Parameter value or default
     */
    template <typename T>
    T get_parameter_or(const std::string& name, const T& default_value) const
    {
        auto param = this->get_transformation_parameter(name);
        if (!param.has_value())
            return default_value;

        auto result = safe_any_cast<T>(param);
        return result.value_or(default_value);
    }

    /**
     * @brief Creates output with proper type conversion
     * @param data Input data to convert
     * @return Output with converted data type
     *
     * Handles type conversion between InputType and OutputType when necessary,
     * or direct assignment when types match. Ensures spectral processing results
     * are properly converted back to the expected output type.
     */
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
