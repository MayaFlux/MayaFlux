#pragma once

#include "MayaFlux/Kinesis/Discrete/Convolution.hpp"
#include "UniversalTransformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

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
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API ConvolutionTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

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
        return std::string("ConvolutionTransformer_").append(Reflect::enum_to_string(m_operation));
    }

protected:
    /**
     * @brief Applies the configured convolution operation
     * @param input Input data
     * @return Transformed output
     *
     * Extracts per-channel double spans, calls the corresponding
     * Kinesis::Discrete::Convolution primitive on each channel, then
     * reconstructs the typed output via OperationHelper.
     */
    output_type transform_implementation(input_type& input) override
    {
        switch (m_operation) {

        case ConvolutionOperation::DIRECT_CONVOLUTION: {
            const auto ir = get_parameter_or<std::vector<double>>(
                "impulse_response", std::vector<double> { 1.0 });
            return apply_per_channel(input, [&ir](std::span<const double> ch) {
                return D::convolve(ch, ir);
            });
        }

        case ConvolutionOperation::CROSS_CORRELATION: {
            const auto tmpl = get_parameter_or<std::vector<double>>(
                "template_signal", std::vector<double> { 1.0 });
            const auto norm = get_parameter_or<bool>("normalize", true);
            return apply_per_channel(input, [&tmpl, norm](std::span<const double> ch) {
                return D::cross_correlate(ch, tmpl, norm);
            });
        }

        case ConvolutionOperation::MATCHED_FILTER: {
            const auto ref = get_parameter_or<std::vector<double>>(
                "reference_signal", std::vector<double> { 1.0 });
            return apply_per_channel(input, [&ref](std::span<const double> ch) {
                return D::matched_filter(ch, ref);
            });
        }

        case ConvolutionOperation::DECONVOLUTION: {
            const auto ir = get_parameter_or<std::vector<double>>(
                "impulse_response", std::vector<double> { 1.0 });
            const auto reg = get_parameter_or<double>("regularization", 1e-6);
            return apply_per_channel(input, [&ir, reg](std::span<const double> ch) {
                return D::deconvolve(ch, ir, reg);
            });
        }

        case ConvolutionOperation::AUTO_CORRELATION: {
            const auto norm = get_parameter_or<bool>("normalize", true);
            return apply_per_channel(input, [norm](std::span<const double> ch) {
                return D::auto_correlate(ch, norm);
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
            if (auto r = safe_any_cast<ConvolutionOperation>(value)) {
                m_operation = *r.value;
                return;
            }
            if (auto r = safe_any_cast<std::string>(value)) {
                if (auto e = Reflect::string_to_enum_case_insensitive<ConvolutionOperation>(*r.value)) {
                    m_operation = *e;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    ConvolutionOperation m_operation; ///< Current convolution operation
    mutable std::vector<std::vector<double>> m_working_buffer; ///< Buffer for out-of-place convolution operations

    /**
     * @brief Extracts per-channel spans, applies func to each, and reconstructs.
     * @tparam Func Callable matching either void(std::span<double>) or
     *         std::vector<double>(std::span<const double>)
     *
     * In-place: results are copied back into the original channel spans of @p input.
     * Out-of-place: input is not mutated.
     */
    template <typename Func>
    output_type apply_per_channel(input_type& input, Func&& func)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(input);
        m_working_buffer.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i) {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, std::span<double>>>) {
                m_working_buffer[i].assign(channels[i].begin(), channels[i].end());
                func(std::span<double> { m_working_buffer[i] });
            } else {
                m_working_buffer[i] = func(std::span<const double> { channels[i] });
            }
        }
        if (this->is_in_place())
            for (size_t i = 0; i < channels.size(); ++i)
                std::ranges::copy(m_working_buffer[i], channels[i].begin());
        return create_output(
            OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
    }

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
