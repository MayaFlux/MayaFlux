#include "Extract.hpp"

namespace MayaFlux::Kinesis::Discrete {

bool validate_window_parameters(uint32_t window_size, uint32_t hop_size, size_t data_size) noexcept
{
    if (window_size == 0 || hop_size == 0)
        return false;
    if (data_size == 0)
        return true;
    return data_size >= 3;
}

std::vector<std::pair<size_t, size_t>> merge_intervals(
    const std::vector<std::pair<size_t, size_t>>& intervals)
{
    if (intervals.empty())
        return {};

    auto sorted = intervals;
    std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<std::pair<size_t, size_t>> merged;
    merged.push_back(sorted[0]);

    for (size_t i = 1; i < sorted.size(); ++i) {
        auto& last = merged.back();
        const auto& cur = sorted[i];
        if (cur.first <= last.second) {
            last.second = std::max(last.second, cur.second);
        } else {
            merged.push_back(cur);
        }
    }

    return merged;
}

std::vector<double> slice_intervals(
    std::span<const double> data,
    const std::vector<std::pair<size_t, size_t>>& intervals)
{
    std::vector<double> out;
    for (const auto& [s, e] : intervals) {
        if (s < data.size() && e <= data.size() && s < e) {
            out.insert(out.end(), data.begin() + s, data.begin() + e);
        }
    }
    return out;
}

std::vector<std::pair<size_t, size_t>> regions_around_positions(
    const std::vector<size_t>& positions,
    size_t half_region,
    size_t data_size)
{
    std::vector<std::pair<size_t, size_t>> out;
    out.reserve(positions.size());
    for (size_t pos : positions) {
        const size_t s = (pos >= half_region) ? pos - half_region : 0;
        const size_t e = std::min(pos + half_region, data_size);
        if (s < e)
            out.emplace_back(s, e);
    }
    return out;
}

std::vector<std::pair<size_t, size_t>> intervals_from_window_starts(
    const std::vector<size_t>& window_starts,
    uint32_t window_size,
    size_t data_size)
{
    std::vector<std::pair<size_t, size_t>> out;
    out.reserve(window_starts.size());
    for (size_t s : window_starts) {
        const size_t e = std::min(s + static_cast<size_t>(window_size), data_size);
        if (s < e)
            out.emplace_back(s, e);
    }
    return out;
}

std::vector<double> overlapping_windows(
    std::span<const double> data,
    uint32_t window_size,
    double overlap)
{
    if (window_size == 0 || overlap < 0.0 || overlap >= 1.0 || data.size() < window_size)
        return {};

    const auto hop = std::max(1U, static_cast<uint32_t>(window_size * (1.0 - overlap)));
    std::vector<double> out;
    out.reserve(((data.size() - window_size) / hop + 1) * window_size);

    for (size_t start = 0; start + window_size <= data.size(); start += hop)
        out.insert(out.end(), data.begin() + start, data.begin() + start + window_size);

    return out;
}

std::vector<double> windowed_by_indices(
    std::span<const double> data,
    const std::vector<size_t>& window_starts,
    uint32_t window_size)
{
    std::vector<double> out;
    if (window_size == 0)
        return out;

    out.reserve(window_starts.size() * window_size);
    for (size_t s : window_starts) {
        if (s + window_size <= data.size()) {
            out.insert(out.end(), data.begin() + s, data.begin() + s + window_size);
        }
    }

    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
