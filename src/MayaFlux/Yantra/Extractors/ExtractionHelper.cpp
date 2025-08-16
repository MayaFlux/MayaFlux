#include "ExtractionHelper.hpp"

namespace {
/**
 * @brief Remove duplicate indices from extracted window positions
 * @param window_positions Vector of [start, end] pairs
 * @return Merged non-overlapping regions
 */
std::vector<std::pair<size_t, size_t>> merge_overlapping_windows(
    const std::vector<std::pair<size_t, size_t>>& window_positions)
{
    if (window_positions.empty()) {
        return {};
    }

    auto sorted_windows = window_positions;
    std::ranges::sort(sorted_windows, [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<std::pair<size_t, size_t>> merged;
    merged.push_back(sorted_windows[0]);

    for (size_t i = 1; i < sorted_windows.size(); ++i) {
        auto& last_merged = merged.back();
        const auto& current = sorted_windows[i];

        if (current.first <= last_merged.second) {
            last_merged.second = std::max(last_merged.second, current.second);
        } else {

            merged.push_back(current);
        }
    }

    return merged;
}
}

namespace MayaFlux::Yantra {

std::vector<double> extract_high_energy_data(
    std::span<const double> data,
    double energy_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    if (data.empty()) {
        return {};
    }

    u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(data.size()));
    u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
    if (effective_hop_size == 0)
        effective_hop_size = 1;

    if (!validate_extraction_parameters(effective_window_size, effective_hop_size, data.size())) {
        return {};
    }

    try {
        auto energy_analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(
            effective_window_size, effective_hop_size);
        energy_analyzer->set_parameter("method", "rms");

        std::vector<double> data_vec(data.begin(), data.end());
        Kakshya::DataVariant data_variant { data_vec };

        EnergyAnalysisResult energy_result = energy_analyzer->analyze_energy(data_variant);

        if (energy_result.energy_values.empty() || energy_result.window_positions.empty()) {
            return {};
        }

        std::vector<std::pair<size_t, size_t>> qualifying_windows;
        for (size_t i = 0; i < energy_result.energy_values.size(); ++i) {
            if (energy_result.energy_values[i] > energy_threshold) {
                auto [start_idx, end_idx] = energy_result.window_positions[i];
                if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
                    qualifying_windows.emplace_back(start_idx, end_idx);
                }
            }
        }

        auto merged_windows = merge_overlapping_windows(qualifying_windows);

        std::vector<double> extracted_data;
        for (const auto& [start_idx, end_idx] : merged_windows) {
            std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                std::back_inserter(extracted_data));
        }

        return extracted_data;
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<double> extract_peak_data(
    std::span<const double> data,
    double threshold,
    double min_distance,
    u_int32_t region_size)
{
    if (data.size() < 3) {
        return {};
    }

    std::vector<size_t> peak_indices;
    size_t last_peak = 0;

    for (size_t i = 1; i + 1 < data.size(); ++i) {
        if (data[i] > data[i - 1] && data[i] > data[i + 1] && data[i] > threshold && (i - last_peak) >= static_cast<size_t>(min_distance)) {
            peak_indices.push_back(i);
            last_peak = i;
        }
    }

    std::vector<double> extracted_data;
    for (const auto peak_idx : peak_indices) {
        const size_t half_region = region_size / 2;
        const size_t start_idx = (peak_idx >= half_region) ? peak_idx - half_region : 0;
        const size_t end_idx = std::min(peak_idx + half_region, data.size());

        if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
            std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                std::back_inserter(extracted_data));
        }
    }

    return extracted_data;
}

