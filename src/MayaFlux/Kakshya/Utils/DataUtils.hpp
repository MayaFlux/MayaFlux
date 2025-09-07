#pragma once

#include "MayaFlux/Kakshya/NDData.hpp"

#include "MayaFlux/Utils.hpp"

#include <typeindex>

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
 * @brief Get type index from DataVariant
 */
std::type_index get_variant_type_index(const DataVariant& data);

/**
 * @brief Extract a single frame of data from a span.
 * @tparam T Data type.
 * @param data Source data span.
 * @param frame_index Index of the frame to extract.
 * @param frame_size Number of elements per frame.
 * @return Span containing the frame data.
 */
template <ProcessableData T>
constexpr std::span<T> extract_frame(std::span<T> data, u_int64_t frame_index, u_int64_t frame_size) noexcept
{
    u_int64_t start = frame_index * frame_size;
    u_int64_t end = std::min(start + frame_size, data.size());

    if (start >= data.size()) {
        return {};
    }

    return data.subspan(start, end - start);
}

/**
 * @brief Extract a single frame from planar data (returns interleaved).
 * @tparam T Data type.
 * @param channel_spans Vector of spans, one per channel.
 * @param frame_index Index of the frame to extract.
 * @param output_buffer Buffer to store interleaved frame data.
 * @return Span containing the interleaved frame data.
 */
template <ProcessableData T>
std::span<T> extract_frame(
    const std::vector<std::span<T>>& channel_spans,
    u_int64_t frame_index,
    std::vector<T>& output_buffer) noexcept
{
    output_buffer.clear();
    output_buffer.reserve(channel_spans.size());

    for (const auto& channel_span : channel_spans) {
        if (frame_index < channel_span.size()) {
            output_buffer.push_back(channel_span[frame_index]);
        } else {
            output_buffer.push_back(T { 0 });
        }
    }

    return std::span<T>(output_buffer.data(), output_buffer.size());
}

template <ProcessableData From, ProcessableData To>
    requires(ComplexData<From> && ArithmeticData<To>)
void convert_complex(std::span<From> source,
    std::span<To> destination,
    Utils::ComplexConversionStrategy strategy)
{
    const size_t count = std::min(source.size(), destination.size());

    for (size_t i = 0; i < count; ++i) {
        switch (strategy) {
        case Utils::ComplexConversionStrategy::MAGNITUDE:
            destination[i] = static_cast<To>(std::abs(source[i]));
            break;
        case Utils::ComplexConversionStrategy::REAL_PART:
            destination[i] = static_cast<To>(source[i].real());
            break;
        case Utils::ComplexConversionStrategy::IMAG_PART:
            destination[i] = static_cast<To>(source[i].imag());
            break;
        case Utils::ComplexConversionStrategy::SQUARED_MAGNITUDE:
            destination[i] = static_cast<To>(std::norm(source[i]));
            break;
        }
    }
}

/**
 * @brief Convert a span of one data type to another (with type conversion).
 * @tparam From Source type.
 * @tparam To Destination type.
 * @param source Source data span.
 * @param dest Destination data span.
 * @return Span of converted data.
 */
template <ProcessableData From, ProcessableData To>
std::span<To> convert_data(std::span<From> source,
    Utils::ComplexConversionStrategy strategy = Utils::ComplexConversionStrategy::MAGNITUDE)
{
    if constexpr (std::is_same_v<From, To>) {
        return source;
    }

    else if constexpr (ComplexData<From> && ArithmeticData<To>) {
        auto dest_span = std::span<To>(
            reinterpret_cast<To*>(source.data()),
            source.size());
        convert_complex(source, dest_span, strategy);
        return dest_span;
    }

    else if constexpr (ArithmeticData<From> && ComplexData<To>) {
        using RealType = typename To::value_type;
        To* dest = reinterpret_cast<To*>(source.data());
        const size_t dest_size = source.size();

        for (size_t i = 0; i < dest_size; ++i) {
            dest[i] = To(static_cast<RealType>(source[i]), RealType { 0 });
        }
        return { dest, dest_size };
    }

    else if constexpr (ArithmeticData<From> && ArithmeticData<To>) {
        To* dest = reinterpret_cast<To*>(source.data());
        const size_t count = source.size();

        for (size_t i = 0; i < count; ++i) {
            dest[i] = static_cast<To>(source[i]);
        }
        return { dest, count };
    }

    else {
        // TODO: some assert. dont know yet
        return {};
    }
}

template <ProcessableData T>
std::span<T> convert_variant(DataVariant& variant,
    Utils::ComplexConversionStrategy strategy = Utils::ComplexConversionStrategy::MAGNITUDE)
{
    if (std::holds_alternative<std::vector<T>>(variant)) {
        auto& vec = std::get<std::vector<T>>(variant);
        return std::span<T>(vec.data(), vec.size());
    }

    return std::visit([&variant, strategy](auto& data) -> std::span<T> {
        using ValueType = typename std::decay_t<decltype(data)>::value_type;

        if constexpr (ProcessableData<ValueType> && !std::is_same_v<ValueType, T>) {
            auto source_span = std::span<ValueType>(data.data(), data.size());
            auto converted_span = convert_data<ValueType, T>(source_span, strategy);

            std::vector<T> new_data(converted_span.begin(), converted_span.end());
            variant = DataVariant { std::move(new_data) };

            auto& new_vec = std::get<std::vector<T>>(variant);
            return std::span<T>(new_vec.data(), new_vec.size());
        }
        return {};
    },
        variant);
}

