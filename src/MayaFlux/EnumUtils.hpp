#pragma once

#include "config.h"

#include "magic_enum/magic_enum.hpp"

namespace MayaFlux::Utils {

/**
 * @brief Convert string to lowercase
 */
inline std::string to_lowercase(std::string_view str)
{
    std::string result;
    result.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(result),
        [](char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Convert string to uppercase
 */
inline std::string to_uppercase(std::string_view str)
{
    std::string result;
    result.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(result),
        [](char c) { return std::toupper(c); });
    return result;
}

/**
 * @brief Universal enum to lowercase string converter using magic_enum
 * @tparam EnumType Any enum type
 * @param value Enum value to convert
 * @return Lowercase string representation of the enum
 */
template <typename EnumType>
std::string enum_to_lowercase_string(EnumType value) noexcept
{
    auto name = magic_enum::enum_name(value);
    return to_lowercase(name);
}

/**
 * @brief Universal enum to string converter using magic_enum (original case)
 * @tparam EnumType Any enum type
 * @param value Enum value to convert
 * @return String representation of the enum
 */
template <typename EnumType>
constexpr std::string_view enum_to_string(EnumType value) noexcept
{
    return magic_enum::enum_name(value);
}

/**
 * @brief Universal case-insensitive string to enum converter using magic_enum
 * @tparam EnumType Any enum type
 * @param str String to convert (case-insensitive)
 * @return Optional enum value if valid, nullopt otherwise
 */
template <typename EnumType>
std::optional<EnumType> string_to_enum_case_insensitive(std::string_view str) noexcept
{
    // Try direct match first (most efficient)
    auto direct_result = magic_enum::enum_cast<EnumType>(str);
    if (direct_result.has_value()) {
        return direct_result;
    }

    // Convert input to uppercase for comparison with enum names
    std::string upper_str = to_uppercase(str);
    auto upper_result = magic_enum::enum_cast<EnumType>(upper_str);
    if (upper_result.has_value()) {
        return upper_result;
    }

    return std::nullopt;
}

/**
 * @brief Universal string to enum converter using magic_enum (exact case match)
 * @tparam EnumType Any enum type
 * @param str String to convert
 * @return Optional enum value if valid, nullopt otherwise
 */
template <typename EnumType>
constexpr std::optional<EnumType> string_to_enum(std::string_view str) noexcept
{
    return magic_enum::enum_cast<EnumType>(str);
}

/**
 * @brief Get all enum values as lowercase strings
 * @tparam EnumType Any enum type
 * @return Vector of all enum value names in lowercase
 */
template <typename EnumType>
std::vector<std::string> get_enum_names_lowercase() noexcept
{
    auto names = magic_enum::enum_names<EnumType>();
    std::vector<std::string> lowercase_names;
    lowercase_names.reserve(names.size());

    for (const auto& name : names) {
        lowercase_names.push_back(to_lowercase(name));
    }

    return lowercase_names;
}

/**
 * @brief Get all enum values as strings (original case)
 * @tparam EnumType Any enum type
 * @return Vector of all enum value names
 */
template <typename EnumType>
constexpr auto get_enum_names() noexcept
{
    return magic_enum::enum_names<EnumType>();
}

/**
 * @brief Get all enum values
 * @tparam EnumType Any enum type
 * @return Array of all enum values
 */
template <typename EnumType>
constexpr auto get_enum_values() noexcept
{
    return magic_enum::enum_values<EnumType>();
}

/**
 * @brief Validate if string is a valid enum value (case-insensitive)
 * @tparam EnumType Any enum type
 * @param str String to validate
 * @return True if string represents a valid enum value
 */
template <typename EnumType>
bool is_valid_enum_string_case_insensitive(std::string_view str) noexcept
{
    return string_to_enum_case_insensitive<EnumType>(str).has_value();
}

/**
 * @brief Get enum count
 * @tparam EnumType Any enum type
 * @return Number of enum values
 */
template <typename EnumType>
constexpr size_t enum_count() noexcept
{
    return magic_enum::enum_count<EnumType>();
}

/**
 * @brief Convert string to enum with exception on failure (case-insensitive)
 * @tparam EnumType Any enum type
 * @param str String to convert
 * @param context Optional context for error message
 * @return Enum value
 * @throws std::invalid_argument if string is not valid enum
 */
template <typename EnumType>
EnumType string_to_enum_or_throw_case_insensitive(std::string_view str,
    std::string_view context = "")
{
    auto result = string_to_enum_case_insensitive<EnumType>(str);
    if (!result.has_value()) {
        std::string error_msg = "Invalid enum value: '" + std::string(str) + "'";
        if (!context.empty()) {
            error_msg += " for " + std::string(context);
        }

        error_msg += ". Valid values are: ";
        auto names = get_enum_names_lowercase<EnumType>();
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0)
                error_msg += ", ";
            error_msg += names[i];
        }

        throw std::invalid_argument(error_msg);
    }
    return *result;
}
}
