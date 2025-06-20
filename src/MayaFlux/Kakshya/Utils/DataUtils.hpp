#pragma once
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Calculate the total number of elements in an N-dimensional container.
 * @param dimensions Dimension descriptors.
 * @return Product of all dimension sizes.
 */
u_int64_t calculate_total_elements(const std::vector<DataDimension>& dimensions);

/**
 * @brief Calculate the frame size (number of elements per frame) for a set of dimensions.
 * @param dimensions Dimension descriptors.
 * @return Frame size (product of all but the first dimension).
 */
u_int64_t calculate_frame_size(const std::vector<DataDimension>& dimensions);

/**
 * @brief Extract a single frame of data from a vector.
 * @tparam T Data type.
 * @param data Source data vector.
 * @param frame_index Index of the frame to extract.
 * @param frame_size Number of elements per frame.
 * @return Vector containing the frame data.
 */
template <typename T>
std::vector<T> extract_frame(const std::vector<T>& data, u_int64_t frame_index, u_int64_t frame_size)
{
    u_int64_t start = frame_index * frame_size;
    u_int64_t end = std::min(start + frame_size, data.size());
    return std::vector<T>(data.begin() + start, data.begin() + end);
}

/**
 * @brief Convert a span of one data type to another (with type conversion).
 * @tparam From Source type.
 * @tparam To Destination type.
 * @param source Source data span.
 * @return Vector of converted data.
 */
template <typename From, typename To>
std::vector<To> convert_data_type(std::span<const From> source)
{
    std::vector<To> result;
    result.reserve(source.size());

    for (const auto& value : source) {
        if constexpr (std::is_same_v<From, std::complex<float>> || std::is_same_v<From, std::complex<double>>) {
            if constexpr (std::is_same_v<To, std::complex<float>> || std::is_same_v<To, std::complex<double>>) {
                result.push_back(static_cast<To>(value));
            } else {
                result.push_back(static_cast<To>(std::abs(value)));
            }
        } else {
            result.push_back(static_cast<To>(value));
        }
    }

    return result;
}

/**
 * @brief Extract a vector of type T from a DataVariant, with type conversion if needed.
 * @tparam T Desired type.
 * @param variant DataVariant to extract from.
 * @return Optional vector of type T.
 */
template <typename T>
std::optional<std::vector<T>> extract_from_variant(const DataVariant& variant)
{
    return std::visit([](const auto& data) -> std::optional<std::vector<T>> {
        using DataType = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<DataType, std::vector<T>>) {
            return data;
        } else if constexpr (std::is_arithmetic_v<typename DataType::value_type>) {
            return convert_data_type<typename DataType::value_type, T>(data);
        } else {
            return std::nullopt;
        }
    },
        variant);
}

/**
 * @brief Extract a value of type T from a DataVariant at a specific position.
 * @tparam T Desired type.
 * @param variant DataVariant to extract from.
 * @param pos Position in the data.
 * @return Optional value of type T.
 */
