#pragma once

#include "Region.hpp"

namespace MayaFlux::Kakshya {

// Primary data type concepts
template <typename T>
concept ArithmeticData = std::is_arithmetic_v<T>;

template <typename T>
concept ComplexData = requires {
    typename T::value_type;
    std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>;
};

template <typename T>
concept ProcessableData = ArithmeticData<T> || ComplexData<T>;

// Container concepts
template <typename T>
concept ContiguousContainer = requires(T t) {
    { t.data() } -> std::convertible_to<typename T::value_type*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
};

template <typename T>
concept SpanLike = requires(T t) {
    { t.data() } -> std::convertible_to<const typename T::element_type*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

// Region access concepts
template <typename T>
concept RegionExtractable = requires(T t, const Region& region) {
    { extract_region_data(t, region) } -> ContiguousContainer;
};

// Dimension concepts
template <typename T>
concept DimensionalData = requires(T t) {
    { t.get_dimensions() } -> std::convertible_to<std::vector<DataDimension>>;
    { t.get_processed_data() } -> std::convertible_to<const DataVariant&>;
};

template <typename T>
struct TypeHandler {
    static constexpr bool is_supported = false;
    static constexpr const char* name = "unsupported";
};

// Specializations for supported types
template <>
struct TypeHandler<float> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "float";
    using processing_type = float;
};

template <>
struct TypeHandler<double> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "double";
    using processing_type = double;
};

template <>
struct TypeHandler<std::complex<float>> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "complex_float";
    using processing_type = std::complex<float>;
};

// Concept-based type checking instead of std::type_index
template <typename T>
concept SupportedDataType = TypeHandler<T>::is_supported;

}
