#pragma once

#include "EnumReflect.hpp"

#include <charconv>

namespace MayaFlux::Reflect {

enum class TextError { Invalid, Cardinality, Unsupported };

/**
 * @brief Parse whitespace separated finite numbers.
 * @return The values, or std::nullopt for any malformed or non-finite token.
 */
template <typename T>
    requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
[[nodiscard]] std::optional<std::vector<T>> parse_numbers(std::string_view source)
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

/**
 * @brief Parse exactly N whitespace separated finite numbers.
 * @param error Receives Invalid for a malformed token and Cardinality for a wrong count.
 */
template <typename T, size_t N>
[[nodiscard]] std::optional<std::array<T, N>> parse_tuple(std::string_view source,
    TextError& error)
{
    const auto values = parse_numbers<T>(source);
    if (!values) {
        error = TextError::Invalid;
        return std::nullopt;
    }
    if (values->size() != N) {
        error = TextError::Cardinality;
        return std::nullopt;
    }
    std::array<T, N> result;
    std::ranges::copy(*values, result.begin());
    return result;
}

/**
 * @brief Conversion of authored text to one value type.
 *
 * Any text leaf can use it: a document attribute, a delimited cell, a
 * command argument. Specialize for a type that needs another spelling.
 * A specialization provides extent (expected component count, 0 when not
 * numeric) and decode(source, value, error).
 */
template <typename T>
struct TextCodec { };

template <typename T>
    requires((std::is_integral_v<T> || std::is_floating_point_v<T>)
        && !std::same_as<T, bool>)
struct TextCodec<T> {
    static constexpr size_t extent = 1;

    static bool decode(std::string_view source, T& value, TextError& error)
    {
        const auto decoded = parse_tuple<T, 1>(source, error);
        if (!decoded)
            return false;
        value = (*decoded)[0];
        return true;
    }
};

template <>
struct TextCodec<bool> {
    static constexpr size_t extent = 1;

    static bool decode(std::string_view source, bool& value, TextError& error)
    {
        if (source == "true" || source == "1") {
            value = true;
            return true;
        }
        if (source == "false" || source == "0") {
            value = false;
            return true;
        }
        error = TextError::Invalid;
        return false;
    }
};

template <>
struct TextCodec<std::string> {
    static constexpr size_t extent = 0;

    static bool decode(std::string_view source, std::string& value, TextError&)
    {
        value = source;
        return true;
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct TextCodec<T> {
    static constexpr size_t extent = 0;

    static bool decode(std::string_view source, T& value, TextError& error)
    {
        const auto decoded = string_to_enum<T>(source);
        if (!decoded) {
            error = TextError::Unsupported;
            return false;
        }
        value = *decoded;
        return true;
    }
};

template <typename T>
concept TextDecodable = requires(std::string_view source, T& value, TextError& error) {
    { TextCodec<T>::decode(source, value, error) } -> std::same_as<bool>;
};

template <TextDecodable T>
struct TextCodec<std::optional<T>> {
    static constexpr size_t extent = TextCodec<T>::extent;

    static bool decode(std::string_view source, std::optional<T>& value,
        TextError& error)
    {
        T decoded = value.value_or(T {});
        if (!TextCodec<T>::decode(source, decoded, error))
            return false;
        value = std::move(decoded);
        return true;
    }
};

}
