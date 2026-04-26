#pragma once

namespace MayaFlux::IO {

// ---------------------------------------------------------------------------
// Property descriptors
// ---------------------------------------------------------------------------

/**
 * @struct Property
 * @brief Binds a string key to a required member pointer.
 */
template <typename Class, typename T>
struct Property {
    std::string_view key;
    T Class::* member;
    static constexpr bool is_optional = false;
};

/**
 * @struct OptionalProperty
 * @brief Binds a string key to a std::optional<T> member pointer.
 *        Omitted from output when nullopt; missing key on decode leaves nullopt.
 */
template <typename Class, typename T>
struct OptionalProperty {
    std::string_view key;
    std::optional<T> Class::* member;
    static constexpr bool is_optional = true;
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------

template <typename Class, typename T>
constexpr auto member(std::string_view key, T Class::* ptr)
{
    return Property<Class, T> { key, ptr };
}

template <typename Class, typename T>
constexpr auto opt_member(std::string_view key, std::optional<T> Class::* ptr)
{
    return OptionalProperty<Class, T> { key, ptr };
}

// ---------------------------------------------------------------------------
// Concept: a type that exposes a constexpr describe() returning a tuple of
// Property / OptionalProperty descriptors.
// ---------------------------------------------------------------------------

template <typename T>
concept Reflectable = requires {
    { T::describe() } -> std::same_as<decltype(T::describe())>;
    requires std::tuple_size_v<decltype(T::describe())> >= 1;
};

// ---------------------------------------------------------------------------
// Type-category detection helpers (used by the serializer engine)
// ---------------------------------------------------------------------------

template <typename T>
struct is_optional : std::false_type { };
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using inner = T;
};
template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_vector : std::false_type { };
template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
    using element = T;
};
template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

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

template <typename T>
concept GlmSerializable = GlmType<T>;

} // namespace MayaFlux::IO
