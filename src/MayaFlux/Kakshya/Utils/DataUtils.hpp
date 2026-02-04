#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <typeindex>

namespace MayaFlux::Kakshya {

/**
 * @enum ComplexConversionStrategy
 * @brief Strategy for converting complex numbers to real values
 */
enum class ComplexConversionStrategy : uint8_t {
    MAGNITUDE, ///< |z| = sqrt(real² + imag²)
    REAL_PART, ///< z.real()
    IMAG_PART, ///< z.imag()
    SQUARED_MAGNITUDE ///< |z|² = real² + imag²
};

template <typename From, typename To, typename Enable = void>
struct DataConverter {
    static std::span<To> convert(std::span<From> /*source*/,
        std::vector<To>& /*storage*/,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        static_assert(always_false_v<From>, "No conversion available for these types");
        return {};
    }
};

// === Specialization: Identity Conversion ===

template <typename T>
struct DataConverter<T, T> {
    static std::span<T> convert(std::span<T> source,
        std::vector<T>&,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        return source;
    }
};

// === Specialization: Arithmetic to Arithmetic (excluding GLM) ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        ArithmeticData<From> && ArithmeticData<To> && !std::is_same_v<From, To> && !GlmType<From> && !GlmType<To>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        storage.resize(source.size());
        std::transform(
            source.begin(), source.end(), storage.begin(),
            [](From val) { return static_cast<To>(val); });

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: Complex to Arithmetic ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        ComplexData<From> && ArithmeticData<To> && !GlmType<From> && !GlmType<To>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy strategy)
    {
        storage.resize(source.size());

        for (size_t i = 0; i < source.size(); ++i) {
            switch (strategy) {
            case ComplexConversionStrategy::MAGNITUDE:
                storage[i] = static_cast<To>(std::abs(source[i]));
                break;
            case ComplexConversionStrategy::REAL_PART:
                storage[i] = static_cast<To>(source[i].real());
                break;
            case ComplexConversionStrategy::IMAG_PART:
                storage[i] = static_cast<To>(source[i].imag());
                break;
            case ComplexConversionStrategy::SQUARED_MAGNITUDE:
                storage[i] = static_cast<To>(std::norm(source[i]));
                break;
            }
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: Arithmetic to Complex ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        ArithmeticData<From> && ComplexData<To> && !GlmType<From> && !GlmType<To>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        using ComplexValueType = typename To::value_type;
        storage.resize(source.size());

        for (size_t i = 0; i < source.size(); ++i) {
            storage[i] = To(static_cast<ComplexValueType>(source[i]), ComplexValueType { 0 });
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: GLM to Arithmetic (Flattening) ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        GlmType<From> && ArithmeticData<To> && !GlmType<To>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        constexpr size_t components = glm_component_count<From>();
        using ComponentType = glm_component_type<From>;

        storage.resize(source.size() * components);

        size_t out_idx = 0;
        for (const auto& elem : source) {
            const ComponentType* ptr = glm::value_ptr(elem);
            for (size_t c = 0; c < components; ++c) {
                storage[out_idx++] = static_cast<To>(ptr[c]);
            }
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: Arithmetic to GLM (Structuring) ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        ArithmeticData<From> && GlmType<To> && !GlmType<From>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        constexpr size_t components = glm_component_count<To>();
        using ComponentType = glm_component_type<To>;

        if (source.size() % components != 0) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Source size ({}) must be multiple of GLM component count ({})",
                source.size(),
                components);
        }

        size_t element_count = source.size() / components;
        storage.resize(element_count);

        for (size_t i = 0; i < element_count; ++i) {
            ComponentType temp[components];
            for (size_t c = 0; c < components; ++c) {
                temp[c] = static_cast<ComponentType>(source[i * components + c]);
            }

            if constexpr (GlmVec2Type<To>) {
                storage[i] = To(temp[0], temp[1]);
            } else if constexpr (GlmVec3Type<To>) {
                storage[i] = To(temp[0], temp[1], temp[2]);
            } else if constexpr (GlmVec4Type<To>) {
                storage[i] = To(temp[0], temp[1], temp[2], temp[3]);
            } else if constexpr (GlmMatrixType<To>) {
                storage[i] = glm::make_mat4(temp);
            }
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: GLM to GLM (same component count) ===

template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        GlmType<From> && GlmType<To> && !std::is_same_v<From, To> && (glm_component_count<From>() == glm_component_count<To>())>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        using FromComponent = glm_component_type<From>;
        using ToComponent = glm_component_type<To>;
        constexpr size_t components = glm_component_count<From>();

        storage.resize(source.size());

        for (size_t i = 0; i < source.size(); ++i) {
            const FromComponent* src_ptr = glm::value_ptr(source[i]);
            ToComponent temp[components];

            for (size_t c = 0; c < components; ++c) {
                temp[c] = static_cast<ToComponent>(src_ptr[c]);
            }

            if constexpr (GlmVec2Type<To>) {
                storage[i] = To(temp[0], temp[1]);
            } else if constexpr (GlmVec3Type<To>) {
                storage[i] = To(temp[0], temp[1], temp[2]);
            } else if constexpr (GlmVec4Type<To>) {
                storage[i] = To(temp[0], temp[1], temp[2], temp[3]);
            } else if constexpr (GlmMatrixType<To>) {
                storage[i] = glm::make_mat4(temp);
            }
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

// === Specialization: Complex -> Complex ===
template <typename From, typename To>
struct DataConverter<
    From, To,
    std::enable_if_t<
        ComplexData<From> && ComplexData<To> && !GlmType<From> && !GlmType<To> && !std::is_same_v<From, To>>> {
    static std::span<To> convert(
        std::span<From> source,
        std::vector<To>& storage,
        ComplexConversionStrategy = ComplexConversionStrategy::MAGNITUDE)
    {
        using FromValue = typename From::value_type;
        using ToValue = typename To::value_type;

        storage.resize(source.size());
        for (size_t i = 0; i < source.size(); ++i) {
            const auto r = static_cast<FromValue>(source[i].real());
            const auto im = static_cast<FromValue>(source[i].imag());
            storage[i] = To(static_cast<ToValue>(r), static_cast<ToValue>(im));
        }

        return std::span<To>(storage.data(), storage.size());
    }
};

/**
 * @brief Calculate the total number of elements in an N-dimensional container.
 * @param dimensions Dimension descriptors.
 * @return Product of all dimension sizes.
 */
uint64_t calculate_total_elements(const std::vector<DataDimension>& dimensions);

/**
 * @brief Calculate the frame size (number of elements per frame) for a set of dimensions.
 * @param dimensions Dimension descriptors.
 * @return Frame size (product of all but the first dimension).
 */
uint64_t calculate_frame_size(const std::vector<DataDimension>& dimensions);

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
constexpr std::span<T> extract_frame(std::span<T> data, uint64_t frame_index, uint64_t frame_size) noexcept
{
    uint64_t start = frame_index * frame_size;
    uint64_t end = std::min(static_cast<uint64_t>(start + frame_size),
        static_cast<uint64_t>(data.size()));

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
    uint64_t frame_index,
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

/**
 * @brief Convert a span of one data type to another (with type conversion).
 * @tparam From Source type.
 * @tparam To Destination type.
 * @param source Source data span.
 * @param dest Destination data span.
 * @return Span of converted data.
 */
template <typename From, typename To>
std::span<To> convert_data(std::span<From> source,
    std::vector<To>& storage,
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    return DataConverter<From, To>::convert(source, storage, strategy);
}

/**
 * @brief Legacy interface - redirects to convert_data
 */
template <typename From, typename To>
    requires(ComplexData<From> && ArithmeticData<To>)
void convert_complex(std::span<From> source,
    std::span<To> destination,
    ComplexConversionStrategy strategy)
{
    std::vector<To> temp_storage;
    auto result = convert_data(source, temp_storage, strategy);
    std::copy_n(result.begin(), std::min(result.size(), destination.size()), destination.begin());
}

/**
 * @brief Get const span from DataVariant without conversion (zero-copy for matching types)
 * @tparam T Data type (must match DataVariant contents)
 * @param variant DataVariant to extract from
 * @return Const span of type T
 * @throws std::runtime_error if type doesn't match
 */
template <ProcessableData T>
std::span<T> convert_variant(DataVariant& variant,
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    if (std::holds_alternative<std::vector<T>>(variant)) {
        auto& vec = std::get<std::vector<T>>(variant);
        return std::span<T>(vec.data(), vec.size());
    }

    return std::visit([&variant, strategy](auto& data) -> std::span<T> {
        using ValueType = typename std::decay_t<decltype(data)>::value_type;

        if constexpr (is_convertible_data_v<ValueType, T>) {
            std::vector<T> new_storage;
            auto source_span = std::span<ValueType>(data.data(), data.size());
            auto result = convert_data(source_span, new_storage, strategy);

            variant = std::move(new_storage);
            auto& new_vec = std::get<std::vector<T>>(variant);
            return std::span<T>(new_vec.data(), new_vec.size());
        } else {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "No conversion available from {} to {}",
                typeid(ValueType).name(),
                typeid(T).name());
        }
    },
        variant);
}

template <ProcessableData T>
std::span<T> convert_variant(const DataVariant& variant,
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    return convert_variant<T>(const_cast<DataVariant&>(variant), strategy);
}

template <ProcessableData T>
std::vector<std::span<T>> convert_variants(
    const std::vector<DataVariant>& variants,
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    std::vector<std::span<T>> result;
    result.reserve(variants.size());

    for (const auto& i : variants) {
        result.push_back(convert_variant<T>(const_cast<DataVariant&>(i), strategy));
    }
    return result;
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
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    const size_t total_bytes = source.size() * sizeof(From);
    const size_t required_elements = (total_bytes + sizeof(To) - 1) / sizeof(To);
    destination.resize(required_elements);

    if constexpr (std::is_same_v<From, To>) {
        destination.resize(source.size());
        std::memcpy(destination.data(), source.data(), total_bytes);
        return std::span<To>(destination.data(), source.size());
    } else if constexpr (std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To> && (sizeof(From) == sizeof(To))) {
        // Bitwise reinterpretation allowed (e.g. int32_t <-> float)
        std::memcpy(destination.data(), source.data(), total_bytes);
        return std::span<To>(destination.data(), source.size());
    } else {
        // General case — use proper conversion
        std::vector<From> temp(source.begin(), source.end());
        auto converted = convert_data<From, To>(std::span<const From>(temp), strategy);
        destination.assign(converted.begin(), converted.end());
        return std::span<To>(destination.data(), destination.size());
    }
}

/**
 * @brief Get typed span from DataVariant using concepts
 * @tparam T Data type (must satisfy ProcessableData)
 * @param variant DataVariant to extract from
 * @return Span of type T, or empty span if type doesn't match
 */
template <ProcessableData T>
std::span<T> extract_from_variant(const DataVariant& variant,
    std::vector<T>& storage,
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
{
    return std::visit([&storage, strategy](const auto& data) -> std::span<T> {
        using ValueType = typename std::decay_t<decltype(data)>::value_type;

        if constexpr (std::is_same_v<ValueType, T>) {
            storage = data;
            return std::span<T>(storage.data(), storage.size());
        } else if constexpr (is_convertible_data_v<ValueType, T>) {
            auto source_span = std::span<const ValueType>(data.data(), data.size());
            std::vector<ValueType> temp_source(source_span.begin(), source_span.end());
            auto temp_span = std::span<ValueType>(temp_source.data(), temp_source.size());
            return convert_data(temp_span, storage, strategy);
        } else {
            // throw std::runtime_error("Cannot convert from " + std::string(typeid(ValueType).name()) + " to " + std::string(typeid(T).name()));
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "No conversion available from {} to {}",
                typeid(ValueType).name(),
                typeid(T).name());
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
std::optional<T> extract_from_variant_at(const DataVariant& variant, uint64_t pos)
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
    ComplexConversionStrategy strategy = ComplexConversionStrategy::MAGNITUDE)
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

/**
 * @brief Detect data dimensions from a vector of DataVariants
 * @param variants Vector of DataVariants to analyze
 * @return Vector of DataDimension descriptors
 *
 * This function analyzes the structure of the provided vector of DataVariants and extracts
 * dimension information, including size, stride, and semantic roles.
 *
 * WARNING: This method makes naive assumptions about the data structure and may lead to incorrect interpretations.
 * It is recommended to use more specific methods when dealing with known containers, regions, or segments.
 * Use this function only when absolutely necessary and be aware of potential computational errors.
 */
std::vector<Kakshya::DataDimension> detect_data_dimensions(const std::vector<Kakshya::DataVariant>& variants);
}
