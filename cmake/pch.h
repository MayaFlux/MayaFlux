#pragma once

#include "any"
#include "atomic"
#include "deque"
#include "exception"
#include "functional"
#include "iostream"
#include "list"
#include "map"
#include "memory"
#include "mutex"
#include "numbers"
#include "numeric"
#include "optional"
#include "ranges"
#include "shared_mutex"
#include "span"
#include "thread"
#include "unordered_map"
#include "unordered_set"
#include "utility"
#include "variant"

#include <glm/fwd.hpp>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>

#include "config.h"

// === Core MayaFlux Type System ===

namespace MayaFlux {

// === Universal Type Concepts ===

template <typename T>
concept IntegerData = std::is_integral_v<T>;

template <typename T>
concept DecimalData = std::is_floating_point_v<T>;

template <typename T>
concept ArithmeticData = IntegerData<T> || DecimalData<T>;

template <typename T>
concept ComplexData = requires {
    typename T::value_type;
    std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>;
};

template <typename T>
concept StringData = std::is_same_v<T, std::string> || std::is_same_v<T, const char*> || std::is_same_v<T, char*>;

// template <typename T>
// concept ProcessableData = ArithmeticData<T> || ComplexData<T>;

// === Universal Container Concepts ===

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

// === Range and Iterator Concepts ===

template <typename T>
concept RandomAccessRange = std::ranges::random_access_range<T>;

template <typename T>
concept SortableContainer = RandomAccessRange<T> && requires(T& container) {
    std::ranges::sort(container);
};

// === Conversion Safety Concepts ===

template <typename From, typename To>
concept SafeIntegerConversion = IntegerData<From> && IntegerData<To> && (sizeof(From) <= sizeof(To)) && (std::is_signed_v<From> == std::is_signed_v<To>);

template <typename From, typename To>
concept SafeDecimalConversion = DecimalData<From> && DecimalData<To> && (sizeof(From) <= sizeof(To));

template <typename From, typename To>
concept SafeArithmeticConversion = ArithmeticData<From> && ArithmeticData<To> && (SafeIntegerConversion<From, To> || SafeDecimalConversion<From, To> || (IntegerData<From> && DecimalData<To>));

// === Safe Any Cast System ===

template <typename T>
struct CastResult {
    std::optional<T> value;
    std::string error;
    bool precision_loss = false;

    explicit operator bool() const { return value.has_value(); }
    T value_or(const T& default_val) const { return value.value_or(default_val); }

    // Implicit conversion to std::optional for compatibility
    operator std::optional<T>() const { return value; }

    // Implicit conversion to T (throws if no value) - exclude bool to avoid conflict
    // operator std::enable_if_t<!std::is_same_v<U, bool>, U>() const
    template <typename U = T>
    operator U() const
        requires(!std::is_same_v<U, bool>)
    {
        if (!value.has_value()) {
            throw std::runtime_error("Safe cast failed: " + error);
        }
        return *value;
    }
};

template <typename To, typename From>
CastResult<To> try_convert(const From& value)
{
    CastResult<To> result;

    if constexpr (!ArithmeticData<To> && !ComplexData<To> && !StringData<To>) {
        result.error = "Unsupported target type";
        return result;
    }

    if constexpr (std::is_same_v<From, To>) {
        result.value = value;
    } else if constexpr (SafeArithmeticConversion<From, To>) {
        result.value = static_cast<To>(value);
        result.precision_loss = (sizeof(From) > sizeof(To));
    } else if constexpr (ArithmeticData<From> && ArithmeticData<To>) {
        if constexpr (DecimalData<From> && IntegerData<To>) {
            if (std::floor(value) != value)
                result.precision_loss = true;
        }
        result.value = static_cast<To>(value);
    } else if constexpr (ComplexData<From> && ArithmeticData<To>) {
        result.value = static_cast<To>(std::abs(value));
        result.precision_loss = (std::imag(value) != 0);
    } else {
        result.error = "No conversion available";
    }

    return result;
}

template <typename T>
    requires(ArithmeticData<T> || ComplexData<T> || StringData<T>)
CastResult<T> safe_any_cast(const std::any& any_val)
{
    CastResult<T> result;

    if (!any_val.has_value()) {
        result.error = "Empty any";
        return result;
    }

    if (any_val.type() == typeid(T)) {
        try {
            result.value = std::any_cast<T>(any_val);
        } catch (const std::bad_any_cast& e) {
            result.error = e.what();
        }
        return result;
    }

    if constexpr (!ArithmeticData<T> && !ComplexData<T> && !StringData<T>) {
        result.error = "Unsupported target type for conversion";
        return result;
    }

    if constexpr (ArithmeticData<T>) {
        if (any_val.type() == typeid(int)) {
            return try_convert<T>(std::any_cast<int>(any_val));
        } else if (any_val.type() == typeid(unsigned)) {
            return try_convert<T>(std::any_cast<unsigned>(any_val));
        } else if (any_val.type() == typeid(long)) {
            return try_convert<T>(std::any_cast<long>(any_val));
        } else if (any_val.type() == typeid(size_t)) {
            return try_convert<T>(std::any_cast<size_t>(any_val));
        } else if (any_val.type() == typeid(uint32_t)) {
            return try_convert<T>(std::any_cast<uint32_t>(any_val));
        } else if (any_val.type() == typeid(float)) {
            return try_convert<T>(std::any_cast<float>(any_val));
        } else if (any_val.type() == typeid(double)) {
            return try_convert<T>(std::any_cast<double>(any_val));
        }
    }

    if constexpr (StringData<T>) {
        if (any_val.type() == typeid(std::string)) {
            if constexpr (std::is_same_v<T, std::string>) {
                result.value = std::any_cast<std::string>(any_val);
            }
        } else if (any_val.type() == typeid(const char*)) {
            if constexpr (std::is_same_v<T, std::string>) {
                result.value = std::string(std::any_cast<const char*>(any_val));
            } else if constexpr (std::is_same_v<T, const char*>) {
                result.value = std::any_cast<const char*>(any_val);
            }
        }
    }

    if constexpr (ComplexData<T>) {
        if (any_val.type() == typeid(std::complex<float>)) {
            return try_convert<T>(std::any_cast<std::complex<float>>(any_val));
        } else if (any_val.type() == typeid(std::complex<double>)) {
            return try_convert<T>(std::any_cast<std::complex<double>>(any_val));
        } else if (any_val.type() == typeid(float)) {
            return try_convert<T>(std::any_cast<float>(any_val));
        } else if (any_val.type() == typeid(double)) {
            return try_convert<T>(std::any_cast<double>(any_val));
        }
    }

    result.error = "No safe conversion found";
    return result;
}

template <typename T>
    requires(!ArithmeticData<T> && !ComplexData<T> && !StringData<T>)
CastResult<T> safe_any_cast(const std::any& any_val)
{
    CastResult<T> result;

    if (!any_val.has_value()) {
        result.error = "Empty any";
        return result;
    }

    if (any_val.type() == typeid(T)) {
        try {
            result.value = std::any_cast<T>(any_val);
        } catch (const std::bad_any_cast& e) {
            result.error = "Failed to cast to " + std::string(typeid(T).name()) + ": " + e.what();
        }
    } else {
        result.error = "Type mismatch: expected " + std::string(typeid(T).name()) + ", got " + std::string(any_val.type().name());
    }

    return result;
}

template <typename T>
T safe_any_cast_or_throw(const std::any& any_val)
{
    auto result = safe_any_cast<T>(any_val);
    if (!result)
        throw std::runtime_error("Safe cast failed: " + result.error);
    return *result.value;
}

template <typename T>
T safe_any_cast_or_default(const std::any& any_val, const T& default_value)
{
    return safe_any_cast<T>(any_val).value_or(default_value);
}

// === Universal Type Handler System ===

template <typename T>
struct TypeHandler {
    static constexpr bool is_supported = false;
    static constexpr const char* name = "unsupported";
};

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

template <>
struct TypeHandler<std::complex<double>> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "complex_double";
    using processing_type = std::complex<double>;
};

