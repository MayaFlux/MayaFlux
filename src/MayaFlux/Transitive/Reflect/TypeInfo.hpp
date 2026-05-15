#pragma once

#if defined(MAYAFLUX_COMPILER_CLANG) || defined(MAYAFLUX_COMPILER_GCC)
#include <cxxabi.h>
#endif

namespace MayaFlux::Reflect {

namespace detail {

    [[nodiscard]] inline std::string_view strip_namespaces(std::string_view name) noexcept
    {
        auto pos = name.rfind(':');
        return (pos != std::string_view::npos) ? name.substr(pos + 1) : name;
    }

} // namespace detail

/**
 * @brief Returns the fully qualified compile-time type name of @p T.
 *
 * Zero allocation. Returns a string_view into static storage valid for the
 * process lifetime. Replaces the local live_type_name<T>() in LiveArena.hpp.
 *
 * @tparam T Any type.
 */
template <typename T>
[[nodiscard]] constexpr std::string_view type_name() noexcept
{
#if defined(MAYAFLUX_COMPILER_CLANG)
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr auto s = fn.find("T = ") + 4;
    constexpr auto e = fn.rfind(']');
    return fn.substr(s, e - s);
#elif defined(MAYAFLUX_COMPILER_GCC)
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr auto s = fn.find("T = ") + 4;
    constexpr auto e = fn.find(';', s);
    return (e != std::string_view::npos) ? fn.substr(s, e - s) : fn.substr(s);
#elif defined(MAYAFLUX_COMPILER_MSVC)
    constexpr std::string_view fn = __FUNCSIG__;
    constexpr auto s = fn.find("type_name<") + 10;
    constexpr auto e = fn.rfind(">(");
    return (e != std::string_view::npos && e > s) ? fn.substr(s, e - s) : fn.substr(s);
#else
    return "unknown";
#endif
}

/**
 * @brief Returns the unqualified compile-time type name of @p T.
 *
 * Strips all namespace prefixes. Equivalent to the old live_type_name<T>()
 * in LiveArena.hpp, which can now delegate here.
 *
 * @tparam T Any type.
 * @return e.g. "Phasor" for "MayaFlux::Nodes::Phasor".
 */
template <typename T>
[[nodiscard]] constexpr std::string_view short_type_name() noexcept
{
    return detail::strip_namespaces(type_name<T>());
}

/**
 * @brief Returns the demangled fully qualified dynamic type name of @p obj.
 *
 * Allocates once per call via abi::__cxa_demangle on GCC/Clang. On MSVC
 * typeid().name() is already human-readable. Intended for display and
 * introspection paths, not hot loops.
 *
 * @param obj Any polymorphic object.
 */
template <typename T>
[[nodiscard]] inline std::string dynamic_type_name(const T& obj) noexcept
{
    const char* mangled = typeid(obj).name();
#if defined(MAYAFLUX_COMPILER_CLANG) || defined(MAYAFLUX_COMPILER_GCC)
    int status {};
    char* buf = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string result = (status == 0 && buf) ? buf : mangled;
    std::free(buf);
    return result;
#else
    return mangled;
#endif
}

/**
 * @brief Returns the unqualified dynamic type name of @p obj.
 * @param obj Any polymorphic object.
 * @return e.g. "Phasor".
 */
template <typename T>
[[nodiscard]] inline std::string short_dynamic_type_name(const T& obj) noexcept
{
    auto full = dynamic_type_name(obj);
    return std::string(detail::strip_namespaces(full));
}

/**
 * @brief Overload for shared_ptr: dereferences before querying dynamic type.
 * @param ptr shared_ptr to a polymorphic object.
 * @return Unqualified dynamic type name, or "null" if ptr is empty.
 */
template <typename T>
[[nodiscard]] inline std::string short_dynamic_type_name(const std::shared_ptr<T>& ptr) noexcept
{
    if (!ptr)
        return "null";
    return short_dynamic_type_name(*ptr);
}

} // namespace MayaFlux::Reflect
