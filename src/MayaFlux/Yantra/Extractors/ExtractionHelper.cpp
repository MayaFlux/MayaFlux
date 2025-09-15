#include "ExtractionHelper.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"

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

std::vector<std::vector<double>> extract_high_energy_data(
    const std::vector<std::span<const double>>& data,
    double energy_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty()) {
            result.emplace_back();
            continue;
        }

        u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(channel.size()));
        u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
        if (effective_hop_size == 0)
            effective_hop_size = 1;

        if (!validate_extraction_parameters(effective_window_size, effective_hop_size, channel.size())) {
            result.emplace_back();
            continue;
        }

        try {
            auto energy_analyzer = std::make_shared<EnergyAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                effective_window_size, effective_hop_size);
            energy_analyzer->set_parameter("method", "rms");

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            EnergyAnalysis energy_result = energy_analyzer->analyze_energy(data_variant);

            if (energy_result.channels.empty() || energy_result.channels[0].energy_values.empty() || energy_result.channels[0].window_positions.empty()) {
                result.emplace_back();
                continue;
            }

            std::vector<std::pair<size_t, size_t>> qualifying_windows;
            const auto& ch_energy = energy_result.channels[0].energy_values;
            const auto& ch_windows = energy_result.channels[0].window_positions;
            for (size_t i = 0; i < ch_energy.size(); ++i) {
                if (ch_energy[i] > energy_threshold) {
                    auto [start_idx, end_idx] = ch_windows[i];
                    if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                        qualifying_windows.emplace_back(start_idx, end_idx);
                    }
                }
            }

            auto merged_windows = merge_overlapping_windows(qualifying_windows);

            std::vector<double> extracted_data;
            for (const auto& [start_idx, end_idx] : merged_windows) {
                std::ranges::copy(channel.subspan(start_idx, end_idx - start_idx),
                    std::back_inserter(extracted_data));
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

std::vector<std::vector<double>> extract_peak_data(
    const std::vector<std::span<const double>>& data,
    double threshold,
    double min_distance,
    u_int32_t region_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.size() < 3) {
            result.emplace_back();
            continue;
        }

        std::vector<size_t> peak_indices;
        size_t last_peak = 0;

        for (size_t i = 1; i + 1 < channel.size(); ++i) {
            if (channel[i] > channel[i - 1] && channel[i] > channel[i + 1] && channel[i] > threshold && (i - last_peak) >= static_cast<size_t>(min_distance)) {
                peak_indices.push_back(i);
                last_peak = i;
            }
        }

        std::vector<double> extracted_data;
        for (const auto peak_idx : peak_indices) {
            const size_t half_region = region_size / 2;
            const size_t start_idx = (peak_idx >= half_region) ? peak_idx - half_region : 0;
            const size_t end_idx = std::min(peak_idx + half_region, channel.size());

            if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                std::ranges::copy(channel.subspan(start_idx, end_idx - start_idx),
                    std::back_inserter(extracted_data));
            }
        }

        result.push_back(std::move(extracted_data));
    }

    return result;
}

