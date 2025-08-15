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

    /**
     * @brief Constructs a ConvolutionTransformer with specified operation
     * @param op The convolution operation to perform (default: DIRECT_CONVOLUTION)
     */
    explicit ConvolutionTransformer(ConvolutionOperation op = ConvolutionOperation::DIRECT_CONVOLUTION)
        : m_operation(op)
    {
        set_default_parameters();
    }

    /**
     * @brief Gets the transformation type
     * @return TransformationType::CONVOLUTION
     */
    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::CONVOLUTION;
    }

    /**
     * @brief Gets the transformer name including the operation type
     * @return String representation of the transformer name
     */
    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("ConvolutionTransformer_").append(Utils::enum_to_string(m_operation));
    }

protected:
    /**
     * @brief Core transformation implementation for convolution operations
     * @param input Input data to transform using convolution methods
     * @return Transformed output data
     *
     * Performs the convolution operation specified by m_operation on the input data.
     * Operations include direct convolution, cross-correlation, matched filtering,
     * deconvolution, and auto-correlation. These operations are fundamental for
     * signal processing tasks such as filtering, pattern matching, and system
     * identification. Supports both in-place and out-of-place transformations.
     */
    output_type transform_implementation(input_type& input) override
    {
        auto& input_data = input.data;

        switch (m_operation) {
        case ConvolutionOperation::DIRECT_CONVOLUTION: {
            auto impulse_response = get_parameter_or<std::vector<double>>("impulse_response", std::vector<double> { 1.0 });

            if (this->is_in_place()) {
                return create_output(transform_convolve(input_data, impulse_response));
            }
            return create_output(transform_convolve(input_data, impulse_response, m_working_buffer));
        }

        case ConvolutionOperation::CROSS_CORRELATION: {
            auto template_signal = get_parameter_or<std::vector<double>>("template_signal", std::vector<double> { 1.0 });
            auto normalize = get_parameter_or<bool>("normalize", true);

            if (this->is_in_place()) {
                return create_output(transform_cross_correlate(input_data, template_signal, normalize));
            }
            return create_output(transform_cross_correlate(input_data, template_signal, normalize, m_working_buffer));
        }

        case ConvolutionOperation::MATCHED_FILTER: {
            auto reference_signal = get_parameter_or<std::vector<double>>("reference_signal", std::vector<double> { 1.0 });

            if (this->is_in_place()) {
                return create_output(transform_matched_filter(input_data, reference_signal));
            }
            return create_output(transform_matched_filter(input_data, reference_signal, m_working_buffer));
        }

        case ConvolutionOperation::DECONVOLUTION: {
            auto impulse_response = get_parameter_or<std::vector<double>>("impulse_response", std::vector<double> { 1.0 });
            auto regularization = get_parameter_or<double>("regularization", 1e-6);

            if (this->is_in_place()) {
                return create_output(transform_deconvolve(input_data, impulse_response, regularization));
            }
            return create_output(transform_deconvolve(input_data, impulse_response, regularization, m_working_buffer));
        }

        case ConvolutionOperation::AUTO_CORRELATION: {
            auto normalize = get_parameter_or<bool>("normalize", true);

            if (this->is_in_place()) {
                return create_output(transform_auto_correlate_fft(input_data, normalize));
            }
            return create_output(transform_auto_correlate_fft(input_data, m_working_buffer, normalize));
        }

        default:
            return create_output(input_data);
        }
    }

    /**
     * @brief Sets transformation parameters
     * @param name Parameter name
     * @param value Parameter value
     *
     * Handles setting of convolution operation type and delegates other parameters to base class.
     * Supports both enum and string values for the "operation" parameter.
     *
     * Common parameters include:
     * - "impulse_response": Impulse response for convolution/deconvolution operations
     * - "template_signal": Template signal for cross-correlation
     * - "reference_signal": Reference signal for matched filtering
     * - "normalize": Whether to normalize correlation results
     * - "regularization": Regularization parameter for deconvolution stability
     */
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
    ConvolutionOperation m_operation; ///< Current convolution operation
    mutable std::vector<double> m_working_buffer; ///< Buffer for out-of-place convolution operations

    /**
     * @brief Sets default parameter values for all convolution operations
     *
     * Initializes all possible parameters with sensible defaults to ensure
     * the transformer works correctly regardless of the selected operation.
     * Default values are chosen for typical signal processing scenarios.
     */
    void set_default_parameters()
    {
        this->set_parameter("impulse_response", std::vector<double> { 1.0 });
        this->set_parameter("template_signal", std::vector<double> { 1.0 });
        this->set_parameter("reference_signal", std::vector<double> { 1.0 });
        this->set_parameter("normalize", true);
        this->set_parameter("regularization", 1e-6);
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
     * or direct assignment when types match. Ensures convolution processing results
     * maintain the correct output type and signal characteristics.
     */
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
