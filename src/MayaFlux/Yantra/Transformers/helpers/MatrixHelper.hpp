#pragma once

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"
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
 * @brief Region-selective transformation using container-based extraction (IN-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED (serves as the target)
 * @param container Signal source container
 * @param regions Regions to transform and place back into input
 * @param transform_func Transformation function
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, DataType>
DataType transform_regions(DataType& input,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::vector<Kakshya::Region>& regions,
    TransformFunc transform_func)
{
    if (!container) {
        throw std::invalid_argument("Container is required for region-based transformations");
    }

    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (const auto& region : regions) {
        auto region_data = container->get_region_data(region);
        auto transformed_region = transform_func(region_data);
        auto transformed_doubles = OperationHelper::extract_as_double(transformed_region);

        auto start_sample = static_cast<u_int64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<u_int64_t>(region.end_coordinates[0]);

        if (start_sample < target_data.size() && end_sample <= target_data.size()) {
            auto target_span = target_data.subspan(start_sample, end_sample - start_sample);
            auto copy_size = std::min(transformed_doubles.size(), target_span.size());

            std::ranges::copy(transformed_doubles | std::views::take(copy_size),
                target_span.begin());
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(target_data.begin(), target_data.end()), structure_info);
}

/**
 * @brief Region-selective transformation using container-based extraction (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param container Signal source container
 * @param regions Regions to transform and place back into copy
 * @param transform_func Transformation function
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, DataType>
DataType transform_regions(DataType& input,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::vector<Kakshya::Region>& regions,
    TransformFunc transform_func,
    std::vector<double>& working_buffer)
{
    if (!container) {
        throw std::invalid_argument("Container is required for region-based transformations");
    }

    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (const auto& region : regions) {
        auto region_data = container->get_region_data(region);
        auto transformed_region = transform_func(region_data);
        auto transformed_doubles = OperationHelper::extract_as_double(transformed_region);

        auto start_sample = static_cast<u_int64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<u_int64_t>(region.end_coordinates[0]);

        if (start_sample < target_data.size() && end_sample <= target_data.size()) {
            auto target_span = target_data.subspan(start_sample, end_sample - start_sample);
            auto copy_size = std::min(transformed_doubles.size(), target_span.size());

            std::ranges::copy(transformed_doubles | std::views::take(copy_size),
                target_span.begin());
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Energy-based transformation using existing EnergyAnalyzer (IN-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED
 * @param energy_threshold Energy threshold for transformation
 * @param transform_func Transformation function
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_by_energy(DataType& input,
    double energy_threshold,
    TransformFunc transform_func,
    u_int32_t window_size = 1024,
    u_int32_t hop_size = 512)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    energy_analyzer->set_parameter("method", static_cast<int>(EnergyMethod::RMS));
    auto energy_result = energy_analyzer->analyze_energy(input);

    for (size_t i = 0; i < energy_result.energy_values.size(); ++i) {
        if (energy_result.energy_values[i] > energy_threshold && i < energy_result.window_positions.size()) {
            auto [start_idx, end_idx] = energy_result.window_positions[i];
            if (start_idx < data_span.size() && end_idx <= data_span.size()) {
                auto window_span = data_span.subspan(start_idx, end_idx - start_idx);
                std::ranges::transform(window_span, window_span.begin(), transform_func);
            }
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Energy-based transformation using existing EnergyAnalyzer (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param energy_threshold Energy threshold for transformation
 * @param transform_func Transformation function
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_by_energy(DataType& input,
    double energy_threshold,
    TransformFunc transform_func,
    u_int32_t window_size,
    u_int32_t hop_size,
    std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

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

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Statistical outlier transformation using existing StatisticalAnalyzer (IN-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED
 * @param std_dev_threshold Standard deviation threshold for outliers
 * @param transform_func Transformation function
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_outliers(DataType& input,
    double std_dev_threshold,
    TransformFunc transform_func)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    auto stat_analyzer = std::make_shared<StandardStatisticalAnalyzer>();
    auto stats = stat_analyzer->analyze_statistics(input);

    double threshold_low = stats.mean_stat - std_dev_threshold * stats.stat_std_dev;
    double threshold_high = stats.mean_stat + std_dev_threshold * stats.stat_std_dev;

    std::ranges::transform(data_span, data_span.begin(),
        [&](double x) {
            return (x < threshold_low || x > threshold_high) ? transform_func(x) : x;
        });

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Statistical outlier transformation using existing StatisticalAnalyzer (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param std_dev_threshold Standard deviation threshold for outliers
 * @param transform_func Transformation function
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_outliers(DataType& input,
    double std_dev_threshold,
    TransformFunc transform_func,
    std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto stat_analyzer = std::make_shared<StandardStatisticalAnalyzer>();
    auto stats = stat_analyzer->analyze_statistics(input);

    double threshold_low = stats.mean_stat - std_dev_threshold * stats.stat_std_dev;
    double threshold_high = stats.mean_stat + std_dev_threshold * stats.stat_std_dev;

    std::ranges::transform(target_data, target_data.begin(),
        [&](double x) {
            return (x < threshold_low || x > threshold_high) ? transform_func(x) : x;
        });

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Cross-fade between regions with smooth transitions (IN-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - WILL BE MODIFIED
 * @param fade_regions Pairs of regions to crossfade
 * @param fade_duration Duration of fade in samples
 * @return Crossfaded data
 */
template <ComputeData DataType>
DataType transform_crossfade_regions(DataType& input,
    const std::vector<std::pair<Kakshya::Region, Kakshya::Region>>& fade_regions,
    u_int32_t fade_duration)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::ranges::for_each(fade_regions, [&](const auto& fade_pair) {
        const auto& [region_a, region_b] = fade_pair;

        auto start_a = static_cast<u_int64_t>(region_a.start_coordinates[0]);
        auto end_a = static_cast<u_int64_t>(region_a.end_coordinates[0]);
        auto start_b = static_cast<u_int64_t>(region_b.start_coordinates[0]);

        u_int64_t fade_start = (end_a > fade_duration) ? end_a - fade_duration : 0;
        u_int64_t fade_end = std::min(start_b + fade_duration, static_cast<u_int64_t>(data_span.size()));

        if (fade_start < fade_end && fade_start < data_span.size()) {
            auto fade_span = data_span.subspan(fade_start, fade_end - fade_start);

            auto fade_indices = std::views::iota(size_t { 0 }, fade_span.size());
            std::ranges::for_each(fade_indices, [&](size_t i) {
                double ratio = static_cast<double>(i) / (fade_span.size() - 1);
                double smooth_ratio = 0.5 * (1.0 - std::cos(ratio * M_PI));
                fade_span[i] *= (1.0 - smooth_ratio);
            });
        }
    });

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Cross-fade between regions with smooth transitions (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - will NOT be modified
 * @param fade_regions Pairs of regions to crossfade
 * @param fade_duration Duration of fade in samples
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Crossfaded data
 */
template <ComputeData DataType>
DataType transform_crossfade_regions(DataType& input,
    const std::vector<std::pair<Kakshya::Region, Kakshya::Region>>& fade_regions,
    u_int32_t fade_duration,
    std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    std::ranges::for_each(fade_regions, [&](const auto& fade_pair) {
        const auto& [region_a, region_b] = fade_pair;

        auto start_a = static_cast<u_int64_t>(region_a.start_coordinates[0]);
        auto end_a = static_cast<u_int64_t>(region_a.end_coordinates[0]);
        auto start_b = static_cast<u_int64_t>(region_b.start_coordinates[0]);

        u_int64_t fade_start = (end_a > fade_duration) ? end_a - fade_duration : 0;
        u_int64_t fade_end = std::min(start_b + fade_duration, static_cast<u_int64_t>(target_data.size()));

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

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Matrix transformation using existing infrastructure (IN-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - WILL BE MODIFIED
 * @param transformation_matrix Matrix to apply
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix(DataType& input, const Eigen::MatrixXd& transformation_matrix)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (transformation_matrix.cols() == static_cast<long>(data_span.size())) {
        Eigen::Map<Eigen::VectorXd> data_vector(data_span.data(), data_span.size());
        Eigen::VectorXd result = transformation_matrix * data_vector;

        auto copy_size = std::min(result.size(), static_cast<long>(data_span.size()));
        std::ranges::copy(result.head(copy_size), data_span.begin());
    }

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Matrix transformation using existing infrastructure (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - will NOT be modified
 * @param transformation_matrix Matrix to apply
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix(DataType& input, const Eigen::MatrixXd& transformation_matrix, std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (transformation_matrix.cols() == static_cast<long>(target_data.size())) {
        Eigen::Map<Eigen::VectorXd> data_vector(target_data.data(), target_data.size());
        Eigen::VectorXd result = transformation_matrix * data_vector;

        working_buffer.resize(result.size());
        std::ranges::copy(result, working_buffer.begin());
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Multi-channel matrix transformation with error handling (IN-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - WILL BE MODIFIED
 * @param transformation_matrix Matrix to apply per channel
 * @param num_channels Number of channels
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix_multichannel(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    u_int32_t num_channels)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    if (transformation_matrix.rows() != num_channels || transformation_matrix.cols() != num_channels) {
        throw std::invalid_argument("Transformation matrix dimensions must match number of channels");
    }

    if (data_span.size() % num_channels != 0) {
        throw std::invalid_argument("Data size must be divisible by number of channels");
    }

    const size_t num_frames = data_span.size() / num_channels;

    auto frame_indices = std::views::iota(size_t { 0 }, num_frames);

    std::ranges::for_each(frame_indices, [&](size_t frame) {
        size_t frame_start = frame * num_channels;
        auto frame_data = data_span.subspan(frame_start, num_channels);

        Eigen::Map<Eigen::VectorXd> frame_vector(frame_data.data(), num_channels);

        Eigen::VectorXd transformed = transformation_matrix * frame_vector;

        std::ranges::copy(transformed, frame_data.begin());
    });

    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Multi-channel matrix transformation with error handling (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - will NOT be modified
 * @param transformation_matrix Matrix to apply per channel
 * @param num_channels Number of channels
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_matrix_multichannel(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    u_int32_t num_channels,
    std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (transformation_matrix.rows() != num_channels || transformation_matrix.cols() != num_channels) {
        throw std::invalid_argument("Transformation matrix dimensions must match number of channels");
    }

    if (target_data.size() % num_channels != 0) {
        throw std::invalid_argument("Data size must be divisible by number of channels");
    }

    const size_t num_frames = target_data.size() / num_channels;

    auto frame_indices = std::views::iota(size_t { 0 }, num_frames);

    std::ranges::for_each(frame_indices, [&](size_t frame) {
        size_t frame_start = frame * num_channels;
        auto frame_data = target_data.subspan(frame_start, num_channels);

        Eigen::Map<Eigen::VectorXd> frame_vector(frame_data.data(), num_channels);

        Eigen::VectorXd transformed = transformation_matrix * frame_vector;

        std::ranges::copy(transformed, frame_data.begin());
    });

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Channel operations using C++20 ranges (IN-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - WILL BE MODIFIED
 * @param num_channels Number of channels
 * @param interleave Whether to interleave or deinterleave
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_channel_operation(DataType& input,
    u_int32_t num_channels,
    bool interleave)
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

    std::ranges::copy(result, data_span.begin());
    return OperationHelper::reconstruct_from_double<DataType>(
        std::vector<double>(data_span.begin(), data_span.end()), structure_info);
}

/**
 * @brief Channel operations using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType ComputeData type
 * @param input Input data - will NOT be modified
 * @param num_channels Number of channels
 * @param interleave Whether to interleave or deinterleave
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <ComputeData DataType>
DataType transform_channel_operation(DataType& input,
    u_int32_t num_channels,
    bool interleave,
    std::vector<double>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (target_data.size() % num_channels != 0) {
        throw std::invalid_argument("Data size must be divisible by number of channels");
    }

    const size_t num_frames = target_data.size() / num_channels;
    working_buffer.resize(target_data.size());

    if (interleave) {
        auto frame_indices = std::views::iota(size_t { 0 }, num_frames);
        std::ranges::for_each(frame_indices, [&](size_t frame) {
            auto channel_indices = std::views::iota(size_t { 0 }, num_channels);
            std::ranges::for_each(channel_indices, [&](size_t channel) {
                size_t src_idx = channel * num_frames + frame;
                size_t dst_idx = frame * num_channels + channel;
                working_buffer[dst_idx] = target_data[src_idx];
            });
        });
    } else {
        auto frame_indices = std::views::iota(size_t { 0 }, num_frames);
        std::ranges::for_each(frame_indices, [&](size_t frame) {
            auto channel_indices = std::views::iota(size_t { 0 }, num_channels);
            std::ranges::for_each(channel_indices, [&](size_t channel) {
                size_t src_idx = frame * num_channels + channel;
                size_t dst_idx = channel * num_frames + frame;
                working_buffer[dst_idx] = target_data[src_idx];
            });
        });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

// Keep existing utility functions unchanged...
template <ComputeData DataType>
std::vector<Kakshya::Region> detect_regions_by_energy(const DataType& input,
    double energy_threshold,
    u_int32_t min_region_size = 512,
    u_int32_t window_size = 1024,
    u_int32_t hop_size = 512);

template <ComputeData DataType>
std::vector<double> extract_region_data(DataType& input,
    const std::vector<Kakshya::Region>& regions);

Eigen::MatrixXd create_rotation_matrix(double angle, u_int32_t axis = 2, u_int32_t dimensions = 2);
Eigen::MatrixXd create_scaling_matrix(const std::vector<double>& scale_factors);

} // namespace MayaFlux::Yantra
