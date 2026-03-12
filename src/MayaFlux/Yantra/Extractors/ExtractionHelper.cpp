#include "ExtractionHelper.hpp"

#include "MayaFlux/Kinesis/Discrete/Analysis.hpp"
#include "MayaFlux/Kinesis/Discrete/Extract.hpp"

namespace MayaFlux::Yantra {

namespace D = Kinesis::Discrete;

std::vector<std::vector<double>> extract_high_energy(
    const std::vector<std::span<const double>>& channels,
    double energy_threshold,
    uint32_t window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const uint32_t w = std::min(window_size, static_cast<uint32_t>(ch.size()));
        const uint32_t h = std::max(1U, std::min(hop_size, w / 2));
        const auto energy = D::rms(ch, D::num_windows(ch.size(), w, h), h, w);
        std::vector<size_t> starts;
        for (size_t i = 0; i < energy.size(); ++i) {
            if (energy[i] > energy_threshold)
                starts.push_back(i * h);
        }
        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::intervals_from_window_starts(starts, w, ch.size()))));
    }
    return result;
}

std::vector<std::vector<double>> extract_peaks(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    double min_distance,
    uint32_t region_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.size() < 3) {
            result.emplace_back();
            continue;
        }

        const auto [lo, hi] = std::ranges::minmax_element(ch);
        if (*hi - *lo < std::numeric_limits<double>::epsilon()) {
            result.emplace_back();
            continue;
        }
        const auto pos = D::peak_positions(ch, threshold, static_cast<size_t>(min_distance));
        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::regions_around_positions(pos, region_size / 2, ch.size()))));
    }

    return result;
}

std::vector<std::vector<double>> extract_outliers(
    const std::vector<std::span<const double>>& channels,
    double std_dev_threshold,
    uint32_t window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const uint32_t w = std::min(window_size, static_cast<uint32_t>(ch.size()));
        const uint32_t h = std::max(1U, std::min(hop_size, w / 2));
        const size_t nw = D::num_windows(ch.size(), w, h);
        const auto means = D::mean(ch, nw, h, w);

        if (means.empty()) {
            result.emplace_back();
            continue;
        }

        double gm = 0.0;
        for (double v : means)
            gm += v;
        gm /= static_cast<double>(means.size());

        double var = 0.0;
        for (double v : means)
            var += (v - gm) * (v - gm);
        const double gs = std::sqrt(var / static_cast<double>(means.size()));

        if (gs <= 0.0) {
            result.emplace_back();
            continue;
        }

        std::vector<size_t> starts;
        for (size_t i = 0; i < means.size(); ++i) {
            if (std::abs(means[i] - gm) > std_dev_threshold * gs)
                starts.push_back(i * h);
        }

        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::intervals_from_window_starts(starts, w, ch.size()))));
    }

    return result;
}

std::vector<std::vector<double>> extract_high_spectral(
    const std::vector<std::span<const double>>& channels,
    double spectral_threshold,
    uint32_t window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const uint32_t w = std::min(window_size, static_cast<uint32_t>(ch.size()));
        const uint32_t h = std::max(1U, std::min(hop_size, w / 2));
        const auto energy = D::spectral_energy(ch, D::num_windows(ch.size(), w, h), h, w);

        std::vector<size_t> starts;
        for (size_t i = 0; i < energy.size(); ++i) {
            if (energy[i] > spectral_threshold)
                starts.push_back(i * h);
        }

        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::intervals_from_window_starts(starts, w, ch.size()))));
    }
    return result;
}

std::vector<std::vector<double>> extract_above_mean(
    const std::vector<std::span<const double>>& channels,
    double mean_multiplier,
    uint32_t window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const uint32_t w = std::min(window_size, static_cast<uint32_t>(ch.size()));
        const uint32_t h = std::max(1U, std::min(hop_size, w / 2));
        const auto means = D::mean(ch, D::num_windows(ch.size(), w, h), h, w);
        double gm = 0.0;

        for (double v : means)
            gm += v;

        if (!means.empty())
            gm /= static_cast<double>(means.size());

        const double thr = gm * mean_multiplier;

        std::vector<size_t> starts;
        for (size_t i = 0; i < means.size(); ++i) {
            if (means[i] > thr)
                starts.push_back(i * h);
        }

        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::intervals_from_window_starts(starts, w, ch.size()))));
    }
    return result;
}

std::vector<std::vector<double>> extract_overlapping_windows(
    const std::vector<std::span<const double>>& channels,
    uint32_t window_size,
    double overlap)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels)
        result.push_back(D::overlapping_windows(ch, window_size, overlap));
    return result;
}

std::vector<std::vector<double>> extract_zero_crossings(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    double min_distance,
    uint32_t region_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty() || region_size == 0) {
            result.emplace_back();
            continue;
        }
        auto all = D::zero_crossing_positions(ch, threshold);
        std::vector<size_t> filtered;
        size_t last = 0;
        for (size_t p : all) {
            if (filtered.empty() || (p - last) >= static_cast<size_t>(min_distance)) {
                filtered.push_back(p);
                last = p;
            }
        }

        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::regions_around_positions(filtered, region_size / 2, ch.size()))));
    }
    return result;
}

std::vector<std::vector<double>> extract_silence(
    const std::vector<std::span<const double>>& channels,
    double silence_threshold,
    uint32_t min_duration,
    uint32_t window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const uint32_t w = std::min(window_size, static_cast<uint32_t>(ch.size()));
        const uint32_t h = std::max(1U, std::min(hop_size, w / 2));
        const auto energy = D::rms(ch, D::num_windows(ch.size(), w, h), h, w);
        std::vector<size_t> starts;
        for (size_t i = 0; i < energy.size(); ++i) {
            if (energy[i] <= silence_threshold && w >= min_duration) {
                starts.push_back(i * h);
            }
        }
        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::intervals_from_window_starts(starts, w, ch.size()))));
    }
    return result;
}

std::vector<std::vector<double>> extract_onsets(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    uint32_t region_size,
    uint32_t fft_window_size,
    uint32_t hop_size)
{
    std::vector<std::vector<double>> result;
    result.reserve(channels.size());
    for (const auto& ch : channels) {
        if (ch.empty()) {
            result.emplace_back();
            continue;
        }
        const auto pos = D::onset_positions(ch, fft_window_size, hop_size, threshold);
        result.push_back(D::slice_intervals(ch,
            D::merge_intervals(D::regions_around_positions(pos, region_size / 2, ch.size()))));
    }
    return result;
}

} // namespace MayaFlux::Yantra
