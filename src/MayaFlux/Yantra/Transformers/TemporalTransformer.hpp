#pragma once

#include "UniversalTransformer.hpp"

#include "helpers/TemporalHelper.hpp"

namespace MayaFlux::Yantra {

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
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class TemporalTransformer final : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

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
        return std::string("TemporalTransformer_").append(Utils::enum_to_string(m_operation));
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
        auto& input_data = input.data;

        switch (m_operation) {
        case TemporalOperation::TIME_REVERSE: {
            if (this->is_in_place()) {
                return create_output(transform_time_reverse(input_data));
            }

            return create_output(transform_time_reverse(input_data, m_working_buffer));
        }

        case TemporalOperation::TIME_STRETCH: {
            auto stretch_factor = get_parameter_or<double>("stretch_factor", 1.0);
            if (this->is_in_place()) {
                return create_output(transform_time_stretch(input_data, stretch_factor));
            }

            return create_output(transform_time_stretch(input_data, stretch_factor, m_working_buffer));
        }

        case TemporalOperation::DELAY: {
            auto delay_samples = get_parameter_or<uint32_t>("delay_samples", 1000);
            auto fill_value = get_parameter_or<double>("fill_value", 0.0);
            if (this->is_in_place()) {
                return create_output(transform_delay(input_data, delay_samples, fill_value));
            }

            return create_output(transform_delay(input_data, delay_samples, fill_value, m_working_buffer));
        }

        case TemporalOperation::FADE_IN_OUT: {
            auto fade_in_ratio = get_parameter_or<double>("fade_in_ratio", 0.1);
            auto fade_out_ratio = get_parameter_or<double>("fade_out_ratio", 0.1);
            if (this->is_in_place()) {
                return create_output(transform_fade(input_data, fade_in_ratio, fade_out_ratio));
            }

            return create_output(transform_fade(input_data, fade_in_ratio, fade_out_ratio, m_working_buffer));
        }

        case TemporalOperation::SLICE: {
            auto start_ratio = get_parameter_or<double>("start_ratio", 0.0);
            auto end_ratio = get_parameter_or<double>("end_ratio", 1.0);
            if (this->is_in_place()) {
                return create_output(transform_slice(input_data, start_ratio, end_ratio));
            }

            return create_output(transform_slice(input_data, start_ratio, end_ratio, m_working_buffer));
        }

        case TemporalOperation::INTERPOLATE: {
            auto target_size = get_parameter_or<size_t>("target_size", 0);
            auto use_cubic = get_parameter_or<bool>("use_cubic", false);
            if (target_size > 0) {
                if (use_cubic) {
                    if (this->is_in_place()) {
                        return create_output(interpolate_cubic(input_data, target_size));
                    }

                    return create_output(interpolate_cubic(input_data, target_size, m_working_buffer));
                }

                if (this->is_in_place()) {
                    return create_output(interpolate_linear(input_data, target_size));
                }
                return create_output(interpolate_linear(input_data, target_size, m_working_buffer));
            }
            return create_output(input_data);
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
            if (auto op_result = safe_any_cast<TemporalOperation>(value)) {
                m_operation = *op_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                if (auto op_enum = Utils::string_to_enum_case_insensitive<TemporalOperation>(*str_result.value)) {
                    m_operation = *op_enum;
                    return;
                }
            }
        }

        UniversalTransformer<InputType, OutputType>::set_transformation_parameter(name, std::move(value));
    }

private:
    TemporalOperation m_operation; ///< Current temporal operation
    mutable std::vector<double> m_working_buffer; ///< Buffer for out-of-place temporal operations

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