std::vector<std::vector<double>> extract_outlier_data(
    const std::vector<std::span<const double>>& data,
    double std_dev_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty()) {
            result.emplace_back();
            continue;
        }

        u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(channel.size()));
        u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
        if (effective_hop_size == 0)
            effective_hop_size = 1;

        if (!validate_extraction_parameters(effective_window_size, effective_hop_size, channel.size())) {
            result.emplace_back();
            continue;
        }

        try {
            auto stat_analyzer = std::make_shared<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                effective_window_size, effective_hop_size);
            stat_analyzer->set_parameter("method", "mean");

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            ChannelStatistics stat_result = stat_analyzer->analyze_statistics(data_variant).channel_statistics[0];

            if (stat_result.statistical_values.empty() || stat_result.window_positions.empty() || stat_result.stat_std_dev <= 0.0) {
                result.emplace_back();
                continue;
            }

            const double global_mean = stat_result.mean_stat;
            const double global_std_dev = stat_result.stat_std_dev;
            const double outlier_threshold = std_dev_threshold * global_std_dev;

            std::vector<std::pair<size_t, size_t>> qualifying_windows;
            for (size_t i = 0; i < stat_result.statistical_values.size(); ++i) {
                if (std::abs(stat_result.statistical_values[i] - global_mean) > outlier_threshold) {
                    auto [start_idx, end_idx] = stat_result.window_positions[i];
                    if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                        qualifying_windows.emplace_back(start_idx, end_idx);
                    }
                }
            }

            auto merged_windows = merge_overlapping_windows(qualifying_windows);

            std::vector<double> extracted_data;
            for (const auto& [start_idx, end_idx] : merged_windows) {
                std::ranges::copy(channel.subspan(start_idx, end_idx - start_idx),
                    std::back_inserter(extracted_data));
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

std::vector<std::vector<double>> extract_high_spectral_data(
    const std::vector<std::span<const double>>& data,
    double spectral_threshold,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty()) {
            result.emplace_back();
            continue;
        }

        u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(channel.size()));
        u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
        if (effective_hop_size == 0)
            effective_hop_size = 1;

        if (!validate_extraction_parameters(effective_window_size, effective_hop_size, channel.size())) {
            result.emplace_back();
            continue;
        }

        try {
            auto energy_analyzer = std::make_shared<EnergyAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                effective_window_size, effective_hop_size);
            energy_analyzer->set_parameter("method", "spectral");

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            EnergyAnalysis energy_result = energy_analyzer->analyze_energy(data_variant);

            if (energy_result.channels.empty() || energy_result.channels[0].energy_values.empty() || energy_result.channels[0].window_positions.empty()) {
                result.emplace_back();
                continue;
            }

            std::vector<std::pair<size_t, size_t>> qualifying_windows;
            const auto& ch_energy = energy_result.channels[0].energy_values;
            const auto& ch_windows = energy_result.channels[0].window_positions;
            for (size_t i = 0; i < ch_energy.size(); ++i) {
                if (ch_energy[i] > spectral_threshold) {
                    auto [start_idx, end_idx] = ch_windows[i];
                    if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                        qualifying_windows.emplace_back(start_idx, end_idx);
                    }
                }
            }

            auto merged_windows = merge_overlapping_windows(qualifying_windows);

            std::vector<double> extracted_data;
            for (const auto& [start_idx, end_idx] : merged_windows) {
                std::ranges::copy(channel.subspan(start_idx, end_idx - start_idx),
                    std::back_inserter(extracted_data));
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

std::vector<std::vector<double>> extract_above_mean_data(
    const std::vector<std::span<const double>>& data,
    double mean_multiplier,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty()) {
            result.emplace_back();
            continue;
        }

        u_int32_t effective_window_size = std::min(window_size, static_cast<u_int32_t>(channel.size()));
        u_int32_t effective_hop_size = std::min(hop_size, effective_window_size / 2);
        if (effective_hop_size == 0)
            effective_hop_size = 1;

        if (!validate_extraction_parameters(effective_window_size, effective_hop_size, channel.size())) {
            result.emplace_back();
            continue;
        }

        try {
            auto stat_analyzer = std::make_shared<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                effective_window_size, effective_hop_size);
            stat_analyzer->set_parameter("method", "mean");

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            ChannelStatistics stat_result = stat_analyzer->analyze_statistics(data_variant).channel_statistics[0];

            if (stat_result.statistical_values.empty() || stat_result.window_positions.empty()) {
                result.emplace_back();
                continue;
            }

            const double threshold = stat_result.mean_stat * mean_multiplier;

            std::vector<std::pair<size_t, size_t>> qualifying_windows;
            for (size_t i = 0; i < stat_result.statistical_values.size(); ++i) {
                if (stat_result.statistical_values[i] > threshold) {
                    auto [start_idx, end_idx] = stat_result.window_positions[i];
                    if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                        qualifying_windows.emplace_back(start_idx, end_idx);
                    }
                }
            }

            auto merged_windows = merge_overlapping_windows(qualifying_windows);

            std::vector<double> extracted_data;
            for (const auto& [start_idx, end_idx] : merged_windows) {
                std::ranges::copy(channel.subspan(start_idx, end_idx - start_idx),
                    std::back_inserter(extracted_data));
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

std::vector<std::vector<double>> extract_overlapping_windows(
    const std::vector<std::span<const double>>& data,
    u_int32_t window_size,
    double overlap)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    if (window_size == 0 || overlap < 0.0 || overlap >= 1.0) {
        for (size_t i = 0; i < data.size(); ++i)
            result.emplace_back();
        return result;
    }

    for (const auto& channel : data) {
        if (channel.empty() || window_size > channel.size()) {
            result.emplace_back();
            continue;
        }

        const auto hop_size = std::max(1U, static_cast<u_int32_t>(window_size * (1.0 - overlap)));
        std::vector<double> channel_windows;

        for (size_t start = 0; start + window_size <= channel.size(); start += hop_size) {
            channel_windows.insert(channel_windows.end(),
                channel.begin() + start,
                channel.begin() + start + window_size);
        }

        result.push_back(std::move(channel_windows));
    }

    return result;
}

