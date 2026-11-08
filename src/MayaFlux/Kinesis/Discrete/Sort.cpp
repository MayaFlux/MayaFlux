#include "MayaFlux/Kinesis/Discrete/Sort.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

/**
 * The mountain arrangement needs every element in its sorted position.
 * PARTIAL and NTH_ELEMENT only place some elements, so they run as a
 * complete sort here. Every other algorithm is already a complete sort.
 */
SortingAlgorithm full_sort_algorithm(SortingAlgorithm algorithm)
{
    if (algorithm == SortingAlgorithm::PARTIAL || algorithm == SortingAlgorithm::NTH_ELEMENT) {
        return SortingAlgorithm::STANDARD;
    }
    return algorithm;
}

} // namespace

std::vector<size_t> mountain_positions(size_t count)
{
    std::vector<size_t> positions(count);
    size_t left = 0;
    size_t right_taken = 0;
    for (size_t rank = 0; rank < count; ++rank) {
        if (rank % 2 == 0) {
            positions[rank] = left++;
        } else {
            positions[rank] = count - 1 - right_taken++;
        }
    }
    return positions;
}

void sort_span(std::span<double> data, const SortOrder& order, SortingAlgorithm algorithm)
{
    if (data.empty())
        return;

    if (order.direction == SortingDirection::BIDIRECTIONAL) {
        execute(data.begin(), data.end(), double_comparator(SortingDirection::ASCENDING), full_sort_algorithm(algorithm));
        const std::vector<double> sorted(data.begin(), data.end());
        const auto positions = mountain_positions(sorted.size());
        for (size_t rank = 0; rank < sorted.size(); ++rank) {
            data[positions[rank]] = sorted[rank];
        }
        return;
    }

    execute(data.begin(), data.end(), double_comparator(order), algorithm);
}

std::span<double> sort_span_into(std::span<const double> data, std::vector<double>& output_storage, const SortOrder& order, SortingAlgorithm algorithm)
{
    output_storage.assign(data.begin(), data.end());
    sort_span(std::span<double>(output_storage), order, algorithm);
    return { output_storage.data(), output_storage.size() };
}

void sort_channels(std::vector<std::span<double>>& channels, const SortOrder& order, SortingAlgorithm algorithm)
{
    for (auto& ch : channels)
        sort_span(ch, order, algorithm);
}

std::vector<std::span<double>> sort_channels_into(const std::vector<std::span<const double>>& channels, std::vector<std::vector<double>>& output_storage, const SortOrder& order, SortingAlgorithm algorithm)
{
    output_storage.resize(channels.size());
    std::vector<std::span<double>> out;
    out.reserve(channels.size());
    for (size_t i = 0; i < channels.size(); ++i)
        out.push_back(sort_span_into(channels[i], output_storage[i], order, algorithm));
    return out;
}

std::vector<size_t> span_sort_indices(std::span<double> data, const SortOrder& order)
{
    std::vector<size_t> idx(data.size());
    std::iota(idx.begin(), idx.end(), 0);
    if (data.empty())
        return idx;

    if (order.direction == SortingDirection::BIDIRECTIONAL) {
        std::ranges::sort(idx, [&](size_t a, size_t b) { return data[a] < data[b]; });
        std::vector<size_t> placed(idx.size());
        const auto positions = mountain_positions(idx.size());
        for (size_t rank = 0; rank < idx.size(); ++rank) {
            placed[positions[rank]] = idx[rank];
        }
        return placed;
    }

    const auto comp = double_comparator(order);
    std::ranges::sort(idx, [&](size_t a, size_t b) { return comp(data[a], data[b]); });
    return idx;
}

std::vector<std::vector<size_t>> channels_sort_indices(const std::vector<std::span<double>>& channels, const SortOrder& order)
{
    std::vector<std::vector<size_t>> out;
    out.reserve(channels.size());
    for (const auto& ch : channels)
        out.push_back(span_sort_indices(ch, order));
    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
