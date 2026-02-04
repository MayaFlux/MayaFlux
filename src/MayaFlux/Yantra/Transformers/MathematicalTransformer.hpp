#pragma once

#include "UniversalTransformer.hpp"
#include "helpers/MathematicalHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum MathematicalOperation
 * @brief Specific mathematical operations supported
 */
enum class MathematicalOperation : uint8_t {
    GAIN, ///< Linear gain/attenuation
    OFFSET, ///< DC offset
    POWER, ///< Power function
    LOGARITHMIC, ///< Logarithmic transform
    EXPONENTIAL, ///< Exponential transform
    TRIGONOMETRIC, ///< Trigonometric functions
    QUANTIZE, ///< Quantization/bit reduction
    NORMALIZE, ///< Normalization
    POLYNOMIAL ///< Polynomial transform
};

/**
 * @class MathematicalTransformer
 * @brief Concrete transformer for mathematical operations
 *
 * Handles pure mathematical transformations:
 * - Arithmetic operations (gain, offset, scaling)
 * - Trigonometric functions
 * - Logarithmic and exponential transforms
 * - Polynomial and power functions
 * - Quantization and bit reduction
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API MathematicalTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    /**
     * @brief Constructs a MathematicalTransformer with specified operation
     * @param op The mathematical operation to perform (default: GAIN)
     */
    explicit MathematicalTransformer(MathematicalOperation op = MathematicalOperation::GAIN)
        : m_operation(op)
    {
        set_default_parameters();
    }

    /**
     * @brief Gets the transformation type
     * @return TransformationType::MATHEMATICAL
     */
    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::MATHEMATICAL;
    }

    /**
     * @brief Gets the transformer name including the operation type
     * @return String representation of the transformer name
     */
    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("MathematicalTransformer_").append(Reflect::enum_to_string(m_operation));
    }

