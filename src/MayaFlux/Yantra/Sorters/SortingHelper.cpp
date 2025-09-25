#include "SortingHelper.hpp"

namespace MayaFlux::Yantra {

void sort_span_inplace(std::span<double> data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if (data.empty()) {
        return;
    }

    auto comp = create_double_comparator(direction);
    execute_sorting_algorithm(data.begin(), data.end(), comp, algorithm);
}

std::span<double> sort_span_extract(std::span<const double> data,
    std::vector<double>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    output_storage.assign(data.begin(), data.end());

    std::span<double> output_span(output_storage.data(), output_storage.size());
    sort_span_inplace(output_span, direction, algorithm);

    return output_span;
}

void sort_channels_inplace(std::vector<std::span<double>>& channels,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    for (auto& channel : channels) {
        sort_span_inplace(channel, direction, algorithm);
    }
}

std::vector<std::span<double>> sort_channels_extract(
    const std::vector<std::span<const double>>& channels,
    std::vector<std::vector<double>>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    output_storage.resize(channels.size());

    std::vector<std::span<double>> output_spans;
    output_spans.reserve(channels.size());

    for (size_t i = 0; i < channels.size(); ++i) {
        auto output_span = sort_span_extract(channels[i], output_storage[i], direction, algorithm);
        output_spans.push_back(output_span);
    }

    return output_spans;
}

std::vector<size_t> generate_span_sort_indices(std::span<double> data,
    SortingDirection direction)
{
    std::vector<size_t> indices(data.size());
    std::iota(indices.begin(), indices.end(), 0);

    if (data.empty()) {
        return indices;
    }

    auto comp = create_double_comparator(direction);
    std::ranges::sort(indices, [&](size_t a, size_t b) {
        return comp(data[a], data[b]);
    });

    return indices;
}

std::vector<std::vector<size_t>> generate_channels_sort_indices(
    const std::vector<std::span<double>>& channels,
    SortingDirection direction)
{
    std::vector<std::vector<size_t>> indices;
    indices.reserve(channels.size());

    for (const auto& channel : channels) {
        indices.push_back(generate_span_sort_indices(channel, direction));
    }

    return indices;
}

} // namespace MayaFlux::Yantra