template <typename T>
std::optional<T> extract_from_variant_at(const DataVariant& variant, u_int64_t pos)
{
    return std::visit([pos](const auto& data) -> std::optional<T> {
        using DataType = std::decay_t<decltype(data)>;
        using ValueType = typename DataType::value_type;

        if (pos >= data.size()) {
            return std::nullopt;
        }

        if constexpr (std::is_same_v<ValueType, T>) {
            return data[pos];
        } else if constexpr (std::is_arithmetic_v<ValueType> && std::is_arithmetic_v<T>) {
            return static_cast<T>(data[pos]);
        } else if constexpr (std::is_same_v<ValueType, std::complex<float>> || std::is_same_v<ValueType, std::complex<double>>) {
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<T>(std::abs(data[pos]));
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    },
        variant);
}

/**
 * @brief Safely copy data from a DataVariant to another DataVariant, handling type conversion.
 * @param input Source DataVariant.
 * @param output Destination DataVariant.
 */
void safe_copy_data_variant(const DataVariant& input, DataVariant& output);

/**
 * @brief Safely copy data from a DataVariant to another DataVariant of a specific type.
 * @tparam T Data type.
 * @param input Source DataVariant.
 * @param output Destination DataVariant.
 */
template <typename T>
void safe_copy_typed_variant(const DataVariant& input, DataVariant& output)
{
    auto input_ptr = std::get_if<std::vector<T>>(&input);
    auto out_ptr = std::get_if<std::vector<T>>(&output);

    if (input_ptr && out_ptr) {
        std::copy(input_ptr->begin(), input_ptr->end(), out_ptr->begin());
    }
}

/**
 * @brief Safely copy data from a DataVariant to a span of doubles.
 * @param input Source DataVariant.
 * @param output Destination span of doubles.
 * @throws std::runtime_error if complex types are involved.
 */
void safe_copy_data_variant_to_span(const DataVariant& input, std::span<double> output);

/**
 * @brief Get a typed span from a DataVariant if the type matches.
 * @tparam T Data type.
 * @param data DataVariant to query.
 * @return Span of type T, or empty span if type does not match.
 */
template <typename T>
std::span<const T> get_typed_data(const DataVariant& data)
{
    if (std::holds_alternative<std::vector<T>>(data)) {
        const auto& vec = std::get<std::vector<T>>(data);
        return std::span<const T>(vec.data(), vec.size());
    }
    return {};
}

std::vector<double> convert_variant_to_double(const Kakshya::DataVariant& data);

/**
 * @brief Set a value in a metadata map (key-value).
 * @param metadata Metadata map.
 * @param key Key to set.
 * @param value Value to set.
 */
void set_metadata_value(std::unordered_map<std::string, std::any>& metadata, const std::string& key, std::any value);

/**
 * @brief Get a value from a metadata map by key.
 * @tparam T Expected type.
 * @param metadata Metadata map.
 * @param key Key to retrieve.
 * @return Optional value if present and convertible.
 */
template <typename T>
std::optional<T> get_metadata_value(const std::unordered_map<std::string, std::any>& metadata, const std::string& key)
{
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/**
 * @brief Find the index of a dimension by its semantic role.
 * @param dimensions Dimension descriptors.
 * @param role Semantic role to search for.
 * @return Index of the dimension, or -1 if not found.
 */
int find_dimension_by_role(const std::vector<DataDimension>& dimensions, DataDimension::Role role);

/**
 * @brief Extract a specific frame from container.
 * @param container The container to extract from.
 * @param frame_index Index of the frame to extract.
 * @return DataVariant containing frame data.
 */
DataVariant extract_frame_data(std::shared_ptr<SignalSourceContainer> container,
    u_int64_t frame_index);

/**
 * @brief Extract a slice of data with arbitrary coordinates.
 * @param container The container to extract from.
 * @param slice_start Starting coordinates for each dimension.
 * @param slice_end Ending coordinates for each dimension.
 * @return DataVariant containing sliced data.
 */
DataVariant extract_slice_data(std::shared_ptr<SignalSourceContainer> container,
    const std::vector<u_int64_t>& slice_start,
    const std::vector<u_int64_t>& slice_end);

/**
 * @brief Extract subsampled data from container.
 * @param container The container to extract from.
 * @param subsample_factor Factor to subsample by.
 * @param start_offset Optional starting offset.
 * @return DataVariant containing subsampled data.
 */
DataVariant extract_subsample_data(std::shared_ptr<SignalSourceContainer> container,
    u_int32_t subsample_factor,
    u_int64_t start_offset = 0);

/**
 * @brief Detects data modality from dimension information
 * @param dimensions Vector of data dimensions with role information
 * @return Detected DataModality type
 *
 * Consolidates modality detection logic that was duplicated across analyzers.
 * Uses dimension roles and count to determine appropriate processing approach.
 */
DataModality detect_data_modality(const std::vector<DataDimension>& dimensions);
}