template <>
struct TypeHandler<int> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "int";
    using processing_type = int;
};

template <>
struct TypeHandler<std::uint32_t> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "uint32";
    using processing_type = std::uint32_t;
};

template <>
struct TypeHandler<std::size_t> {
    static constexpr bool is_supported = true;
    static constexpr const char* name = "size_t";
    using processing_type = std::size_t;
};

template <typename T>
concept SupportedDataType = TypeHandler<T>::is_supported;

template <typename T>
inline constexpr bool always_false_v = false;

template <typename T>
concept GlmVec2Type = std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::dvec2>;

template <typename T>
concept GlmVec3Type = std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>;

template <typename T>
concept GlmVec4Type = std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::dvec4>;

template <typename T>
concept GlmVectorType = GlmVec2Type<T> || GlmVec3Type<T> || GlmVec4Type<T>;

template <typename T>
concept GlmMatrixType = std::is_same_v<T, glm::mat2> || std::is_same_v<T, glm::mat3> || std::is_same_v<T, glm::mat4> || std::is_same_v<T, glm::dmat2> || std::is_same_v<T, glm::dmat3> || std::is_same_v<T, glm::dmat4>;

template <typename T>
concept GlmType = GlmVectorType<T> || GlmMatrixType<T>;

template <typename T>
constexpr size_t glm_component_count()
{
    if constexpr (GlmVec2Type<T>) {
        return 2;
    } else if constexpr (GlmVec3Type<T>) {
        return 3;
    } else if constexpr (GlmVec4Type<T>) {
        return 4;
    } else if constexpr (std::is_same_v<T, glm::mat2> || std::is_same_v<T, glm::dmat2>) {
        return 4;
    } else if constexpr (std::is_same_v<T, glm::mat3> || std::is_same_v<T, glm::dmat3>) {
        return 9;
    } else if constexpr (std::is_same_v<T, glm::mat4> || std::is_same_v<T, glm::dmat4>) {
        return 16;
    } else {
        return 0;
    }
}

template <typename T>
using glm_component_type = typename T::value_type;

template <typename T>
concept GlmVectorData = GlmVectorType<T>;

template <typename T>
concept GlmMatrixData = GlmMatrixType<T>;

template <typename T>
concept GlmData = GlmVectorData<T> || GlmMatrixData<T>;

template <typename T>
concept ProcessableData = ArithmeticData<T> || ComplexData<T> || GlmData<T>;

template <typename T>
concept ComponentProcessableData = ArithmeticData<T> || ComplexData<T>;

template <typename From, typename To>
struct is_convertible_data : std::false_type { };

template <typename From, typename To>
    requires ArithmeticData<From> && ArithmeticData<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires ComplexData<From> && ArithmeticData<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires ArithmeticData<From> && ComplexData<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires ComplexData<From> && ComplexData<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires GlmType<From> && GlmType<To> && (glm_component_count<From>() == glm_component_count<To>())
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires GlmType<From> && ArithmeticData<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
    requires ArithmeticData<From> && GlmType<To>
struct is_convertible_data<From, To> : std::true_type { };

template <typename From, typename To>
inline constexpr bool is_convertible_data_v = is_convertible_data<From, To>::value;

} // namespace MayaFlux