/**
 * @brief Get const span from DataVariant without conversion (zero-copy for matching types)
 * @tparam T Data type (must match DataVariant contents)
 * @param variant DataVariant to extract from
 * @return Const span of type T
 * @throws std::runtime_error if type doesn't match
 */
template <ProcessableData T>
std::span<const T> convert_variant(const DataVariant& variant)
{
    return std::visit([](const auto& vec) -> std::span<const T> {
        using VecType = typename std::decay_t<decltype(vec)>::value_type;
        if constexpr (std::is_same_v<VecType, T>) {
            return std::span<const T>(vec.data(), vec.size());
        } else {
            throw std::runtime_error("Type mismatch - conversion needed");
        }
    },
        variant);
}

/**
 * @brief Concept-based data extraction with type conversion
 * @tparam T Target type (must satisfy ProcessableData)
 * @param variant Source DataVariant (may be modified for conversion)
 * @return Span of converted data
 *
 * Performance advantage: Eliminates runtime type dispatch for supported types
 * Uses constexpr branching for compile-time optimization
 */
template <ProcessableData From, ProcessableData To>
std::span<To> extract_data(std::span<const From> source,
    std::vector<To>& destination,
    Utils::ComplexConversionStrategy strategy = Utils::ComplexConversionStrategy::MAGNITUDE)
{
    size_t required_elements = (source.size() * sizeof(From) + sizeof(To) - 1) / sizeof(To);
    destination.resize(required_elements);

    std::memcpy(destination.data(), source.data(), source.size() * sizeof(From));

    auto source_span = std::span<From>(reinterpret_cast<From*>(destination.data()), source.size());
    return convert_data<From, To>(source_span, strategy);
}

/**
 * @brief Get typed span from DataVariant using concepts
 * @tparam T Data type (must satisfy ProcessableData)
 * @param variant DataVariant to extract from
 * @return Span of type T, or empty span if type doesn't match
 */
template <ProcessableData T>
std::span<T> extract_from_variant(const DataVariant& variant,
    std::vector<T>& user_storage,
    Utils::ComplexConversionStrategy strategy = Utils::ComplexConversionStrategy::MAGNITUDE) noexcept
{
    if (std::holds_alternative<std::vector<T>>(variant)) {
        user_storage = std::get<std::vector<T>>(variant);
        return std::span<T>(user_storage.data(), user_storage.size());
    }

    return std::visit([&user_storage, strategy](auto& data) -> std::span<T> {
        using ValueType = typename std::decay_t<decltype(data)>::value_type;
        auto source_span = std::span<const ValueType>(data.data(), data.size());
        auto result = extract_data<ValueType, T>(source_span, user_storage, strategy);
        return std::span<T>(result.data(), result.size());
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
 * @brief Safely copy data from a DataVariant to a span of doubles.
 * @param input Source DataVariant.
 * @param output Destination span of doubles.
 * @throws std::runtime_error if complex types are involved.
 */
inline std::span<const double> safe_copy_data_variant_to_span(const DataVariant& input, std::vector<double>& output)
{
    return extract_from_variant(input, output);
}

/**
 * @brief Safely copy data from a DataVariant to another DataVariant of a specific type.
 * @tparam T Data type.
 * @param input Source DataVariant.
 * @param output Destination DataVariant.
 */
template <typename T>
void safe_copy_typed_variant(const DataVariant& input, DataVariant& output)
{
    std::vector<T> temp_storage;
    auto input_span = extract_from_variant<T>(input, temp_storage);
    auto output_span = get_typed_data<T>(output);
    std::copy_n(input_span.begin(), std::min(input_span.size(), output_span.size()),
        output_span.begin());
}

/**
 * @brief Convert variant to double span
 * @param data Source DataVariant (may be modified for conversion)
 * @param strategy Conversion strategy for complex numbers
 * @return Span of double data
 */
inline std::span<double> convert_variant_to_double(DataVariant& data,
    Utils::ComplexConversionStrategy strategy = Utils::ComplexConversionStrategy::MAGNITUDE)
{
    return convert_variant<double>(data, strategy);
}

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
            return safe_any_cast<T>(it->second);
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
 * @brief Detects data modality from dimension information
 * @param dimensions Vector of data dimensions with role information
 * @return Detected DataModality type
 *
 * Consolidates modality detection logic that was duplicated across analyzers.
 * Uses dimension roles and count to determine appropriate processing approach.
 */
DataModality detect_data_modality(const std::vector<DataDimension>& dimensions);

/**
 * @brief Detect data dimensions from a DataVariant
 * @param data DataVariant to analyze
 * @return Vector of DataDimension descriptors
 *
 * This function analyzes the structure of the provided DataVariant and extracts
 * dimension information, including size, stride, and semantic roles.
 */
std::vector<Kakshya::DataDimension> detect_data_dimensions(const DataVariant& data);
}
