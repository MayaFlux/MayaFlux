#pragma once

#include "UniversalTransformer.hpp"

#include "MayaFlux/Kinesis/Discrete/Transform.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum TemporalOperation
 * @brief Specific temporal operations supported
 */
enum class TemporalOperation : uint8_t {
    TIME_REVERSE, ///< Reverse temporal order
    TIME_STRETCH, ///< Change playback speed
    DELAY, ///< Add temporal delay
    FADE_IN_OUT, ///< Apply fade envelope
    SLICE, ///< Extract temporal slice
    INTERPOLATE ///< Temporal interpolation
};

/**
 * @class TemporalTransformer
 * @brief Concrete transformer for time-domain operations
 *
 * Handles transformations that operate in the temporal domain:
 * - Time reversal, stretching, delay
 * - Envelope shaping (fade in/out)
 * - Temporal slicing and repositioning
 * - Rhythm and timing manipulations
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API TemporalTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Constructs a TemporalTransformer with specified operation
     * @param op The temporal operation to perform (default: TIME_REVERSE)
     */
    explicit TemporalTransformer(TemporalOperation op = TemporalOperation::TIME_REVERSE)
        : m_operation(op)
    {
        set_default_parameters();
    }

    /**
     * @brief Gets the transformation type
     * @return TransformationType::TEMPORAL
     */
    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::TEMPORAL;
    }

    /**
     * @brief Gets the transformer name including the operation type
     * @return String representation of the transformer name
     */
    [[nodiscard]] std::string get_transformer_name() const override
    {
        return std::string("TemporalTransformer_").append(Reflect::enum_to_string(m_operation));
    }

protected:
    /**
     * @brief Core transformation implementation for temporal operations
     * @param input Input data to transform in the time domain
     * @return Transformed output data
     *
     * Performs the temporal operation specified by m_operation on the input data.
     * Operations modify the temporal characteristics of the data including timing,
     * duration, ordering, and envelope shaping. Supports both in-place and
     * out-of-place transformations based on transformer settings.
     */
    output_type transform_implementation(input_type& input) override
    {
        switch (m_operation) {

        case TemporalOperation::TIME_REVERSE:
            return apply_per_channel(input, [](std::span<double> ch) {
                D::reverse(ch);
            });

        case TemporalOperation::FADE_IN_OUT: {
            const auto fi = get_parameter_or<double>("fade_in_ratio", 0.1);
            const auto fo = get_parameter_or<double>("fade_out_ratio", 0.1);
            return apply_per_channel(input, [fi, fo](std::span<double> ch) {
                D::fade(ch, fi, fo);
            });
        }

        case TemporalOperation::TIME_STRETCH: {
            const auto factor = get_parameter_or<double>("stretch_factor", 1.0);
            return apply_per_channel(input, [factor](std::span<const double> ch) {
                return D::time_stretch(ch, factor);
            });
        }

        case TemporalOperation::DELAY: {
            const auto samples = get_parameter_or<uint32_t>("delay_samples", 1000);
            const auto fill = get_parameter_or<double>("fill_value", 0.0);
            return apply_per_channel(input, [samples, fill](std::span<const double> ch) {
                return D::delay(ch, samples, fill);
            });
        }

        case TemporalOperation::SLICE: {
            const auto start = get_parameter_or<double>("start_ratio", 0.0);
            const auto end = get_parameter_or<double>("end_ratio", 1.0);
            return apply_per_channel(input, [start, end](std::span<const double> ch) {
                return D::slice(ch, start, end);
            });
        }

        case TemporalOperation::INTERPOLATE: {
            const auto target = get_parameter_or<size_t>("target_size", size_t { 0 });
            const auto cubic = get_parameter_or<bool>("use_cubic", false);
            if (target == 0)
                return create_output(input);
            return apply_per_channel(input, [target, cubic](std::span<const double> ch) {
                std::vector<double> out(target);
                if (cubic)
                    D::interpolate_cubic(ch, out);
                else
                    D::interpolate_linear(ch, out);
                return out;
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
     * Handles setting of temporal operation type and delegates other parameters to base class.
     * Supports both enum and string values for the "operation" parameter.
     *
     * Common parameters include:
     * - "stretch_factor": Time stretching factor (1.0 = no change, >1.0 = slower, <1.0 = faster)
     * - "delay_samples": Number of samples to delay
     * - "fill_value": Value to fill delayed regions with
     * - "fade_in_ratio", "fade_out_ratio": Fade envelope ratios (0.0-1.0)
     * - "start_ratio", "end_ratio": Slice boundaries as ratios of total length
     * - "target_size": Target size for interpolation
     * - "use_cubic": Whether to use cubic interpolation (vs linear)
     */
    void set_transformation_parameter(const std::string& name, std::any value) override
    {
        if (name == "operation") {
            if (auto r = safe_any_cast<TemporalOperation>(value)) {
                m_operation = *r.value;
                return;
            }
            if (auto r = safe_any_cast<std::string>(value)) {
                if (auto e = Reflect::string_to_enum_case_insensitive<TemporalOperation>(*r.value)) {
                    m_operation = *e;
                    return;
                }
            }
        }
        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    TemporalOperation m_operation; ///< Current temporal operation
    mutable std::vector<std::vector<double>> m_working_buffer; ///< Buffer for out-of-place temporal operations

    /**
     * @brief Extracts per-channel spans, applies func to each, and reconstructs.
     * @tparam Func Callable matching either void(std::span<double>) for same-size
     *         operations or std::vector<double>(std::span<const double>) for
     *         operations that may resize the channel.
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
     * @brief Sets default parameter values for all temporal operations
     *
     * Initializes all possible parameters with sensible defaults to ensure
     * the transformer works correctly regardless of the selected operation.
     * Default values are chosen for typical temporal processing scenarios.
     */
    void set_default_parameters()
    {
        this->set_parameter("stretch_factor", 1.0);
        this->set_parameter("delay_samples", uint32_t { 1000 });
        this->set_parameter("fill_value", 0.0);
        this->set_parameter("fade_in_ratio", 0.1);
        this->set_parameter("fade_out_ratio", 0.1);
        this->set_parameter("start_ratio", 0.0);
        this->set_parameter("end_ratio", 1.0);
        this->set_parameter("target_size", size_t { 0 });
        this->set_parameter("use_cubic", false);
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
     * or direct assignment when types match. Ensures temporal processing results
     * maintain the correct output type and temporal characteristics.
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
