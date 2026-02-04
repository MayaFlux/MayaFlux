#pragma once

#include "UniversalTransformer.hpp"
#include "helpers/SpectralHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum SpectralOperation
 * @brief Specific spectral operations supported
 */
enum class SpectralOperation : uint8_t {
    FREQUENCY_SHIFT, ///< Shift entire spectrum
    PITCH_SHIFT, ///< Pitch-preserving shift
    SPECTRAL_FILTER, ///< Filter frequency bands
    HARMONIC_ENHANCE, ///< Enhance harmonics
    SPECTRAL_GATE, ///< Spectral gating
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
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

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
     * @brief Core transformation implementation for spectral operations
     * @param input Input data to transform in the frequency domain
     * @return Transformed output data
     *
     * Performs the spectral operation specified by m_operation on the input data.
     * Operations are performed using FFT/IFFT transforms with windowing for overlap-add processing.
     * Supports both in-place and out-of-place transformations.
     */
    output_type transform_implementation(input_type& input) override
    {
        switch (m_operation) {
        case SpectralOperation::FREQUENCY_SHIFT: {
            auto shift_hz = get_parameter_or<double>("shift_hz", 0.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);

            auto low_freq = std::max(0.0, shift_hz);
            auto high_freq = sample_rate / 2.0 + shift_hz;

            if (this->is_in_place()) {
                return create_output(transform_spectral_filter(input, low_freq, high_freq, sample_rate, window_size, hop_size));
            }
            return create_output(transform_spectral_filter(input, low_freq, high_freq, sample_rate, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::PITCH_SHIFT: {
            auto pitch_ratio = get_parameter_or<double>("pitch_ratio", 1.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            double semitones = 12.0 * std::log2(pitch_ratio);

            if (this->is_in_place()) {
                return create_output(transform_pitch_shift(input, semitones, window_size, hop_size));
            }
            return create_output(transform_pitch_shift(input, semitones, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::SPECTRAL_FILTER: {
            auto low_freq = get_parameter_or<double>("low_freq", 20.0);
            auto high_freq = get_parameter_or<double>("high_freq", 20000.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);

            if (this->is_in_place()) {
                return create_output(transform_spectral_filter(input, low_freq, high_freq, sample_rate, window_size, hop_size));
            }
            return create_output(transform_spectral_filter(input, low_freq, high_freq, sample_rate, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::HARMONIC_ENHANCE: {
            auto enhancement_factor = get_parameter_or<double>("enhancement_factor", 2.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            if (this->is_in_place()) {
                auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

                auto processor = [enhancement_factor](Eigen::VectorXcd& spectrum, size_t) {
                    for (int i = 1; i < spectrum.size() / 2; ++i) {
                        double freq_factor = 1.0 + (enhancement_factor - 1.0) * (double(i) / (spectrum.size() / 2));
                        spectrum[i] *= freq_factor;
                        spectrum[spectrum.size() - i] *= freq_factor;
                    }
                };

                for (auto& span : data_span) {
                    auto result = process_spectral_windows(span, window_size, hop_size, processor);
                    std::ranges::copy(result, span.begin());
                }

                auto reconstructed_data = data_span
                    | std::views::transform([](const auto& span) {
                          return std::vector<double>(span.begin(), span.end());
                      })
                    | std::ranges::to<std::vector>();

                return create_output(OperationHelper::reconstruct_from_double<InputType>(reconstructed_data, structure_info));
            }

            auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, m_working_buffer);
            auto processor = [enhancement_factor](Eigen::VectorXcd& spectrum, size_t) {
                for (int i = 1; i < spectrum.size() / 2; ++i) {
                    double freq_factor = 1.0 + (enhancement_factor - 1.0) * (double(i) / (spectrum.size() / 2));
                    spectrum[i] *= freq_factor;
                    spectrum[spectrum.size() - i] *= freq_factor;
                }
            };

            for (size_t i = 0; i < target_data.size(); ++i) {
                auto result = process_spectral_windows(target_data[i], window_size, hop_size, processor);
                m_working_buffer[i] = std::move(result);
            }

            return create_output(OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
        }

        case SpectralOperation::SPECTRAL_GATE: {
            auto threshold = get_parameter_or<double>("threshold", -40.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            double linear_threshold = std::pow(10.0, threshold / 20.0);

            if (this->is_in_place()) {
                auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

                auto processor = [linear_threshold](Eigen::VectorXcd& spectrum, size_t) {
                    std::ranges::transform(spectrum, spectrum.begin(), [linear_threshold](const std::complex<double>& bin) {
                        return (std::abs(bin) > linear_threshold) ? bin : std::complex<double>(0.0, 0.0);
                    });
                };

                for (auto& span : data_span) {
                    auto result = process_spectral_windows(span, window_size, hop_size, processor);
                    std::ranges::copy(result, span.begin());
                }

                auto reconstructed_data = data_span
                    | std::views::transform([](const auto& span) {
                          return std::vector<double>(span.begin(), span.end());
                      })
                    | std::ranges::to<std::vector>();

                return create_output(OperationHelper::reconstruct_from_double<InputType>(reconstructed_data, structure_info));
            }

            auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, m_working_buffer);
            auto processor = [linear_threshold](Eigen::VectorXcd& spectrum, size_t) {
                std::ranges::transform(spectrum, spectrum.begin(), [linear_threshold](const std::complex<double>& bin) {
                    return (std::abs(bin) > linear_threshold) ? bin : std::complex<double>(0.0, 0.0);
                });
            };

            for (size_t i = 0; i < target_data.size(); ++i) {
                auto result = process_spectral_windows(target_data[i], window_size, hop_size, processor);
                m_working_buffer[i] = std::move(result);
            }

            return create_output(OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
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
            if (auto op_result = safe_any_cast<SpectralOperation>(value)) {
                m_operation = *op_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                if (auto op_enum = Reflect::string_to_enum_case_insensitive<SpectralOperation>(*str_result.value)) {
                    m_operation = *op_enum;
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
