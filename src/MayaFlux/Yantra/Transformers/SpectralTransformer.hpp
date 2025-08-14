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
    PHASE_VOCODER ///< Phase vocoder processing
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
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class SpectralTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    explicit SpectralTransformer(SpectralOperation op = SpectralOperation::FREQUENCY_SHIFT)
        : m_operation(op)
    {
        set_default_parameters();
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::SPECTRAL;
    }

    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("SpectralTransformer_").append(Utils::enum_to_string(m_operation));
    }

protected:
    output_type transform_implementation(input_type& input) override
    {
        auto& input_data = input.data;

        switch (m_operation) {
        case SpectralOperation::FREQUENCY_SHIFT: {
            auto shift_hz = get_parameter_or<double>("shift_hz", 0.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);

            auto low_freq = std::max(0.0, shift_hz);
            auto high_freq = sample_rate / 2.0 + shift_hz;

            if (this->is_in_place()) {
                return create_output(transform_spectral_filter(input_data, low_freq, high_freq, sample_rate, window_size, hop_size));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_spectral_filter(input_data, low_freq, high_freq, sample_rate, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::PITCH_SHIFT: {
            auto pitch_ratio = get_parameter_or<double>("pitch_ratio", 1.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            double semitones = 12.0 * std::log2(pitch_ratio);

            if (this->is_in_place()) {
                return create_output(transform_pitch_shift(input_data, semitones, window_size, hop_size));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_pitch_shift(input_data, semitones, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::SPECTRAL_FILTER: {
            auto low_freq = get_parameter_or<double>("low_freq", 20.0);
            auto high_freq = get_parameter_or<double>("high_freq", 20000.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);
            auto sample_rate = get_parameter_or<double>("sample_rate", 48000.0);

            if (this->is_in_place()) {
                return create_output(transform_spectral_filter(input_data, low_freq, high_freq, sample_rate, window_size, hop_size));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_spectral_filter(input_data, low_freq, high_freq, sample_rate, window_size, hop_size, m_working_buffer));
        }

        case SpectralOperation::HARMONIC_ENHANCE: {
            auto enhancement_factor = get_parameter_or<double>("enhancement_factor", 2.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            // Use custom spectral processing for harmonic enhancement
            if (this->is_in_place()) {
                auto [data_span, structure_info] = OperationHelper::extract_structured_double(input_data);

                auto processor = [enhancement_factor](Eigen::VectorXcd& spectrum, size_t) {
                    for (int i = 1; i < spectrum.size() / 2; ++i) {
                        double freq_factor = 1.0 + (enhancement_factor - 1.0) * (double(i) / (spectrum.size() / 2));
                        spectrum[i] *= freq_factor;
                        spectrum[spectrum.size() - i] *= freq_factor;
                    }
                };

                auto result = process_spectral_windows(data_span, window_size, hop_size, processor);
                std::copy(result.begin(), result.end(), data_span.begin());

                return create_output(OperationHelper::reconstruct_from_double<InputType>(
                    std::vector<double>(data_span.begin(), data_span.end()), structure_info));
            }

            // thread_local std::vector<double> m_working_buffer;
            auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input_data, m_working_buffer);
            auto processor = [enhancement_factor](Eigen::VectorXcd& spectrum, size_t) {
                for (int i = 1; i < spectrum.size() / 2; ++i) {
                    double freq_factor = 1.0 + (enhancement_factor - 1.0) * (double(i) / (spectrum.size() / 2));
                    spectrum[i] *= freq_factor;
                    spectrum[spectrum.size() - i] *= freq_factor;
                }
            };

            auto result = process_spectral_windows(target_data, window_size, hop_size, processor);
            m_working_buffer = std::move(result);

            return create_output(OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
        }

        case SpectralOperation::SPECTRAL_GATE: {
            auto threshold = get_parameter_or<double>("threshold", -40.0);
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            double linear_threshold = std::pow(10.0, threshold / 20.0);

            // Use custom spectral processing for spectral gating
            if (this->is_in_place()) {
                auto [data_span, structure_info] = OperationHelper::extract_structured_double(input_data);

                auto processor = [linear_threshold](Eigen::VectorXcd& spectrum, size_t) {
                    std::ranges::transform(spectrum, spectrum.begin(), [linear_threshold](const std::complex<double>& bin) {
                        return (std::abs(bin) > linear_threshold) ? bin : std::complex<double>(0.0, 0.0);
                    });
                };

                auto result = process_spectral_windows(data_span, window_size, hop_size, processor);
                std::copy(result.begin(), result.end(), data_span.begin());

                return create_output(OperationHelper::reconstruct_from_double<InputType>(
                    std::vector<double>(data_span.begin(), data_span.end()), structure_info));
            }

            // thread_local std::vector<double> m_working_buffer;
            auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input_data, m_working_buffer);
            auto processor = [linear_threshold](Eigen::VectorXcd& spectrum, size_t) {
                std::ranges::transform(spectrum, spectrum.begin(), [linear_threshold](const std::complex<double>& bin) {
                    return (std::abs(bin) > linear_threshold) ? bin : std::complex<double>(0.0, 0.0);
                });
            };

            auto result = process_spectral_windows(target_data, window_size, hop_size, processor);
            m_working_buffer = std::move(result);

            return create_output(OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
        }

        case SpectralOperation::PHASE_VOCODER: {
            auto window_size = get_parameter_or<uint32_t>("window_size", 1024);
            auto hop_size = get_parameter_or<uint32_t>("hop_size", 512);

            double semitones = 0.0;

            if (this->is_in_place()) {
                return create_output(transform_pitch_shift(input_data, semitones, window_size, hop_size));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_pitch_shift(input_data, semitones, window_size, hop_size, m_working_buffer));
        }

        default:
            return create_output(input_data);
        }
    }

    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto op_result = safe_any_cast<SpectralOperation>(value)) {
                m_operation = *op_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                if (auto op_enum = Utils::string_to_enum_case_insensitive<SpectralOperation>(*str_result.value)) {
                    m_operation = *op_enum;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    SpectralOperation m_operation;
    mutable std::vector<double> m_working_buffer; ///< Add working buffer member

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

    template <typename T>
    T get_parameter_or(const std::string& name, const T& default_value) const
    {
        auto param = this->get_transformation_parameter(name);
        if (!param.has_value())
            return default_value;

        auto result = safe_any_cast<T>(param);
        return result.value_or(default_value);
    }

    output_type create_output(const InputType& data)
    {
        output_type result;
        if constexpr (std::is_same_v<InputType, OutputType>) {
            result.data = data;
        } else {
            result.data = OperationHelper::convert_result_to_output_type<OutputType>(data);
        }
        return result;
    }
};
}