protected:
    /**
     * @brief Core transformation implementation
     * @param input Input data to transform
     * @return Transformed output data
     *
     * Performs the mathematical operation specified by m_operation on the input data.
     * Supports both in-place and out-of-place transformations based on transformer settings.
     */
    output_type transform_implementation(input_type& input) override
    {
        switch (m_operation) {
        case MathematicalOperation::GAIN: {
            auto gain_factor = get_parameter_or<double>("gain_factor", 1.0);
            if (this->is_in_place()) {
                return create_output(transform_linear(input, gain_factor, 0.0));
            }
            return create_output(transform_linear(input, gain_factor, 0.0, m_working_buffer));
        }

        case MathematicalOperation::OFFSET: {
            auto offset_value = get_parameter_or<double>("offset_value", 0.0);
            if (this->is_in_place()) {
                return create_output(transform_linear(input, 1.0, offset_value));
            }
            return create_output(transform_linear(input, 1.0, offset_value, m_working_buffer));
        }

        case MathematicalOperation::POWER: {
            auto exponent = get_parameter_or<double>("exponent", 2.0);

            if (this->is_in_place()) {
                return create_output(transform_power(input, exponent));
            }
            return create_output(transform_power(input, exponent, m_working_buffer));
        }

        case MathematicalOperation::LOGARITHMIC: {
            auto scale = get_parameter_or<double>("scale", 1.0);
            auto input_scale = get_parameter_or<double>("input_scale", 1.0);
            auto offset = get_parameter_or<double>("offset", 1.0);
            auto base = get_parameter_or<double>("base", std::numbers::e);

            if (this->is_in_place()) {
                return create_output(transform_logarithmic(input, scale, input_scale, offset, base));
            }
            return create_output(transform_logarithmic(input, scale, input_scale, offset, m_working_buffer, base));
        }

        case MathematicalOperation::EXPONENTIAL: {
            auto scale = get_parameter_or<double>("scale", 1.0);
            auto rate = get_parameter_or<double>("rate", 1.0);
            auto base = get_parameter_or<double>("base", std::numbers::e);

            if (this->is_in_place()) {
                return create_output(transform_exponential(input, scale, rate, base));
            }
            return create_output(transform_exponential(input, scale, rate, m_working_buffer, base));
        }

        case MathematicalOperation::TRIGONOMETRIC: {
            auto trig_function = get_parameter_or<std::string>("trig_function", "sin");
            auto frequency = get_parameter_or<double>("frequency", 1.0);
            auto amplitude = get_parameter_or<double>("amplitude", 1.0);
            auto phase = get_parameter_or<double>("phase", 0.0);

            if (trig_function == "sin") {
                if (this->is_in_place()) {
                    return create_output(transform_trigonometric(input, [](double x) { return std::sin(x); }, frequency, amplitude, phase));
                }
                return create_output(transform_trigonometric(input, [](double x) { return std::sin(x); }, frequency, amplitude, phase, m_working_buffer));
            }
            if (trig_function == "cos") {
                if (this->is_in_place()) {
                    return create_output(transform_trigonometric(input, [](double x) { return std::cos(x); }, frequency, amplitude, phase));
                }
                return create_output(transform_trigonometric(input, [](double x) { return std::cos(x); }, frequency, amplitude, phase, m_working_buffer));
            }
            if (trig_function == "tan") {
                if (this->is_in_place()) {
                    return create_output(transform_trigonometric(input, [](double x) { return std::tan(x); }, frequency, amplitude, phase));
                }
                return create_output(transform_trigonometric(input, [](double x) { return std::tan(x); }, frequency, amplitude, phase, m_working_buffer));
            }
            return create_output(input);
        }

        case MathematicalOperation::QUANTIZE: {
            auto bits = get_parameter_or<uint8_t>("bits", 16);
            if (this->is_in_place()) {
                return create_output(transform_quantize(input, bits));
            }
            return create_output(transform_quantize(input, bits, m_working_buffer));
        }

        case MathematicalOperation::NORMALIZE: {
            auto target_peak = get_parameter_or<double>("target_peak", 1.0);
            std::pair<double, double> target_range = { -target_peak, target_peak };
            if (this->is_in_place()) {
                return create_output(transform_normalize(input, target_range));
            }
            return create_output(transform_normalize(input, target_range, m_working_buffer));
        }

        case MathematicalOperation::POLYNOMIAL: {
            auto coefficients = get_parameter_or<std::vector<double>>("coefficients", std::vector<double> { 0.0, 1.0 });
            if (this->is_in_place()) {
                return create_output(transform_polynomial(input, coefficients));
            }
            return create_output(transform_polynomial(input, coefficients, m_working_buffer));
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
     * Handles setting of operation type and delegates other parameters to base class.
     * Supports both enum and string values for the "operation" parameter.
     */
    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto op_result = safe_any_cast<MathematicalOperation>(value)) {
                m_operation = *op_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                if (auto op_enum = Reflect::string_to_enum_case_insensitive<MathematicalOperation>(*str_result.value)) {
                    m_operation = *op_enum;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    MathematicalOperation m_operation; ///< Current mathematical operation
    mutable std::vector<std::vector<double>> m_working_buffer; ///< Buffer for out-of-place operations

    /**
     * @brief Sets default parameter values for all mathematical operations
     *
     * Initializes all possible parameters with sensible defaults to ensure
     * the transformer works correctly regardless of the selected operation.
     */
    void set_default_parameters()
    {
        this->set_parameter("gain_factor", 1.0);
        this->set_parameter("offset_value", 0.0);
        this->set_parameter("exponent", 2.0);
        this->set_parameter("base", std::numbers::e);
        this->set_parameter("scale", 1.0);
        this->set_parameter("trig_function", std::string { "sin" });
        this->set_parameter("frequency", 1.0);
        this->set_parameter("amplitude", 1.0);
        this->set_parameter("phase", 0.0);
        this->set_parameter("bits", uint8_t { 16 });
        this->set_parameter("target_peak", 1.0);
        this->set_parameter("coefficients", std::vector<double> { 0.0, 1.0 });
        this->set_parameter("input_scale", 1.0);
        this->set_parameter("offset", 1.0);
        this->set_parameter("rate", 1.0);
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
     * or direct assignment when types match.
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
