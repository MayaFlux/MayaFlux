#pragma once

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"
#include <Eigen/Dense>

/**
 * @file MatrixHelper.hpp
 * @brief Region-aware and matrix transformation functions leveraging existing ecosystem
 *
 * - Uses EnergyAnalyzer for region detection instead of manual implementation
 * - Leverages StatisticalAnalyzer for outlier detection instead of manual stats
 * - Uses ExtractionHelper functions for data extraction
 * - Uses normalization from MathematicalTransform where appropriate
 */

namespace MayaFlux::Yantra {

/**
 * @brief Region-selective transformation using container-based extraction (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED (serves as the target)
 * @param container Signal source container
 * @param regions Regions to transform and place back into input
 * @param transform_func Transformation function
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
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
        auto transformed_doubles = OperationHelper::extract_numeric_data(transformed_region);

        auto start_sample = static_cast<u_int64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<u_int64_t>(region.end_coordinates[0]);

        for (size_t channel_idx = 0; channel_idx < target_data.size() && channel_idx < transformed_doubles.size(); ++channel_idx) {
            auto& target_channel = target_data[channel_idx];
            const auto& transformed_channel = transformed_doubles[channel_idx];

            if (start_sample < target_channel.size() && end_sample <= target_channel.size()) {
                auto target_span = target_channel.subspan(start_sample, end_sample - start_sample);
                auto copy_size = std::min(transformed_channel.size(), target_span.size());

                std::ranges::copy(transformed_channel | std::views::take(copy_size),
                    target_span.begin());
            }
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
 * @brief Region-selective transformation using container-based extraction (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param container Signal source container
 * @param regions Regions to transform and place back into copy
 * @param transform_func Transformation function
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, DataType>
DataType transform_regions(DataType& input,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::vector<Kakshya::Region>& regions,
    TransformFunc transform_func,
    std::vector<std::vector<double>>& working_buffer)
{
    if (!container) {
        throw std::invalid_argument("Container is required for region-based transformations");
    }

    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (const auto& region : regions) {
        auto region_data = container->get_region_data(region);
        auto transformed_region = transform_func(region_data);
        auto transformed_doubles = OperationHelper::extract_numeric_data(transformed_region);

        auto start_sample = static_cast<u_int64_t>(region.start_coordinates[0]);
        auto end_sample = static_cast<u_int64_t>(region.end_coordinates[0]);

        for (size_t channel_idx = 0; channel_idx < target_data.size() && channel_idx < transformed_doubles.size(); ++channel_idx) {
            auto& target_channel = target_data[channel_idx];
            const auto& transformed_channel = transformed_doubles[channel_idx];

            if (start_sample < target_channel.size() && end_sample <= target_channel.size()) {
                auto target_span = target_channel.subspan(start_sample, end_sample - start_sample);
                auto copy_size = std::min(transformed_channel.size(), target_span.size());

                std::ranges::copy(transformed_channel | std::views::take(copy_size),
                    target_span.begin());
            }
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Energy-based transformation using existing EnergyAnalyzer (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED
 * @param energy_threshold Energy threshold for transformation
 * @param transform_func Transformation function
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_by_energy(DataType& input,
    double energy_threshold,
    TransformFunc transform_func,
    u_int32_t window_size = 1024,
    u_int32_t hop_size = 512)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    energy_analyzer->set_energy_method(EnergyMethod::RMS);
    auto energy_result = energy_analyzer->analyze_energy(input);

    for (size_t ch = 0; ch < energy_result.channels.size() && ch < target_data.size(); ++ch) {
        const auto& channel_energy = energy_result.channels[ch];
        auto& target_channel = target_data[ch];

        for (size_t i = 0; i < channel_energy.energy_values.size(); ++i) {
            if (channel_energy.energy_values[i] > energy_threshold && i < channel_energy.window_positions.size()) {
                auto [start_idx, end_idx] = channel_energy.window_positions[i];

                if (start_idx < target_channel.size() && end_idx <= target_channel.size()) {
                    auto window_span = target_channel.subspan(start_idx, end_idx - start_idx);
                    std::ranges::transform(window_span, window_span.begin(), transform_func);
                }
            }
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
 * @brief Energy-based transformation using existing EnergyAnalyzer (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param energy_threshold Energy threshold for transformation
 * @param transform_func Transformation function
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_by_energy(DataType& input,
    double energy_threshold,
    TransformFunc transform_func,
    u_int32_t window_size,
    u_int32_t hop_size,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    energy_analyzer->set_energy_method(EnergyMethod::RMS);
    auto energy_result = energy_analyzer->analyze_energy(input);

    for (size_t ch = 0; ch < energy_result.channels.size() && ch < target_data.size(); ++ch) {
        const auto& channel_energy = energy_result.channels[ch];
        auto& target_channel = target_data[ch];

        for (size_t i = 0; i < channel_energy.energy_values.size(); ++i) {
            if (channel_energy.energy_values[i] > energy_threshold && i < channel_energy.window_positions.size()) {
                auto [start_idx, end_idx] = channel_energy.window_positions[i];

                if (start_idx < target_channel.size() && end_idx <= target_channel.size()) {
                    auto window_span = target_channel.subspan(start_idx, end_idx - start_idx);
                    std::ranges::transform(window_span, window_span.begin(), transform_func);
                }
            }
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Statistical outlier transformation using existing StatisticalAnalyzer (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - WILL BE MODIFIED
 * @param std_dev_threshold Standard deviation threshold for outliers
 * @param transform_func Transformation function
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_outliers(DataType& input,
    double std_dev_threshold,
    TransformFunc transform_func)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto stat_analyzer = std::make_shared<StandardStatisticalAnalyzer>();
    auto stats = stat_analyzer->analyze_statistics(input);

    if (stats.channel_statistics.empty()) {
        throw std::runtime_error("No channel statistics available for outlier detection");
    }
    const auto& first_channel_stats = stats.channel_statistics[0];
    double threshold_low = first_channel_stats.mean_stat - std_dev_threshold * first_channel_stats.stat_std_dev;
    double threshold_high = first_channel_stats.mean_stat + std_dev_threshold * first_channel_stats.stat_std_dev;

    for (auto& channel_span : target_data) {
        std::ranges::transform(channel_span, channel_span.begin(),
            [&](double x) {
                return (x < threshold_low || x > threshold_high) ? transform_func(x) : x;
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Statistical outlier transformation using existing StatisticalAnalyzer (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TransformFunc Function type for transformation
 * @param input Input data - will NOT be modified
 * @param std_dev_threshold Standard deviation threshold for outliers
 * @param transform_func Transformation function
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TransformFunc>
    requires std::invocable<TransformFunc, double>
DataType transform_outliers(DataType& input,
    double std_dev_threshold,
    TransformFunc transform_func,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto stat_analyzer = std::make_shared<StandardStatisticalAnalyzer>();
    auto stats = stat_analyzer->analyze_statistics(input);

    if (stats.channel_statistics.empty()) {
        throw std::runtime_error("No channel statistics available for outlier detection");
    }
    const auto& first_channel_stats = stats.channel_statistics[0];
    double threshold_low = first_channel_stats.mean_stat - std_dev_threshold * first_channel_stats.stat_std_dev;
    double threshold_high = first_channel_stats.mean_stat + std_dev_threshold * first_channel_stats.stat_std_dev;

    for (auto& channel_span : target_data) {
        std::ranges::transform(channel_span, channel_span.begin(),
            [&](double x) {
                return (x < threshold_low || x > threshold_high) ? transform_func(x) : x;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Cross-fade between regions with smooth transitions (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param fade_regions Pairs of regions to crossfade
 * @param fade_duration Duration of fade in samples
 * @return Crossfaded data
 */
template <OperationReadyData DataType>
DataType transform_crossfade_regions(DataType& input,
    const std::vector<std::pair<Kakshya::Region, Kakshya::Region>>& fade_regions,
    u_int32_t fade_duration)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    std::ranges::for_each(fade_regions, [&](const auto& fade_pair) {
        const auto& [region_a, region_b] = fade_pair;

        auto start_a = static_cast<u_int64_t>(region_a.start_coordinates[0]);
        auto end_a = static_cast<u_int64_t>(region_a.end_coordinates[0]);
        auto start_b = static_cast<u_int64_t>(region_b.start_coordinates[0]);

        u_int64_t fade_start = (end_a > fade_duration) ? end_a - fade_duration : 0;
        u_int64_t fade_end = std::min(start_b + fade_duration, static_cast<u_int64_t>(target_data[0].size()));

        for (auto& channel_span : target_data) {
            if (fade_start < fade_end && fade_start < channel_span.size()) {
                auto fade_span = channel_span.subspan(fade_start, fade_end - fade_start);

                auto fade_indices = std::views::iota(size_t { 0 }, fade_span.size());
                std::ranges::for_each(fade_indices, [&](size_t i) {
                    double ratio = static_cast<double>(i) / (fade_span.size() - 1);
                    double smooth_ratio = 0.5 * (1.0 - std::cos(ratio * M_PI));
                    fade_span[i] *= (1.0 - smooth_ratio);
                });
            }
        }
    });

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Cross-fade between regions with smooth transitions (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param fade_regions Pairs of regions to crossfade
 * @param fade_duration Duration of fade in samples
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Crossfaded data
 */
template <OperationReadyData DataType>
DataType transform_crossfade_regions(DataType& input,
    const std::vector<std::pair<Kakshya::Region, Kakshya::Region>>& fade_regions,
    u_int32_t fade_duration,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    std::ranges::for_each(fade_regions, [&target_data, fade_duration](const auto& fade_pair) {
        const auto& [region_a, region_b] = fade_pair;

        auto start_a = static_cast<u_int64_t>(region_a.start_coordinates[0]);
        auto end_a = static_cast<u_int64_t>(region_a.end_coordinates[0]);
        auto start_b = static_cast<u_int64_t>(region_b.start_coordinates[0]);

        u_int64_t fade_start = (end_a > fade_duration) ? end_a - fade_duration : 0;
        u_int64_t fade_end = std::min(start_b + fade_duration, static_cast<u_int64_t>(target_data[0].size()));

        for (auto& channel_span : target_data) {
            if (fade_start < fade_end && fade_start < channel_span.size()) {
                auto fade_span = channel_span.subspan(fade_start, fade_end - fade_start);

                auto fade_indices = std::views::iota(size_t { 0 }, fade_span.size());
                std::ranges::for_each(fade_indices, [&fade_span](size_t i) {
                    double ratio = static_cast<double>(i) / (fade_span.size() - 1);
                    double smooth_ratio = 0.5 * (1.0 - std::cos(ratio * M_PI));
                    fade_span[i] *= (1.0 - smooth_ratio);
                });
            }
        }
    });

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Matrix transformation using existing infrastructure (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param transformation_matrix Matrix to apply
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_matrix(DataType& input, const Eigen::MatrixXd& transformation_matrix)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& channel_span : target_data) {
        if (transformation_matrix.cols() == static_cast<long>(channel_span.size())) {
            Eigen::Map<Eigen::VectorXd> data_vector(channel_span.data(), channel_span.size());
            Eigen::VectorXd result = transformation_matrix * data_vector;

            auto copy_size = std::min<long>(result.size(), static_cast<long>(channel_span.size()));
            std::ranges::copy(result.head(copy_size), channel_span.begin());
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
 * @brief Matrix transformation using existing infrastructure (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param transformation_matrix Matrix to apply
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_matrix(DataType& input, const Eigen::MatrixXd& transformation_matrix, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t channel_idx = 0; channel_idx < target_data.size(); ++channel_idx) {
        auto& channel_span = target_data[channel_idx];
        if (transformation_matrix.cols() == static_cast<long>(channel_span.size())) {
            Eigen::Map<Eigen::VectorXd> data_vector(channel_span.data(), channel_span.size());
            Eigen::VectorXd result = transformation_matrix * data_vector;

            working_buffer[channel_idx].resize(result.size());
            std::ranges::copy(result, working_buffer[channel_idx].begin());
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Multi-channel matrix transformation with error handling (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param transformation_matrix Matrix to apply per channel
 * @param num_channels Number of channels
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_matrix_multichannel(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    u_int32_t num_channels)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    if (transformation_matrix.rows() != num_channels || transformation_matrix.cols() != num_channels) {
        throw std::invalid_argument("Transformation matrix dimensions must match number of channels");
    }

    if (target_data.size() != num_channels) {
        throw std::invalid_argument("Data channel count must match specified number of channels");
    }

    size_t min_frames = std::ranges::min(target_data | std::views::transform([](const auto& span) { return span.size(); }));

    auto frame_indices = std::views::iota(size_t { 0 }, min_frames);

    std::ranges::for_each(frame_indices, [&](size_t frame) {
        Eigen::VectorXd frame_vector(num_channels);

        for (size_t channel = 0; channel < num_channels; ++channel) {
            frame_vector[channel] = target_data[channel][frame];
        }

        Eigen::VectorXd transformed = transformation_matrix * frame_vector;

        for (size_t channel = 0; channel < num_channels; ++channel) {
            target_data[channel][frame] = transformed[channel];
        }
    });

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Multi-channel matrix transformation with error handling (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param transformation_matrix Matrix to apply per channel
 * @param num_channels Number of channels
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_matrix_multichannel(DataType& input,
    const Eigen::MatrixXd& transformation_matrix,
    u_int32_t num_channels,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (transformation_matrix.rows() != num_channels || transformation_matrix.cols() != num_channels) {
        throw std::invalid_argument("Transformation matrix dimensions must match number of channels");
    }

    if (target_data.size() != num_channels) {
        throw std::invalid_argument("Data channel count must match specified number of channels");
    }

    size_t min_frames = std::ranges::min(target_data | std::views::transform([](const auto& span) { return span.size(); }));

    auto frame_indices = std::views::iota(size_t { 0 }, min_frames);

    std::ranges::for_each(frame_indices, [&](size_t frame) {
        Eigen::VectorXd frame_vector(num_channels);

        for (size_t channel = 0; channel < num_channels; ++channel) {
            frame_vector[channel] = target_data[channel][frame];
        }

        Eigen::VectorXd transformed = transformation_matrix * frame_vector;

        for (size_t channel = 0; channel < num_channels; ++channel) {
            working_buffer[channel][frame] = transformed[channel];
        }
    });

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Channel operations using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param num_channels Number of channels
 * @param interleave Whether to interleave or deinterleave
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_channel_operation(DataType& input,
    u_int32_t num_channels,
    bool interleave)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    if (target_data.size() != num_channels) {
        throw std::invalid_argument("Data channel count must match specified number of channels");
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Channel operations using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param num_channels Number of channels
 * @param interleave Whether to interleave or deinterleave
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_channel_operation(DataType& input,
    u_int32_t num_channels,
    bool interleave,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (target_data.size() != num_channels) {
        throw std::invalid_argument("Data channel count must match specified number of channels");
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Detect regions based on energy threshold using existing EnergyAnalyzer
 * @tparam DataType OperationReadyData type
 * @param input Input data
 * @param energy_threshold Energy threshold for region detection
 * @param min_region_size Minimum size of detected regions in samples
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Detected regions
 */
template <OperationReadyData DataType>
std::vector<Kakshya::Region> detect_regions_by_energy(const DataType& input,
    double energy_threshold,
    u_int32_t min_region_size,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    auto energy_analyzer = std::make_shared<StandardEnergyAnalyzer>(window_size, hop_size);
    energy_analyzer->set_energy_method(EnergyMethod::RMS);
    auto energy_result = energy_analyzer->analyze_energy(input);

    std::vector<Kakshya::Region> regions;

    if (energy_result.channels.empty()) {
        return regions;
    }

    const auto& first_channel = energy_result.channels[0];
    std::optional<u_int64_t> region_start;

    for (size_t i = 0; i < first_channel.energy_values.size(); ++i) {
        bool above_threshold = first_channel.energy_values[i] > energy_threshold;

        if (above_threshold && !region_start.has_value()) {
            if (i < first_channel.window_positions.size()) {
                region_start = first_channel.window_positions[i].first;
            }
        } else if (!above_threshold && region_start.has_value()) {
            if (i < first_channel.window_positions.size()) {
                u_int64_t region_end = first_channel.window_positions[i].second;

                if (region_end - region_start.value() >= min_region_size) {
                    Kakshya::Region region;
                    region.start_coordinates = { region_start.value() };
                    region.end_coordinates = { region_end };
                    regions.push_back(region);
                }
                region_start.reset();
            }
        }
    }

    if (region_start.has_value() && !first_channel.window_positions.empty()) {
        u_int64_t region_end = first_channel.window_positions.back().second;
        if (region_end - region_start.value() >= min_region_size) {
            Kakshya::Region region;

            region.start_coordinates = { region_start.value() };
            region.end_coordinates = { region_end };
            regions.push_back(region);
        }
    }

    return regions;
}

Eigen::MatrixXd create_rotation_matrix(double angle, u_int32_t axis = 2, u_int32_t dimensions = 2);
Eigen::MatrixXd create_scaling_matrix(const std::vector<double>& scale_factors);

} // namespace MayaFlux::Yantra
