#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

#include <charconv>

namespace MayaFlux::IO {

enum class XmlTextError { Invalid, Cardinality, Unsupported };

template <typename T>
    requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
[[nodiscard]] std::optional<std::vector<T>> parse_xml_numbers(std::string_view source)
{
    const auto whitespace = [](char value) {
        return value == ' ' || value == '\t' || value == '\n' || value == '\r';
    };

    std::vector<T> values;
    const char* current = source.data();
    const char* end = current + source.size();
    while (current != end) {
        while (current != end && whitespace(*current))
            ++current;
        if (current == end)
            break;

        const bool positive_sign = *current == '+';
        if (positive_sign)
            ++current;

        T value {};
        const auto [next, status] = std::from_chars(current, end, value);
        if ((positive_sign && (current == end || *current == '+' || *current == '-'))
            || status != std::errc {} || next == current
            || (std::is_floating_point_v<T> && !std::isfinite(value))
            || (next != end && !whitespace(*next))) {
            return std::nullopt;
        }
        values.push_back(value);
        current = next;
    }
    return values;
}

template <typename T, size_t N>
[[nodiscard]] std::optional<std::array<T, N>> parse_xml_tuple(std::string_view source,
    XmlTextError& error)
{
    const auto values = parse_xml_numbers<T>(source);
    if (!values) {
        error = XmlTextError::Invalid;
        return std::nullopt;
    }
    if (values->size() != N) {
        error = XmlTextError::Cardinality;
        return std::nullopt;
    }
    std::array<T, N> result;
    std::ranges::copy(*values, result.begin());
    return result;
}

/** @brief Convert XML attribute text to one described member value. */
template <typename T>
struct XmlTextCodec { };

template <typename T>
    requires((std::is_integral_v<T> || std::is_floating_point_v<T>)
        && !std::same_as<T, bool>)
struct XmlTextCodec<T> {
    static constexpr size_t extent = 1;

    static bool decode(std::string_view source, T& value, XmlTextError& error)
    {
        const auto decoded = parse_xml_tuple<T, 1>(source, error);
        if (!decoded)
            return false;
        value = (*decoded)[0];
        return true;
    }
};

template <>
struct XmlTextCodec<bool> {
    static constexpr size_t extent = 1;

    static bool decode(std::string_view source, bool& value, XmlTextError& error)
    {
        if (source == "true" || source == "1") {
            value = true;
            return true;
        }
        if (source == "false" || source == "0") {
            value = false;
            return true;
        }
        error = XmlTextError::Invalid;
        return false;
    }
};

template <>
struct XmlTextCodec<std::string> {
    static constexpr size_t extent = 0;

    static bool decode(std::string_view source, std::string& value, XmlTextError&)
    {
        value = source;
        return true;
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct XmlTextCodec<T> {
    static constexpr size_t extent = 0;

    static bool decode(std::string_view source, T& value, XmlTextError& error)
    {
        const auto decoded = Reflect::string_to_enum<T>(source);
        if (!decoded) {
            error = XmlTextError::Unsupported;
            return false;
        }
        value = *decoded;
        return true;
    }
};

template <GlmVectorType T>
struct XmlTextCodec<T> {
    static constexpr size_t extent = glm_component_count<T>();

    static bool decode(std::string_view source, T& value, XmlTextError& error)
    {
        using Scalar = typename T::value_type;
        constexpr size_t count = glm_component_count<T>();
        const auto decoded = parse_xml_tuple<Scalar, count>(source, error);
        if (!decoded)
            return false;
        for (size_t i = 0; i < count; ++i)
            value[static_cast<glm::length_t>(i)] = (*decoded)[i];
        return true;
    }
};

template <typename T>
concept XmlTextDecodable = requires(std::string_view source, T& value,
    XmlTextError& error) {
    { XmlTextCodec<T>::decode(source, value, error) } -> std::same_as<bool>;
};

template <XmlTextDecodable T>
struct XmlTextCodec<std::optional<T>> {
    static constexpr size_t extent = XmlTextCodec<T>::extent;

    static bool decode(std::string_view source, std::optional<T>& value,
        XmlTextError& error)
    {
        T decoded = value.value_or(T {});
        if (!XmlTextCodec<T>::decode(source, decoded, error))
            return false;
        value = std::move(decoded);
        return true;
    }
};

}
