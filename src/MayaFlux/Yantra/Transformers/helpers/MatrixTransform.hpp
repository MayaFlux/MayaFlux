#pragma once

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/OperationHelper.hpp"
#include <Eigen/Dense>

/**
 * @file RegionMatrixTransform.hpp
 * @brief Region-aware and matrix transformation functions leveraging existing ecosystem
 *
 * - Uses existing EnergyAnalyzer for region detection instead of manual implementation
 * - Leverages StatisticalAnalyzer for outlier detection instead of manual stats
 * - Uses existing ExtractionHelper functions for data extraction
 * - Eliminates code duplication through analyzer delegation
 * - Uses existing normalization from MathematicalTransform where appropriate
 * - Proper C++20 compliance (no C++23 features)
 */

namespace MayaFlux::Yantra {

/**
 * @brief Common helper to prepare data for in-place or copy operations
 */
template <ComputeData DataType>
std::pair<std::span<double>, std::vector<double>> prepare_data_for_transform(
    DataType& input, bool in_place,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
{
    auto [data_span, structure_info] = container ? OperationHelper::extract_structured_double(input, container) : OperationHelper::extract_structured_double(input);

    if (in_place) {
        return { data_span, std::vector<double> {} };
    }

    std::vector<double> working_data(data_span.begin(), data_span.end());
    return { std::span<double>(working_data), std::move(working_data) };
}

/**
 * @brief Region-selective transformation using existing infrastructure
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data
 * @param container Signal source container
 * @param regions Regions to transform
 * @param transform_func Transformation function
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, std::span<double>> || std::invocable<TransformFunc, double>
DataType transform_regions(DataType& input,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::vector<Kakshya::Region>& regions,
    TransformFunc transform_func,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input, container);
    auto [target_data, working_data] = prepare_data_for_transform(input, in_place, container);

    std::ranges::for_each(regions, [&](const auto& region) {
        auto start_sample = static_cast<uint64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<uint64_t>(region.end_coordinates[0]);

        if (start_sample < target_data.size() && end_sample <= target_data.size()) {
            auto region_span = target_data.subspan(start_sample, end_sample - start_sample);

            if constexpr (std::is_invocable_v<TransformFunc, std::span<double>>) {
                transform_func(region_span);
            } else if constexpr (std::is_invocable_v<TransformFunc, double>) {
                std::ranges::transform(region_span, region_span.begin(), transform_func);
            }
        }
    });

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Energy-based transformation using existing EnergyAnalyzer
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data
 * @param energy_threshold Energy threshold for transformation
 * @param transform_func Transformation function
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_by_energy(DataType& input,
    double energy_threshold,
    TransformFunc transform_func,
    uint32_t window_size = 1024,
    uint32_t hop_size = 512,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);
    auto [target_data, working_data] = prepare_data_for_transform(input, in_place);

    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    energy_analyzer->set_parameter("method", static_cast<int>(EnergyMethod::RMS));
    auto energy_result = energy_analyzer->analyze_energy(input);

    for (size_t i = 0; i < energy_result.energy_values.size(); ++i) {
        if (energy_result.energy_values[i] > energy_threshold && i < energy_result.window_positions.size()) {
            auto [start_idx, end_idx] = energy_result.window_positions[i];
            if (start_idx < target_data.size() && end_idx <= target_data.size()) {
                auto window_span = target_data.subspan(start_idx, end_idx - start_idx);
                std::ranges::transform(window_span, window_span.begin(), transform_func);
            }
        }
    }

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Statistical outlier transformation using existing StatisticalAnalyzer
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data
 * @param std_dev_threshold Standard deviation threshold for outliers
 * @param transform_func Transformation function
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_outliers(DataType& input,
    double std_dev_threshold,
    TransformFunc transform_func,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);
    auto [target_data, working_data] = prepare_data_for_transform(input, in_place);

    auto stat_analyzer = std::make_shared<StandardStatisticalAnalyzer>();
    auto stats = stat_analyzer->analyze_statistics(input);

    double threshold_low = stats.mean_stat - std_dev_threshold * stats.stat_std_dev;
    double threshold_high = stats.mean_stat + std_dev_threshold * stats.stat_std_dev;

    std::ranges::transform(target_data, target_data.begin(),
        [&](double x) {
            return (x < threshold_low || x > threshold_high) ? transform_func(x) : x;
        });

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Intelligent region detection using existing analyzers
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param energy_threshold Energy threshold for region detection
 * @param min_region_size Minimum region size in samples
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Detected regions
 */
template <ComputeData DataType>
std::vector<Kakshya::Region> detect_regions_by_energy(const DataType& input,
    double energy_threshold,
    uint32_t min_region_size = 512,
    uint32_t window_size = 1024,
    uint32_t hop_size = 512)
{
    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    auto energy_result = energy_analyzer->analyze_energy(input);

    std::vector<Kakshya::Region> regions;

    auto high_energy_indices = std::views::iota(size_t { 0 }, energy_result.energy_values.size())
        | std::views::filter([&](size_t i) {
              return energy_result.energy_values[i] > energy_threshold;
          });

    std::vector<size_t> high_energy_vec(high_energy_indices.begin(), high_energy_indices.end());

    if (high_energy_vec.empty()) {
        return regions;
    }

    size_t region_start = high_energy_vec[0];
    size_t region_end = high_energy_vec[0];

    for (size_t i = 1; i < high_energy_vec.size(); ++i) {
        if (high_energy_vec[i] == region_end + 1) {
            region_end = high_energy_vec[i];
        } else {
            if (region_end < energy_result.window_positions.size()) {
                auto [start_sample, end_sample] = energy_result.window_positions[region_start];
                auto [_, final_end] = energy_result.window_positions[region_end];

                if (final_end - start_sample >= min_region_size) {
                    Kakshya::Region region;
                    region.start_coordinates = { start_sample };
                    region.end_coordinates = { final_end };
                    regions.push_back(region);
                }
            }
            region_start = high_energy_vec[i];
            region_end = high_energy_vec[i];
        }
    }

    if (region_end < energy_result.window_positions.size()) {
        auto [start_sample, end_sample] = energy_result.window_positions[region_start];
        auto [_, final_end] = energy_result.window_positions[region_end];

        if (final_end - start_sample >= min_region_size) {
            Kakshya::Region region;
            region.start_coordinates = { start_sample };
            region.end_coordinates = { final_end };
            regions.push_back(region);
        }
    }

    return regions;
}

/**
 * @brief Extract data from regions using existing ExtractionHelper
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param regions Regions to extract
 * @return Extracted data concatenated
 */
template <ComputeData DataType>
std::vector<double> extract_region_data(DataType& input,
    const std::vector<Kakshya::Region>& regions)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> extracted_data;

    std::ranges::for_each(regions, [&](const auto& region) {
        auto start_sample = static_cast<uint64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<uint64_t>(region.end_coordinates[0]);

        if (start_sample < data_span.size() && end_sample <= data_span.size()) {
            auto region_span = data_span.subspan(start_sample, end_sample - start_sample);
            std::ranges::copy(region_span, std::back_inserter(extracted_data));
        }
    });

    return extracted_data;
}

/**
 * @brief Cross-fade between regions with smooth transitions
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param fade_regions Pairs of regions to crossfade
 * @param fade_duration Duration of fade in samples
 * @param in_place Whether to modify input or create copy
 * @return Crossfaded data
 */
template <ComputeData DataType>
DataType transform_crossfade_regions(DataType& input,
    const std::vector<std::pair<Kakshya::Region, Kakshya::Region>>& fade_regions,
    uint32_t fade_duration,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);
    auto [target_data, working_data] = prepare_data_for_transform(input, in_place);