std::vector<double> extract_outlier_data(
    std::span<const double> data,
    double std_dev_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    if (data.empty() || data.size() < 3) {
        return {};
    }

    u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(data.size()));
    u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
    if (effective_hop_size == 0)
        effective_hop_size = 1;

    if (!validate_extraction_parameters(effective_window_size, effective_hop_size, data.size())) {
        return {};
    }

    try {
        auto stat_analyzer = std::make_shared<StatisticalAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(
            effective_window_size, effective_hop_size);
        stat_analyzer->set_parameter("method", "mean");

        std::vector<double> data_vec(data.begin(), data.end());
        Kakshya::DataVariant data_variant { data_vec };

        StatisticalAnalysisResult stat_result = stat_analyzer->analyze_statistics(data_variant);

        if (stat_result.statistical_values.empty() || stat_result.window_positions.empty()) {
            return {};
        }

        if (stat_result.stat_std_dev <= 0.0) {
            return {};
        }

        const double global_mean = stat_result.mean_stat;
        const double global_std_dev = stat_result.stat_std_dev;
        const double outlier_threshold = std_dev_threshold * global_std_dev;

        std::vector<std::pair<size_t, size_t>> qualifying_windows;
        for (size_t i = 0; i < stat_result.statistical_values.size(); ++i) {
            if (std::abs(stat_result.statistical_values[i] - global_mean) > outlier_threshold) {
                auto [start_idx, end_idx] = stat_result.window_positions[i];
                if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
                    qualifying_windows.emplace_back(start_idx, end_idx);
                }
            }
        }

        auto merged_windows = merge_overlapping_windows(qualifying_windows);

        std::vector<double> extracted_data;
        for (const auto& [start_idx, end_idx] : merged_windows) {
            std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                std::back_inserter(extracted_data));
        }

        return extracted_data;
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<double> extract_high_spectral_data(
    std::span<const double> data,
    double spectral_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    if (data.empty()) {
        return {};
    }

    u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(data.size()));
    u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
    if (effective_hop_size == 0)
        effective_hop_size = 1;

    if (!validate_extraction_parameters(effective_window_size, effective_hop_size, data.size())) {
        return {};
    }

    try {
        auto energy_analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(
            effective_window_size, effective_hop_size);
        energy_analyzer->set_parameter("method", "spectral");

        std::vector<double> data_vec(data.begin(), data.end());
        Kakshya::DataVariant data_variant { data_vec };

        EnergyAnalysisResult energy_result = energy_analyzer->analyze_energy(data_variant);

        if (energy_result.energy_values.empty() || energy_result.window_positions.empty()) {
            return {};
        }

        std::vector<std::pair<size_t, size_t>> qualifying_windows;
        for (size_t i = 0; i < energy_result.energy_values.size(); ++i) {
            if (energy_result.energy_values[i] > spectral_threshold) {
                auto [start_idx, end_idx] = energy_result.window_positions[i];
                if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
                    qualifying_windows.emplace_back(start_idx, end_idx);
                }
            }
        }

        auto merged_windows = merge_overlapping_windows(qualifying_windows);

        std::vector<double> extracted_data;
        for (const auto& [start_idx, end_idx] : merged_windows) {
            std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                std::back_inserter(extracted_data));
        }

        return extracted_data;
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<double> extract_above_mean_data(
    std::span<const double> data,
    double mean_multiplier,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    if (data.empty()) {
        return {};
    }

    if (!validate_extraction_parameters(window_size, hop_size, data.size())) {
        throw std::invalid_argument("Invalid extraction parameters");
    }

    try {
        auto stat_analyzer = std::make_shared<StatisticalAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(window_size, hop_size);
        stat_analyzer->set_parameter("method", "mean");

        std::vector<double> data_vec(data.begin(), data.end());
        Kakshya::DataVariant data_variant { data_vec };

        StatisticalAnalysisResult stat_result = stat_analyzer->analyze_statistics(data_variant);

        const double threshold = stat_result.mean_stat * mean_multiplier;

        std::vector<double> extracted_data;
        for (size_t i = 0; i < stat_result.statistical_values.size(); ++i) {
            if (stat_result.statistical_values[i] > threshold) {
                auto [start_idx, end_idx] = stat_result.window_positions[i];

                if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
                    std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                        std::back_inserter(extracted_data));
                }
            }
        }

        return extracted_data;
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<std::vector<double>> extract_overlapping_windows(
    std::span<const double> data,
    u_int32_t window_size,
    double overlap)
{
    if (data.empty() || window_size == 0 || window_size > data.size()) {
        return {};
    }

    if (overlap < 0.0 || overlap >= 1.0) {
        throw std::invalid_argument("Overlap must be between 0.0 and 1.0");
    }

    const auto hop_size = std::max(1U, static_cast<u_int32_t>(window_size * (1.0 - overlap)));

    std::vector<std::vector<double>> windows;

    for (size_t start = 0; start + window_size <= data.size(); start += hop_size) {
        windows.emplace_back(data.subspan(start, window_size).begin(),
            data.subspan(start, window_size).end());
    }

    return windows;
}

std::vector<std::vector<double>> extract_windowed_data_by_indices(
    std::span<const double> data,
    const std::vector<size_t>& window_indices,
    u_int32_t window_size)
{
    if (data.empty() || window_size == 0) {
        return {};
    }

    std::vector<std::vector<double>> windows;
    windows.reserve(window_indices.size());

    for (const auto start_idx : window_indices) {
        if (start_idx + window_size <= data.size()) {
            auto window_span = data.subspan(start_idx, window_size);
            windows.emplace_back(window_span.begin(), window_span.end());
        }
    }

    return windows;
}

