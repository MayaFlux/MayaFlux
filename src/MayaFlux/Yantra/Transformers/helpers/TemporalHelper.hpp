#pragma once

#include <algorithm>

#include "MathematicalHelper.hpp"

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
    u_int32_t window_size,
    u_int32_t hop_size,
    TransformFunc transform_func)
{
    const size_t num_windows = (data.size() - window_size) / hop_size + 1;
    std::vector<double> output(data.size(), 0.0);

    std::ranges::for_each(std::views::iota(size_t { 0 }, num_windows), [&](size_t win) {
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
 * @brief Time reversal transformation using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @return Time-reversed data (same as input, modified in-place)
 */
template <OperationReadyData DataType>
DataType transform_time_reverse(DataType& input)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::reverse(span);
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Time reversal transformation using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Time-reversed data
 */
template <OperationReadyData DataType>
DataType transform_time_reverse(DataType& input, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::reverse(span);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Simple time stretching via resampling using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param stretch_factor Stretch ratio (>1.0 = slower, <1.0 = faster)
 * @return Time-stretched data
 */
template <OperationReadyData DataType>
DataType transform_time_stretch(DataType& input, double stretch_factor)
{
    if (stretch_factor == 1.0) {
        return input;
    }

    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<std::vector<double>> result(target_data.size());

    for (size_t i = 0; i < target_data.size(); ++i) {
        auto new_size = static_cast<size_t>(target_data[i].size() * stretch_factor);
        result[i].resize(new_size);
        interpolate(target_data[i], result[i], new_size);
    }

    return OperationHelper::reconstruct_from_double<DataType>(result, structure_info);
}

/**
 * @brief Simple time stretching via resampling using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param stretch_factor Stretch ratio (>1.0 = slower, <1.0 = faster)
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Time-stretched data
 */
template <OperationReadyData DataType>
DataType transform_time_stretch(DataType& input, double stretch_factor, std::vector<std::vector<double>>& working_buffer)
{
    if (stretch_factor == 1.0) {
        return input;
    }

    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t i = 0; i < target_data.size(); ++i) {
        auto new_size = static_cast<size_t>(target_data[i].size() * stretch_factor);
        working_buffer[i].resize(new_size);
        interpolate(target_data[i], working_buffer[i], new_size);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Delay transformation that extends buffer size (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED/EXTENDED
 * @param delay_samples Number of samples to delay (extends output size)
 * @param fill_value Value to use for padding
 * @return Delayed data with extended size
 */
template <OperationReadyData DataType>
DataType transform_delay(DataType& input, u_int32_t delay_samples, double fill_value = 0.0)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<std::vector<double>> result(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        result[i].resize(target_data[i].size() + delay_samples);
        std::fill_n(result[i].begin(), delay_samples, fill_value);
        std::copy(target_data[i].begin(), target_data[i].end(), result[i].begin() + delay_samples);
    }

    return OperationHelper::reconstruct_from_double<DataType>(result, structure_info);
}

/**
 * @brief Delay transformation that extends buffer size (OUT-OF-PLACE) - CORRECTED
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param delay_samples Number of samples to delay (extends output size)
 * @param fill_value Value to use for padding
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Delayed data with extended size
 */
template <OperationReadyData DataType>
DataType transform_delay(DataType& input, u_int32_t delay_samples, double fill_value, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t i = 0; i < target_data.size(); ++i) {
        std::vector<double> original_data(target_data[i].begin(), target_data[i].end());

        working_buffer[i].resize(original_data.size() + delay_samples);
        std::fill_n(working_buffer[i].begin(), delay_samples, fill_value);
        std::ranges::copy(original_data, working_buffer[i].begin() + delay_samples);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Fade transformation (linear fade-in and fade-out) using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param fade_in_duration_ratio Fade-in duration as a ratio of total length (0.0 to 1.0)
 * @param fade_out_duration_ratio Fade-out duration as a ratio of total length (0.0 to 1.0)
 * @return Faded data
 */
template <OperationReadyData DataType>
DataType transform_fade(DataType& input, double fade_in_duration_ratio = 0.0, double fade_out_duration_ratio = 0.0)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        auto fade_in_samples = static_cast<size_t>(span.size() * fade_in_duration_ratio);
        auto fade_out_samples = static_cast<size_t>(span.size() * fade_out_duration_ratio);
        size_t fade_out_start = span.size() - fade_out_samples;

        for (size_t j = 0; j < fade_in_samples && fade_in_samples > 1; ++j) {
            double fade_factor = static_cast<double>(j) / static_cast<double>(fade_in_samples - 1);
            span[j] *= fade_factor;
        }

        for (size_t j = 0; j < fade_out_samples && fade_out_samples > 1 && fade_out_start + j < span.size(); ++j) {
            double fade_factor = 1.0 - (static_cast<double>(j) / static_cast<double>(fade_out_samples - 1));
            span[fade_out_start + j] *= fade_factor;
        }
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Fade transformation (linear fade-in and fade-out) using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param fade_in_duration_ratio Fade-in duration as a ratio of total length (0.0 to 1.0)
 * @param fade_out_duration_ratio Fade-out duration as a ratio of total length (0.0 to 1.0)
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Faded data
 */
template <OperationReadyData DataType>
DataType transform_fade(DataType& input, double fade_in_duration_ratio, double fade_out_duration_ratio, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t i = 0; i < target_data.size(); ++i) {
        auto& span = target_data[i];
        working_buffer[i].assign(span.begin(), span.end());

        auto fade_in_samples = static_cast<size_t>(span.size() * fade_in_duration_ratio);
        auto fade_out_samples = static_cast<size_t>(span.size() * fade_out_duration_ratio);
        size_t fade_out_start = span.size() - fade_out_samples;

        for (size_t j = 0; j < fade_in_samples && fade_in_samples > 1; ++j) {
            double fade_factor = static_cast<double>(j) / static_cast<double>(fade_in_samples - 1);
            working_buffer[i][j] *= fade_factor;
        }

        for (size_t j = 0; j < fade_out_samples && fade_out_samples > 1 && fade_out_start + j < span.size(); ++j) {
            double fade_factor = 1.0 - (static_cast<double>(j) / static_cast<double>(fade_out_samples - 1));
            working_buffer[i][fade_out_start + j] *= fade_factor;
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Slice transformation to extract a portion of the data based on ratios (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param start_ratio Start ratio (0.0 to 1.0)
 * @param end_ratio End ratio (0.0 to 1.0)
 * @return Sliced data
 */
template <OperationReadyData DataType>
DataType transform_slice(DataType& input, double start_ratio, double end_ratio)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<std::vector<double>> result(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        size_t start_idx = static_cast<size_t>(target_data[i].size() * std::clamp(start_ratio, 0.0, 1.0));
        size_t end_idx = static_cast<size_t>(target_data[i].size() * std::clamp(end_ratio, 0.0, 1.0));

        if (start_idx >= end_idx || end_idx > target_data[i].size()) {
            result[i] = { 0.0 };
        } else {
            result[i].assign(target_data[i].begin() + start_idx, target_data[i].begin() + end_idx);
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(result, structure_info);
}

/**
 * @brief Slice transformation to extract a portion of the data based on ratios (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param start_ratio Start ratio (0.0 to 1.0)
 * @param end_ratio End ratio (0.0 to 1.0)
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Sliced data
 */
template <OperationReadyData DataType>
DataType transform_slice(DataType& input, double start_ratio, double end_ratio, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t i = 0; i < target_data.size(); ++i) {
        size_t start_idx = static_cast<size_t>(target_data[i].size() * std::clamp(start_ratio, 0.0, 1.0));
        size_t end_idx = static_cast<size_t>(target_data[i].size() * std::clamp(end_ratio, 0.0, 1.0));

        if (start_idx >= end_idx || end_idx > target_data[i].size()) {
            working_buffer[i] = { 0.0 };
        } else {
            working_buffer[i].assign(target_data[i].begin() + start_idx, target_data[i].begin() + end_idx);
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

} // namespace MayaFlux::Yantra
