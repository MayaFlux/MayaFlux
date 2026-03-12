#include "MayaFlux/Kinesis/Discrete/Sort.hpp"

namespace MayaFlux::Kinesis::Discrete {

void sort_span(std::span<double> data, SortingDirection direction, SortingAlgorithm algorithm)
{
    if (data.empty())
        return;
    execute(data.begin(), data.end(), double_comparator(direction), algorithm);
}

std::span<double> sort_span_into(std::span<const double> data, std::vector<double>& output_storage, SortingDirection direction, SortingAlgorithm algorithm)
{
    output_storage.assign(data.begin(), data.end());
    sort_span(std::span<double>(output_storage), direction, algorithm);
    return { output_storage.data(), output_storage.size() };
}

void sort_channels(std::vector<std::span<double>>& channels, SortingDirection direction, SortingAlgorithm algorithm)
{
    for (auto& ch : channels)
        sort_span(ch, direction, algorithm);
}

std::vector<std::span<double>> sort_channels_into(const std::vector<std::span<const double>>& channels, std::vector<std::vector<double>>& output_storage, SortingDirection direction, SortingAlgorithm algorithm)
{
    output_storage.resize(channels.size());
    std::vector<std::span<double>> out;
    out.reserve(channels.size());
    for (size_t i = 0; i < channels.size(); ++i)
        out.push_back(sort_span_into(channels[i], output_storage[i], direction, algorithm));
    return out;
}

std::vector<size_t> span_sort_indices(std::span<double> data, SortingDirection direction)
{
    std::vector<size_t> idx(data.size());
    std::iota(idx.begin(), idx.end(), 0);
    if (data.empty())
        return idx;
    const auto comp = double_comparator(direction);
    std::ranges::sort(idx, [&](size_t a, size_t b) { return comp(data[a], data[b]); });
    return idx;
}

std::vector<std::vector<size_t>> channels_sort_indices(const std::vector<std::span<double>>& channels, SortingDirection direction)
{
    std::vector<std::vector<size_t>> out;
    out.reserve(channels.size());
    for (const auto& ch : channels)
        out.push_back(span_sort_indices(ch, direction));
    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
