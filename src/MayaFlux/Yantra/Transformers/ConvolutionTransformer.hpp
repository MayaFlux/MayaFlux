#pragma once

#include "UniversalTransformer.hpp"
#include "helpers/ConvolutionHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum ConvolutionOperation
 * @brief Specific convolution operations supported
 */
enum class ConvolutionOperation : uint8_t {
    DIRECT_CONVOLUTION, ///< Standard convolution
    CROSS_CORRELATION, ///< Cross-correlation
    MATCHED_FILTER, ///< Matched filtering
    DECONVOLUTION, ///< Deconvolution
    AUTO_CORRELATION ///< Auto-correlation
};

/**
 * @class ConvolutionTransformer
 * @brief Concrete transformer for convolution-based operations
 *
 * Handles various convolution operations:
 * - Direct convolution and cross-correlation
 * - Impulse response application
 * - Matched filtering and signal detection
 * - Deconvolution and restoration
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class ConvolutionTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    explicit ConvolutionTransformer(ConvolutionOperation op = ConvolutionOperation::DIRECT_CONVOLUTION)
        : m_operation(op)
    {
        set_default_parameters();
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::CONVOLUTION;
    }

    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("ConvolutionTransformer_").append(Utils::enum_to_string(m_operation));
    }

protected:
    output_type transform_implementation(input_type& input) override
    {
        auto& input_data = input.data;

        switch (m_operation) {
        case ConvolutionOperation::DIRECT_CONVOLUTION: {
            auto impulse_response = get_parameter_or<std::vector<double>>("impulse_response", std::vector<double> { 1.0 });

            if (this->is_in_place()) {
                return create_output(transform_convolve(input_data, impulse_response));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_convolve(input_data, impulse_response, m_working_buffer));
        }

        case ConvolutionOperation::CROSS_CORRELATION: {
            auto template_signal = get_parameter_or<std::vector<double>>("template_signal", std::vector<double> { 1.0 });
            auto normalize = get_parameter_or<bool>("normalize", true);

            if (this->is_in_place()) {
                return create_output(transform_cross_correlate(input_data, template_signal, normalize));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_cross_correlate(input_data, template_signal, normalize, m_working_buffer));
        }

        case ConvolutionOperation::MATCHED_FILTER: {
            auto reference_signal = get_parameter_or<std::vector<double>>("reference_signal", std::vector<double> { 1.0 });

            if (this->is_in_place()) {
                return create_output(transform_matched_filter(input_data, reference_signal));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_matched_filter(input_data, reference_signal, m_working_buffer));
        }

        case ConvolutionOperation::DECONVOLUTION: {
            auto impulse_response = get_parameter_or<std::vector<double>>("impulse_response", std::vector<double> { 1.0 });
            auto regularization = get_parameter_or<double>("regularization", 1e-6);

            if (this->is_in_place()) {
                return create_output(transform_deconvolve(input_data, impulse_response, regularization));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_deconvolve(input_data, impulse_response, regularization, m_working_buffer));
        }

        case ConvolutionOperation::AUTO_CORRELATION: {
            auto normalize = get_parameter_or<bool>("normalize", true);

            if (this->is_in_place()) {
                return create_output(transform_auto_correlate_fft(input_data, normalize));
            }
            // thread_local std::vector<double> m_working_buffer;
            return create_output(transform_auto_correlate_fft(input_data, m_working_buffer, normalize));
        }

        default:
            return create_output(input_data);
        }
    }

    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto op_result = safe_any_cast<ConvolutionOperation>(value)) {
                m_operation = *op_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                if (auto op_enum = Utils::string_to_enum_case_insensitive<ConvolutionOperation>(*str_result.value)) {
                    m_operation = *op_enum;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    ConvolutionOperation m_operation;
    mutable std::vector<double> m_working_buffer;

    void set_default_parameters()
    {
        this->set_parameter("impulse_response", std::vector<double> { 1.0 });
        this->set_parameter("template_signal", std::vector<double> { 1.0 });
        this->set_parameter("reference_signal", std::vector<double> { 1.0 });
        this->set_parameter("normalize", true);
        this->set_parameter("regularization", 1e-6);
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
