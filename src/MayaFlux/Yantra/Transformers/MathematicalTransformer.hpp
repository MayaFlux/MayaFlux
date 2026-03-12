#pragma once

#include "MayaFlux/Kinesis/Discrete/Transform.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "UniversalTransformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;
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
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

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
            const auto a = get_parameter_or<double>("gain_factor", 1.0);
            return apply_per_channel(input, [a](std::span<double> ch) {
                D::linear(ch, a, 0.0);
            });
        }

        case MathematicalOperation::OFFSET: {
            const auto b = get_parameter_or<double>("offset_value", 0.0);
            return apply_per_channel(input, [b](std::span<double> ch) {
                D::linear(ch, 1.0, b);
            });
        }

        case MathematicalOperation::POWER: {
            const auto exp = get_parameter_or<double>("exponent", 2.0);
            return apply_per_channel(input, [exp](std::span<double> ch) {
                D::power(ch, exp);
            });
        }

        case MathematicalOperation::LOGARITHMIC: {
            const auto a = get_parameter_or<double>("scale", 1.0);
            const auto b = get_parameter_or<double>("input_scale", 1.0);
            const auto c = get_parameter_or<double>("offset", 1.0);
            const auto base = get_parameter_or<double>("base", std::numbers::e);
            return apply_per_channel(input, [a, b, c, base](std::span<double> ch) {
                D::logarithmic(ch, a, b, c, base);
            });
        }

        case MathematicalOperation::EXPONENTIAL: {
            const auto a = get_parameter_or<double>("scale", 1.0);
            const auto b = get_parameter_or<double>("rate", 1.0);
            const auto base = get_parameter_or<double>("base", std::numbers::e);
            return apply_per_channel(input, [a, b, base](std::span<double> ch) {
                D::exponential(ch, a, b, base);
            });
        }

        case MathematicalOperation::TRIGONOMETRIC: {
            const auto fn = get_parameter_or<std::string>("trig_function", "sin");
            const auto freq = get_parameter_or<double>("frequency", 1.0);
            const auto amp = get_parameter_or<double>("amplitude", 1.0);
            const auto ph = get_parameter_or<double>("phase", 0.0);
            if (fn == "cos")
                return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                    D::apply_trig(ch, [](double x) { return std::cos(x); }, freq, amp, ph);
                });
            if (fn == "tan")
                return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                    D::apply_trig(ch, [](double x) { return std::tan(x); }, freq, amp, ph);
                });
            return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                D::apply_trig(ch, [](double x) { return std::sin(x); }, freq, amp, ph);
            });
        }

        case MathematicalOperation::QUANTIZE: {
            const auto bits = get_parameter_or<uint8_t>("bits", 16);
            return apply_per_channel(input, [bits](std::span<double> ch) {
                D::quantize(ch, bits);
            });
        }

        case MathematicalOperation::NORMALIZE: {
            const auto peak = get_parameter_or<double>("target_peak", 1.0);
            return apply_per_channel(input, [peak](std::span<double> ch) {
                D::normalize(ch, -peak, peak);
            });
        }

        case MathematicalOperation::POLYNOMIAL: {
            const auto coeffs = get_parameter_or<std::vector<double>>(
                "coefficients", std::vector<double> { 0.0, 1.0 });
            auto poly = std::make_shared<Nodes::Generator::Polynomial>(coeffs);
            return apply_per_channel(input, [&poly](std::span<double> ch) {
                std::ranges::transform(ch, ch.begin(),
                    [&poly](double x) { return poly->process_sample(x); });
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
     * Handles setting of operation type and delegates other parameters to base class.
     * Supports both enum and string values for the "operation" parameter.
     */
    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto r = safe_any_cast<MathematicalOperation>(value)) {
                m_operation = *r.value;
                return;
            }
            if (auto r = safe_any_cast<std::string>(value)) {
                if (auto e = Reflect::string_to_enum_case_insensitive<MathematicalOperation>(*r.value)) {
                    m_operation = *e;
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
     * @brief Extracts per-channel spans, applies an in-place func to each, and reconstructs.
     * @tparam Func Callable matching void(std::span<double>)
     *
     * In-place: results are copied back into the original channel spans of @p input before
     * reconstruction. Out-of-place: input is not mutated.
     */
    template <typename Func>
    output_type apply_per_channel(input_type& input, Func&& func)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(input);
        m_working_buffer.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i) {
            m_working_buffer[i].assign(channels[i].begin(), channels[i].end());
            func(std::span<double> { m_working_buffer[i] });
        }
        if (this->is_in_place())
            for (size_t i = 0; i < channels.size(); ++i)
                std::ranges::copy(m_working_buffer[i], channels[i].begin());
        return create_output(
            OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
    }

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