std::vector<std::vector<double>> extract_windowed_data_by_indices(
    const std::vector<std::span<const double>>& data,
    const std::vector<size_t>& window_indices,
    u_int32_t window_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        std::vector<double> channel_windows;
        if (channel.empty() || window_size == 0) {
            result.emplace_back();
            continue;
        }

        for (size_t start_idx : window_indices) {
            if (start_idx + window_size <= channel.size()) {
                auto window_span = channel.subspan(start_idx, window_size);
                channel_windows.insert(channel_windows.end(), window_span.begin(), window_span.end());
            }
        }
        result.push_back(std::move(channel_windows));
    }

    return result;
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

std::vector<std::vector<double>> extract_zero_crossing_data(
    const std::vector<std::span<const double>>& data,
    double threshold,
    double min_distance,
    u_int32_t region_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty() || region_size == 0) {
            result.emplace_back();
            continue;
        }

        try {
            auto analyzer = std::make_shared<EnergyAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                region_size * 2, static_cast<u_int32_t>(min_distance));
            analyzer->set_parameter("method", "zero_crossing");

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            EnergyAnalysis energy_result = analyzer->analyze_energy(data_variant);

            if (energy_result.channels.empty() || energy_result.channels[0].energy_values.empty() || energy_result.channels[0].window_positions.empty()) {
                result.emplace_back();
                continue;
            }

            const auto& ch_energy = energy_result.channels[0].energy_values;
            const auto& ch_windows = energy_result.channels[0].window_positions;

            std::vector<size_t> indices;
            for (size_t idx = 0; idx < ch_energy.size(); ++idx) {
                if (ch_energy[idx] > threshold)
                    indices.push_back(idx);
            }

            std::vector<double> extracted_data;
            for (auto idx : indices) {
                if (idx >= ch_windows.size())
                    continue;

                auto [start_idx, end_idx] = ch_windows[idx];
                auto region_start = start_idx > region_size / 2 ? start_idx - region_size / 2 : size_t { 0 };
                auto region_end = std::min(start_idx + region_size / 2, channel.size());

                if (region_start < region_end) {
                    auto region = channel.subspan(region_start, region_end - region_start);
                    std::ranges::copy(region, std::back_inserter(extracted_data));
                }
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

std::vector<std::vector<double>> extract_silence_data(
    const std::vector<std::span<const double>>& data,
    double silence_threshold,
    u_int32_t min_duration,
    u_int32_t window_size,
    u_int32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(data.size());

    for (const auto& channel : data) {
        if (channel.empty()) {
            result.emplace_back();
            continue;
        }

        u_int32_t effective_window = std::min(window_size, static_cast<u_int32_t>(channel.size()));
        u_int32_t effective_hop = std::max(1U, std::min(hop_size, window_size / 2));

        try {
            auto analyzer = std::make_shared<EnergyAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(
                effective_window, effective_hop);
            analyzer->set_parameter("method", "rms");
            analyzer->set_energy_thresholds(0.0, silence_threshold, silence_threshold * 2.0, silence_threshold * 5.0);
            analyzer->enable_classification(true);

            std::vector<Kakshya::DataVariant> data_variant { Kakshya::DataVariant { std::vector<double>(channel.begin(), channel.end()) } };
            EnergyAnalysis energy_result = analyzer->analyze_energy(data_variant);

            if (energy_result.channels.empty() || energy_result.channels[0].energy_values.empty() || energy_result.channels[0].window_positions.empty() || energy_result.channels[0].classifications.empty()) {
                result.emplace_back();
                continue;
            }

            const auto& classifications = energy_result.channels[0].classifications;
            const auto& window_positions = energy_result.channels[0].window_positions;

            std::vector<std::pair<size_t, size_t>> regions;
            for (size_t idx = 0; idx < classifications.size(); ++idx) {
                if (classifications[idx] == EnergyLevel::SILENT) {
                    auto [start_idx, end_idx] = window_positions[idx];
                    if (end_idx > start_idx && (end_idx - start_idx) >= min_duration) {
                        regions.emplace_back(start_idx, end_idx);
                    }
                }
            }

            auto merged_regions = merge_overlapping_windows(regions);

            std::vector<double> extracted_data;
            for (const auto& [start_idx, end_idx] : merged_regions) {
                if (start_idx < channel.size() && end_idx <= channel.size() && start_idx < end_idx) {
                    auto region = channel.subspan(start_idx, end_idx - start_idx);
                    std::ranges::copy(region, std::back_inserter(extracted_data));
                }
            }

            result.push_back(std::move(extracted_data));
        } catch (const std::exception&) {
            result.emplace_back();
        }
    }

    return result;
}

}
