#pragma once

#include "MayaFlux/Yantra/Data/DataIO.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "MayaFlux/Kinesis/Discrete/Sort.hpp"

#include "MayaFlux/Transitive/Facet/Facets.hpp"

namespace MayaFlux::Yantra {

using SortingDirection = Kinesis::Discrete::SortingDirection;
using SortingAlgorithm = Kinesis::Discrete::SortingAlgorithm;
using SortOrder = Kinesis::Discrete::SortOrder;

/**
 * @struct SortKey
 * @brief One term of a weighted sort score.
 *
 * Reads one feature facet from each element. direction sets the sign of the
 * term, weight its size (must be positive), and normalize squashes the value
 * with tanh before weighting.
 */
struct MAYAFLUX_API SortKey {
    Facet<double> feature;
    SortingDirection direction { SortingDirection::ASCENDING };
    double weight { 1.0 };
    bool normalize { false };
};

/**
 * @brief Sort function - handles extraction/conversion internally
 * @tparam T ComputeData type
 * @param data Data to sort (modified in-place)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <ComputeData T>
void sort_compute_data_inplace(Datum<T>& data,
    SortOrder direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        if (!data.has_container()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Region-like types require container - use Sorter instead");
        }
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        Kinesis::Discrete::sort_channels(channels, direction, algorithm);
        return;
    }

    if constexpr (requires { OperationHelper::extract_numeric_data(data.data, data.needs_processig()); }) {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.needs_processig());
        Kinesis::Discrete::sort_channels(channels, direction, algorithm);
    } else {
        auto channels = OperationHelper::extract_numeric_data(data.data);
        Kinesis::Discrete::sort_channels(channels, direction, algorithm);
    }
}

/**
 * @brief Sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <ComputeData T>
T sort_compute_data_extract(const T& data,
    SortOrder direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        static_assert(std::is_same_v<T, void>,
            "Region-like types require container parameter - use Sorter instead");
        return T {};
    }

    std::vector<std::vector<double>> working_buffer;
    auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
        const_cast<T&>(data), working_buffer);

    Kinesis::Discrete::sort_channels(working_spans, direction, algorithm);

    return OperationHelper::reconstruct_from_double<T>(working_buffer, structure_info);
}

/**
 * @brief Sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <typename T>
T sort_compute_data_extract(const Datum<T>& data,
    SortOrder direction,
    SortingAlgorithm algorithm)
{
    std::vector<std::vector<double>> working_buffer;
    auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
        const_cast<Datum<T>&>(data), working_buffer);

    Kinesis::Discrete::sort_channels(working_spans, direction, algorithm);

    return OperationHelper::reconstruct_from_double<T>(working_buffer, structure_info);
}

/**
 * @brief Convenience function with default algorithm
 * @tparam T ComputeData type
 * @param data Data to sort
 * @param direction Sort direction
 * @return Sorted copy of the data
 */
template <ComputeData T>
T sort_compute_data(const T& data, SortingDirection direction = SortingDirection::ASCENDING)
{
    return sort_compute_data_extract(data, direction, SortingAlgorithm::STANDARD);
}

/**
 * @brief Generate sort indices for any ComputeData type
 * @tparam T ComputeData type
 * @param data Data to generate indices for
 * @param direction Sort direction
 * @return Vector of index vectors (one per channel)
 */
template <ComputeData T>
std::vector<std::vector<size_t>> generate_compute_data_indices(const Datum<T>& data,
    SortOrder direction)
{
    if constexpr (RequiresContainer<T>) {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        return Kinesis::Discrete::channels_sort_indices(channels, direction);
    }

    if constexpr (SingleVariant<T>) {
        auto channel = OperationHelper::extract_numeric_data(data.data);
        return { Kinesis::Discrete::span_sort_indices({ channel.front() }, direction) };
    } else {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.needs_processig());
        return Kinesis::Discrete::channels_sort_indices(channels, direction);
    }
}

/**
 * @brief Checks that every sort key has a positive, finite weight.
 * @throws std::invalid_argument when a weight is not a positive finite number.
 */
inline void validate_sort_keys(const std::vector<SortKey>& keys)
{
    for (const auto& key : keys) {
        if (!(key.weight > 0.0) || !std::isfinite(key.weight)) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Sort key weight must be a positive finite number");
        }
    }
}

/**
 * @brief Weighted score of one element across the sort keys.
 *
 * score = sum over keys of sign(direction) * weight * f(feature), where f is
 * tanh when normalize is set and the identity otherwise. Returns nullopt when
 * the element lacks the feature of any key, so the caller decides how to treat it.
 */
template <typename Element>
[[nodiscard]] std::optional<double> weighted_sort_score(const Element& element, std::span<const SortKey> keys)
{
    double score = 0.0;
    for (const auto& key : keys) {
        const double* value = element.get(key.feature);
        if (value == nullptr) {
            return std::nullopt;
        }
        const double x = key.normalize ? std::tanh(*value) : *value;
        const double sign = key.direction == SortingDirection::ASCENDING ? 1.0 : -1.0;
        score += sign * key.weight * x;
    }
    return score;
}

/**
 * @brief Helper function to get temporal position from various types
 * Used by TemporalSortable concept
 */
template <typename T>
double get_temporal_position(const T& item)
{
    if constexpr (requires { item.start_coordinates; }) {
        return !item.start_coordinates.empty() ? static_cast<double>(item.start_coordinates[0]) : 0.0;
    } else if constexpr (requires { item.timestamp; }) {
        return static_cast<double>(item.timestamp);
    } else if constexpr (requires { item.time; }) {
        return static_cast<double>(item.time);
    } else {
        static_assert(std::is_same_v<T, void>, "Type does not have temporal information");
        return 0.0;
    }
}

} // namespace MayaFlux::Yantra
