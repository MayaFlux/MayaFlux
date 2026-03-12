#pragma once

#include "MayaFlux/Yantra/Data/DataIO.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "MayaFlux/Kinesis/Discrete/Sort.hpp"

namespace MayaFlux::Yantra {

using SortingDirection = Kinesis::Discrete::SortingDirection;
using SortingAlgorithm = Kinesis::Discrete::SortingAlgorithm;

/**
 * @struct SortKey
 * @brief Multi-dimensional sort key specification for complex sorting
 */
struct MAYAFLUX_API SortKey {
    std::string name;
    std::function<double(const std::any&)> extractor; ///< Extract sort value from data
    SortingDirection direction = SortingDirection::ASCENDING;
    double weight = 1.0; ///< Weight for multi-key sorting
    bool normalize = false; ///< Normalize values before sorting

    SortKey(std::string n, std::function<double(const std::any&)> e)
        : name(std::move(n))
        , extractor(std::move(e))
    {
    }
};

/**
 * @brief Universal sort function - handles extraction/conversion internally
 * @tparam T ComputeData type
 * @param data Data to sort (modified in-place)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <ComputeData T>
void sort_compute_data_inplace(Datum<T>& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        if (!data.has_container()) {
            throw std::runtime_error("Region-like types require container use UniversalSorter instead");
        }
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        Kinesis::Discrete::sort_channels(channels, direction, algorithm);
        return;
    }

    auto channels = OperationHelper::extract_numeric_data(data.data, data.needs_processig());
    Kinesis::Discrete::sort_channels(channels, direction, algorithm);
}

/**
 * @brief Universal sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <ComputeData T>
T sort_compute_data_extract(const T& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        static_assert(std::is_same_v<T, void>,
            "Region-like types require container parameter - use UniversalSorter instead");
        return T {};
    }

    std::vector<std::vector<double>> working_buffer;
    auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
        const_cast<T&>(data), working_buffer);

    Kinesis::Discrete::sort_channels(working_spans, direction, algorithm);

    return OperationHelper::reconstruct_from_double<T>(working_buffer, structure_info);
}

/**
 * @brief Universal sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <typename T>
T sort_compute_data_extract(const Datum<T>& data,
    SortingDirection direction,
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
    SortingDirection direction)
{
    if constexpr (RequiresContainer<T>) {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        return Kinesis::Discrete::channels_sort_indices(channels, direction);
    }

    if constexpr (SingleVariant<T>) {
        auto channel = OperationHelper::extract_numeric_data(data.data);
        return { Kinesis::Discrete::span_sort_indices({ channel }, direction) };
    } else {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.needs_processig());
        return Kinesis::Discrete::channels_sort_indices(channels, direction);
    }
}

/**
 * @brief Creates a multi-key comparator for complex sorting
 * @tparam T Data type being sorted
 * @param keys Vector of sort keys with extractors and weights
 * @return Lambda comparator that applies all keys in order
 */
template <typename T>
auto create_multi_key_comparator(const std::vector<SortKey>& keys)
{
    return [keys](const T& a, const T& b) -> bool {
        for (const auto& key : keys) {
            try {
                double val_a = key.extractor(std::any(a));
                double val_b = key.extractor(std::any(b));

                if (key.normalize) {
                    val_a = std::tanh(val_a);
                    val_b = std::tanh(val_b);
                }

                double weighted_diff = key.weight * (val_a - val_b);
                if (std::abs(weighted_diff) > 1e-9) {
                    return key.direction == SortingDirection::ASCENDING ? weighted_diff < 0 : weighted_diff > 0;
                }
            } catch (...) {
                continue;
            }
        }
        return false;
    };
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

/**
 * @brief Create universal sort key extractor for common data types
 * @tparam T Data type to extract sort key from
 * @param name Sort key name
 * @param direction Sort direction (for the key metadata)
 * @return SortKey with appropriate extractor
 */
template <typename T>
SortKey create_universal_sort_key(const std::string& name,
    SortingDirection direction = SortingDirection::ASCENDING)
{
    SortKey key(name, [](const std::any& data) -> double {
        auto cast_result = safe_any_cast<T>(data);
        if (!cast_result) {
            return 0.0;
        }

        const T& value = *cast_result.value;

        if constexpr (ArithmeticData<T>) {
            return static_cast<double>(value);
        } else if constexpr (ComplexData<T>) {
            return std::abs(value);
        } else if constexpr (requires { get_temporal_position(value); }) {
            return get_temporal_position(value);
        } else {
            return 0.0;
        }
    });

    key.direction = direction;
    return key;
}

} // namespace MayaFlux::Yantra
