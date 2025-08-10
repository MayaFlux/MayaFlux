#pragma once

#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Yantra/OperationHelper.hpp"

/**
 * @file MathematicalTransforms.hpp
 * @brief Mathematical transformation functions leveraging existing ecosystem
 *
 * Provides mathematical transformation functions that can be used by any class.
 * Leverages existing infrastructure:
 * - Uses OperationHelper for data conversion/reconstruction
 * - Uses existing Generator::Polynomial for polynomial operations
 * - Uses StatisticalAnalyzer for normalization statistics
 * - Follows DataUtils in-place vs copy patterns
 * - Uses C++20 ranges/views for efficient processing
 *
 * Philosophy: **Function-based helpers** that compose existing capabilities.
 */

namespace MayaFlux::Yantra {

// ============================================================
// BASIC MATHEMATICAL TRANSFORMATIONS
// ============================================================

/**
 * @brief Linear transformation y = ax + b using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param a Scale factor
 * @param b Offset factor
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_linear(DataType& input, double a, double b, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [a, b](double x) { return a * x + b; });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Polynomial transformation using existing Generator::Polynomial
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param coefficients Polynomial coefficients [a0, a1, a2, ...]
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_polynomial(DataType& input,
    const std::vector<double>& coefficients,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    auto polynomial_gen = std::make_shared<Nodes::Generator::Polynomial>(coefficients);

    std::ranges::transform(target_data, target_data.begin(),
        [&polynomial_gen](double x) {
            return polynomial_gen->process_sample(x);
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Exponential transformation y = a * exp(b * x)
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param a Scale factor
 * @param b Exponential rate
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_exponential(DataType& input, double a, double b, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [a, b](double x) { return a * std::exp(b * x); });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Logarithmic transformation y = a * log(b * x + c)
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param a Scale factor
 * @param b Input scale factor
 * @param c Offset to ensure positive argument
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_logarithmic(DataType& input, double a, double b, double c = 1.0, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [a, b, c](double x) {
            double arg = b * x + c;
            return arg > 0.0 ? a * std::log(arg) : 0.0;
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Trigonometric transformation using specified function
 * @tparam DataType ComputeData type
 * @tparam TrigFunc Trigonometric function type
 * @param input Input data
 * @param trig_func Trigonometric function (sin, cos, tan, etc.)
 * @param frequency Frequency scaling factor
 * @param amplitude Amplitude scaling factor
 * @param phase Phase offset
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType, typename TrigFunc>
    requires std::invocable<TrigFunc, double>
DataType transform_trigonometric(DataType& input,
    TrigFunc trig_func,
    double frequency = 1.0,
    double amplitude = 1.0,
    double phase = 0.0,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [trig_func, frequency, amplitude, phase](double x) {
            return amplitude * trig_func(frequency * x + phase);
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Quantization transformation (bit reduction) using C++20 features
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param bits Number of bits for quantization
 * @param in_place Whether to modify input or create copy
 * @return Quantized data
 */
template <ComputeData DataType>
DataType transform_quantize(DataType& input, uint8_t bits, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    const double levels = std::pow(2.0, bits) - 1.0;

    std::ranges::transform(target_data, target_data.begin(),
        [levels](double x) {
            double clamped = std::clamp(x, -1.0, 1.0);
            return std::round(clamped * levels) / levels;
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Clamp transformation using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @param in_place Whether to modify input or create copy
 * @return Clamped data
 */
template <ComputeData DataType>
DataType transform_clamp(DataType& input, double min_val, double max_val, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [min_val, max_val](double x) { return std::clamp(x, min_val, max_val); });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Wrap transformation (modulo operation) using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param wrap_range Wrap range [0, wrap_range)
 * @param in_place Whether to modify input or create copy
 * @return Wrapped data
 */
template <ComputeData DataType>
DataType transform_wrap(DataType& input, double wrap_range, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [wrap_range](double x) {
            return x - wrap_range * std::floor(x / wrap_range);
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Normalize transformation using existing infrastructure
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param target_range Target range [min, max]
 * @param in_place Whether to modify input or create copy
 * @return Normalized data
 */
template <ComputeData DataType>
DataType transform_normalize(DataType& input,
    const std::pair<double, double>& target_range = { -1.0, 1.0 },
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    auto [min_it, max_it] = std::ranges::minmax_element(data_span);
    double current_min = *min_it;
    double current_max = *max_it;

    if (current_max == current_min)
        return input;

    double current_range = current_max - current_min;
    double target_min = target_range.first;
    double target_max = target_range.second;
    double target_span = target_max - target_min;

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::transform(target_data, target_data.begin(),
        [current_min, current_range, target_span, target_min](double x) {
            return ((x - current_min) / current_range) * target_span + target_min;
        });

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

// ============================================================
// INTERPOLATION FUNCTIONS
// ============================================================

/**
 * @brief Linear interpolation between data points using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param target_size Target size after interpolation
 * @param in_place Whether to modify input or create copy (note: size change means copy)
 * @return Interpolated data
 */
template <ComputeData DataType>
DataType interpolate_linear(DataType& input, size_t target_size, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (target_size == data_span.size()) {
        return input;
    }

    std::vector<double> interpolated(target_size);

    auto indices = std::views::iota(size_t { 0 }, target_size);

    std::ranges::transform(indices, interpolated.begin(),
        [&data_span, target_size](size_t i) {
            double pos = static_cast<double>(i) * (data_span.size() - 1) / (target_size - 1);
            auto idx = static_cast<size_t>(pos);
            double frac = pos - idx;

            if (idx + 1 < data_span.size()) {
                return data_span[idx] * (1.0 - frac) + data_span[idx + 1] * frac;
            } else {
                return data_span[idx];
            }
        });

    return OperationHelper::convert_result_to_output_type<DataType>(interpolated, structure_info);
}

/**
 * @brief Cubic interpolation between data points using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param target_size Target size after interpolation
 * @param in_place Whether to modify input or create copy
 * @return Interpolated data
 */
template <ComputeData DataType>
DataType interpolate_cubic(DataType& input, size_t target_size, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (target_size == data_span.size()) {
        return input;
    }

    std::vector<double> interpolated(target_size);

    auto indices = std::views::iota(size_t { 0 }, target_size);

    std::ranges::transform(indices, interpolated.begin(),
        [&data_span, target_size](size_t i) {
            double pos = static_cast<double>(i) * (data_span.size() - 1) / (target_size - 1);
            int idx = static_cast<int>(pos);
            double frac = pos - idx;

            auto get_sample = [&data_span](int i) {
                return data_span[std::clamp(i, 0, static_cast<int>(data_span.size() - 1))];
            };

            double y0 = get_sample(idx - 1);
            double y1 = get_sample(idx);
            double y2 = get_sample(idx + 1);
            double y3 = get_sample(idx + 2);

            double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
            double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
            double c = -0.5 * y0 + 0.5 * y2;
            double d = y1;

            return ((a * frac + b) * frac + c) * frac + d;
        });

    return OperationHelper::convert_result_to_output_type<DataType>(interpolated, structure_info);
}

} // namespace MayaFlux::Yantra