std::vector<double> extract_data_from_regions(
    std::span<const double> data,
    const std::vector<Kakshya::Region>& regions)
{
    if (data.empty()) {
        return {};
    }

    std::vector<double> extracted_data;

    for (const auto& region : regions) {
        if (region.start_coordinates.empty() || region.end_coordinates.empty()) {
            continue;
        }

        const size_t start_idx = std::min(static_cast<size_t>(region.start_coordinates[0]), data.size());
        const size_t end_idx = std::min(static_cast<size_t>(region.end_coordinates[0]), data.size());

        if (start_idx < end_idx) {
            std::ranges::copy(data.subspan(start_idx, end_idx - start_idx),
                std::back_inserter(extracted_data));
        }
    }

    return extracted_data;
}

std::vector<double> extract_data_from_region_group(
    std::span<const double> data,
    const Kakshya::RegionGroup& region_group)
{
    return extract_data_from_regions(data, region_group.regions);
}

std::vector<std::string> get_available_extraction_methods()
{
    return {
        "high_energy_data",
        "peak_data",
        "outlier_data",
        "high_spectral_data",
        "above_mean_data",
        "overlapping_windows",
        "data_from_regions"
    };
}

bool validate_extraction_parameters(u_int32_t window_size, u_int32_t hop_size, size_t data_size)
{
    if (window_size == 0 || hop_size == 0) {
        return false;
    }

    if (data_size == 0) {
        return true;
    }

    if (data_size < window_size) {
        return data_size >= 3;
    }

    return true;
}

std::vector<double> extract_zero_crossing_data(
    std::span<const double> data,
    double threshold,
    double min_distance,
    u_int32_t region_size)
{
    if (data.empty() || region_size == 0)
        return {};

    try {
        auto analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(
            region_size * 2, static_cast<u_int32_t>(min_distance));
        analyzer->set_parameter("method", "zero_crossing");

        auto result = analyzer->analyze_energy(Kakshya::DataVariant { std::vector<double>(data.begin(), data.end()) });

        auto indices = std::views::iota(size_t { 0 }, result.energy_values.size())
            | std::views::filter([&result, threshold](size_t idx) {
                  return result.energy_values[idx] > threshold;
              });

        std::vector<double> extracted_data;
        for (auto idx : indices) {
            if (idx >= result.window_positions.size())
                continue;

            auto [start_idx, end_idx] = result.window_positions[idx];
            auto region_start = start_idx > region_size / 2 ? start_idx - region_size / 2 : size_t { 0 };
            auto region_end = std::min(start_idx + region_size / 2, data.size());

            if (region_start < region_end) {
                auto region = data.subspan(region_start, region_end - region_start);
                std::ranges::copy(region, std::back_inserter(extracted_data));
            }
        }

        return extracted_data;
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<double> extract_silence_data(
    std::span<const double> data,
    double silence_threshold,
    u_int32_t min_duration,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    if (data.empty())
        return {};

    auto [effective_window, effective_hop] = std::pair {
        std::min(window_size, static_cast<u_int32_t>(data.size())),
        std::max(1U, std::min(hop_size, window_size / 2))
    };

    try {
        auto analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>(
            effective_window, effective_hop);
        analyzer->set_parameter("method", "rms");
        analyzer->set_energy_thresholds(0.0, silence_threshold, silence_threshold * 2.0, silence_threshold * 5.0);
        analyzer->enable_classification(true);

        auto result = analyzer->analyze_energy(Kakshya::DataVariant { std::vector<double>(data.begin(), data.end()) });

        auto silent_indices = std::views::iota(size_t { 0 }, result.energy_classifications.size())
            | std::views::filter([&result](size_t idx) {
                  return result.energy_classifications[idx] == EnergyLevel::SILENT;
              });

        std::vector<std::pair<size_t, size_t>> regions;
        for (auto idx : silent_indices) {
            auto [start_idx, end_idx] = result.window_positions[idx];
            if (end_idx - start_idx >= min_duration) {
                regions.emplace_back(start_idx, end_idx);
            }
        }

        auto merged_regions = merge_overlapping_windows(regions);

        std::vector<double> extracted_data;
        for (const auto& [start_idx, end_idx] : merged_regions) {
            if (start_idx < data.size() && end_idx <= data.size() && start_idx < end_idx) {
                auto region = data.subspan(start_idx, end_idx - start_idx);
                std::ranges::copy(region, std::back_inserter(extracted_data));
            }
        }

        return extracted_data;

    } catch (const std::exception&) {
        return {};
    }
}

}
