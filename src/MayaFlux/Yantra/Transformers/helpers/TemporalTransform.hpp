#pragma once

#include "MathematicalTransform.hpp"

/**
 * @file TemporalSpectralTransforms.hpp
 * @brief Temporal and spectral transformation functions leveraging existing ecosystem
 *
 * Provides temporal transformation functions that can be used by any class.
 * Leverages existing infrastructure:
 * - Uses C++20 ranges/views/algorithms for efficient processing
 * - Integrates with existing windowing and spectral analysis functions
 *
 * Philosophy: **Function-based helpers** that compose existing capabilities.
 */

namespace MayaFlux::Yantra {

/**
 * @brief Overlap-add processing for windowed transforms using C++20 ranges
 * @tparam TransformFunc Function type for transformation
 * @param data Input data
 * @param window_size Size of analysis windows
 * @param hop_size Hop size between windows
 * @param transform_func Transformation function to apply per window
 * @return Processed data
 */
template <typename TransformFunc>
    requires std::invocable<TransformFunc, std::span<const double>>
std::vector<double> process_overlap_add(const std::span<const double>& data,
    uint32_t window_size,
    uint32_t hop_size,
    TransformFunc transform_func)
{
    const size_t num_windows = (data.size() - window_size) / hop_size + 1;
    std::vector<double> output(data.size(), 0.0);

    auto window_indices = std::views::iota(size_t { 0 }, num_windows);

    std::ranges::for_each(window_indices, [&](size_t win) {
        size_t start_idx = win * hop_size;
        auto window_data = data.subspan(start_idx,
            std::min(static_cast<size_t>(window_size), data.size() - start_idx));

        auto transformed = transform_func(window_data);

        for (size_t i = 0; i < transformed.size() && start_idx + i < output.size(); ++i) {
            output[start_idx + i] += transformed[i];
        }
    });

    return output;
}

/**
 * @brief Time reversal transformation using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param in_place Whether to modify input or create copy
 * @return Time-reversed data
 */
template <ComputeData DataType>
DataType transform_time_reverse(DataType& input, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    std::ranges::reverse(target_data);

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Simple time stretching via resampling using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param stretch_factor Stretch ratio (>1.0 = slower, <1.0 = faster)
 * @param in_place Whether to modify input or create copy
 * @return Time-stretched data
 */
template <ComputeData DataType>
DataType transform_time_stretch(DataType& input, double stretch_factor, bool in_place = false)
{
    if (stretch_factor == 1.0) {
        return input;
    }

    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);
    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }
    auto& target_data = in_place ? data_span : std::span(working_data);

    auto new_size = static_cast<size_t>(target_data.size() * stretch_factor);

    return interpolate_linear(target_data, new_size, false);
}

/**
 * @brief Delay transformation (temporal offset) using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param delay_samples Number of samples to delay
 * @param fill_value Value to use for padding
 * @param in_place Whether to modify input or create copy
 * @return Delayed data
 */
template <ComputeData DataType>
DataType transform_delay(DataType& input, uint32_t delay_samples, double fill_value = 0.0, bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    if (delay_samples >= target_data.size()) {
        std::ranges::fill(target_data, fill_value);
    } else {
        std::ranges::copy_backward(
            target_data | std::views::take(target_data.size() - delay_samples),
            target_data.end());

        std::ranges::fill(target_data | std::views::take(delay_samples), fill_value);
    }

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Fade transformation (linear fade-in and fade-out) using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param fade_in_duration_ratio Fade-in duration as a ratio of total length (0.0 to 1.0)
 * @param fade_out_duration_ratio Fade-out duration as a ratio of total length (0.0 to 1.0)
 * @param in_place Whether to modify input or create copy
 * @return Faded data
 */
template <ComputeData DataType>
DataType transform_fade(DataType& input,
    double fade_in_duration_ratio = 0.0,
    double fade_out_duration_ratio = 0.0,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    auto fade_in_samples = static_cast<size_t>(target_data.size() * fade_in_duration_ratio);
    auto fade_out_samples = static_cast<size_t>(target_data.size() * fade_out_duration_ratio);
    size_t fade_out_start = target_data.size() - fade_out_samples;

    if (fade_in_samples > 0) {
        auto fade_in_view = target_data | std::views::take(fade_in_samples);
        auto indices = std::views::iota(static_cast<std::size_t>(0), fade_in_samples);

        std::ranges::transform(fade_in_view, indices, fade_in_view.begin(),
            [fade_in_samples](double sample, size_t i) {
                double fade_factor = static_cast<double>(i) / static_cast<double>(fade_in_samples);
                return sample * fade_factor;
            });
    }

    if (fade_out_samples > 0 && fade_out_start < target_data.size()) {
        auto fade_out_view = target_data | std::views::drop(fade_out_start);
        auto indices = std::views::iota(static_cast<std::size_t>(0), fade_out_samples);

        std::ranges::transform(fade_out_view, indices, fade_out_view.begin(),
            [fade_out_samples](double sample, size_t i) {
                double fade_factor = 1.0 - (static_cast<double>(i) / static_cast<double>(fade_out_samples));
                return sample * fade_factor;
            });
    }

    return OperationHelper::convert_result_to_output_type<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Slice transformation to extract a portion of the data based on ratios
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param start_ratio Start ratio (0.0 to 1.0)
 * @param end_ratio End ratio (0.0 to 1.0)
 * @param in_place Whether to modify input or create copy
 * @return Sliced data
 */
template <ComputeData DataType>
DataType transform_slice(DataType& input,
    double start_ratio,
    double end_ratio,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    if (!in_place) {
        working_data.assign(data_span.begin(), data_span.end());
    }

    auto& target_data = in_place ? data_span : std::span(working_data);

    size_t start_idx = static_cast<size_t>(target_data.size() * std::clamp(start_ratio, 0.0, 1.0));
    size_t end_idx = static_cast<size_t>(target_data.size() * std::clamp(end_ratio, 0.0, 1.0));

    if (start_idx >= end_idx || end_idx > target_data.size()) {
        std::vector<double> empty_result = { 0.0 };
        return OperationHelper::convert_result_to_output_type<DataType>(empty_result, structure_info);
    }

    auto slice_view = target_data.subspan(start_idx, end_idx - start_idx);
    std::vector<double> slice_data(slice_view.begin(), slice_view.end());

    return OperationHelper::convert_result_to_output_type<DataType>(slice_data, structure_info);
}

} // namespace MayaFlux::Yantra::SpectralTransforms