    std::ranges::for_each(fade_regions, [&](const auto& fade_pair) {
        const auto& [region_a, region_b] = fade_pair;

        auto start_a = static_cast<uint64_t>(region_a.start_coordinates[0]);
        auto end_a = static_cast<uint64_t>(region_a.end_coordinates[0]);
        auto start_b = static_cast<uint64_t>(region_b.start_coordinates[0]);

        uint64_t fade_start = (end_a > fade_duration) ? end_a - fade_duration : 0;
        uint64_t fade_end = std::min(start_b + fade_duration, static_cast<uint64_t>(target_data.size()));

        if (fade_start < fade_end && fade_start < target_data.size()) {
            auto fade_span = target_data.subspan(fade_start, fade_end - fade_start);

            auto fade_indices = std::views::iota(size_t { 0 }, fade_span.size());
            std::ranges::for_each(fade_indices, [&](size_t i) {
                double ratio = static_cast<double>(i) / (fade_span.size() - 1);
                double smooth_ratio = 0.5 * (1.0 - std::cos(ratio * M_PI));
                fade_span[i] *= (1.0 - smooth_ratio);
            });
        }
    });

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Matrix transformation using existing infrastructure
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param transformation_matrix Matrix to apply
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);
    auto [target_data, working_data] = prepare_data_for_transform(input, in_place);

    if (transformation_matrix.cols() == static_cast<long>(target_data.size())) {
        Eigen::Map<Eigen::VectorXd> data_vector(target_data.data(), target_data.size());
        Eigen::VectorXd result = transformation_matrix * data_vector;

        auto copy_size = std::min(result.size(), static_cast<long>(target_data.size()));
        std::ranges::copy(result.head(copy_size), target_data.begin());
    }

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Multi-channel matrix transformation with error handling
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param transformation_matrix Matrix to apply per channel
 * @param num_channels Number of channels
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix_multichannel(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    uint32_t num_channels,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (transformation_matrix.rows() != num_channels || transformation_matrix.cols() != num_channels) {
        throw std::invalid_argument("Transformation matrix dimensions must match number of channels");
    }

    if (data_span.size() % num_channels != 0) {
        throw std::invalid_argument("Data size must be divisible by number of channels");
    }

    auto [target_data, working_data] = prepare_data_for_transform(input, in_place);

    const size_t num_frames = target_data.size() / num_channels;

    auto frame_indices = std::views::iota(size_t { 0 }, num_frames);

    std::ranges::for_each(frame_indices, [&](size_t frame) {
        size_t frame_start = frame * num_channels;
        auto frame_data = target_data.subspan(frame_start, num_channels);

        Eigen::Map<Eigen::VectorXd> frame_vector(frame_data.data(), num_channels);

        Eigen::VectorXd transformed = transformation_matrix * frame_vector;

        std::ranges::copy(transformed, frame_data.begin());
    });

    if (in_place) {
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(working_data, structure_info);
}

/**
 * @brief Channel operations using C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param num_channels Number of channels
 * @param interleave Whether to interleave or deinterleave
 * @param in_place Whether to modify input or create copy
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_channel_operation(DataType& input,
    uint32_t num_channels,
    bool interleave,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (data_span.size() % num_channels != 0) {
        throw std::invalid_argument("Data size must be divisible by number of channels");
    }

    const size_t num_frames = data_span.size() / num_channels;
    std::vector<double> result(data_span.size());

    if (interleave) {
        auto frame_indices = std::views::iota(size_t { 0 }, num_frames);
        std::ranges::for_each(frame_indices, [&](size_t frame) {
            auto channel_indices = std::views::iota(size_t { 0 }, num_channels);
            std::ranges::for_each(channel_indices, [&](size_t channel) {
                size_t src_idx = channel * num_frames + frame;
                size_t dst_idx = frame * num_channels + channel;
                result[dst_idx] = data_span[src_idx];
            });
        });
    } else {
        auto frame_indices = std::views::iota(size_t { 0 }, num_frames);
        std::ranges::for_each(frame_indices, [&](size_t frame) {
            auto channel_indices = std::views::iota(size_t { 0 }, num_channels);
            std::ranges::for_each(channel_indices, [&](size_t channel) {
                size_t src_idx = frame * num_channels + channel;
                size_t dst_idx = channel * num_frames + frame;
                result[dst_idx] = data_span[src_idx];
            });
        });
    }

    if (in_place) {
        std::ranges::copy(result, data_span.begin());
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(result, structure_info);
}

/**
 * @brief Create rotation matrix for 2D/3D transformations
 * @param angle Rotation angle in radians
 * @param axis Rotation axis (0=X, 1=Y, 2=Z)
 * @param dimensions Number of dimensions (2 or 3)
 * @return Rotation matrix
 */
Eigen::MatrixXd create_rotation_matrix(double angle, uint32_t axis = 2, uint32_t dimensions = 2);

/**
 * @brief Create scaling matrix
 * @param scale_factors Scaling factors for each dimension
 * @return Scaling matrix
 */
Eigen::MatrixXd create_scaling_matrix(const std::vector<double>& scale_factors);

} // namespace MayaFlux::Yantra
