#pragma once

namespace MayaFlux::Reflect {

/**
 * @struct is_optional
 * @brief True for std::optional<T>; inner names T.
 */
template <typename T>
struct is_optional : std::false_type { };
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using inner = T;
};
template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

/**
 * @struct is_vector
 * @brief True for std::vector<T>; element names T.
 */
template <typename T>
struct is_vector : std::false_type { };
template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
    using element = T;
};
template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

/**
 * @struct is_string_map
 * @brief True for a std::map or std::unordered_map keyed by std::string; element names the mapped type.
 */
template <typename T>
struct is_string_map : std::false_type { };
template <typename V>
struct is_string_map<std::unordered_map<std::string, V>> : std::true_type {
    using element = V;
};
template <typename V>
struct is_string_map<std::map<std::string, V>> : std::true_type {
    using element = V;
};
template <typename T>
inline constexpr bool is_string_map_v = is_string_map<T>::value;

}
