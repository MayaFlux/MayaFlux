#pragma once

/**
 * @def MF_DESCRIBE(Type)
 * @brief Requests a generated member description for a struct at namespace scope.
 *
 * Place it on its own line after the complete definition, and include the
 * header's generated sidecar (Name.describe.hpp) once after the last request.
 * The mayaflux_reflect tool reads the request and writes
 * MayaFlux::Reflect::Generated::Description<Type> into that sidecar. In
 * ordinary compilation the macro expands to nothing.
 */
#ifndef MF_DESCRIBE
#define MF_DESCRIBE(...)
#endif

namespace MayaFlux::Reflect::Generated {

template <typename T>
struct Description;

template <auto Pointer>
struct Member;

/**
 * @struct Member
 * @brief One described data member: its name and its member pointer.
 *
 * Owner is the class that declares the member, which may be a base of the
 * described type.
 */
template <typename Owner, typename Value, Value Owner::* Pointer>
struct Member<Pointer> {
    using owner_type = Owner;
    using value_type = Value;

    std::string_view name;

    static constexpr auto pointer = Pointer;

    constexpr decltype(auto) get(Owner& object) const { return object.*Pointer; }
    constexpr decltype(auto) get(const Owner& object) const { return object.*Pointer; }
};

}

namespace MayaFlux::Reflect {

/**
 * @concept Described
 * @brief A type whose generated sidecar defines Generated::Description<T>::members().
 *
 * The sidecar of a header is included after its namespaces close, so code in the
 * same header that sits before that include does not see the description.
 */
template <typename T>
concept Described = requires { Generated::Description<std::remove_cv_t<T>>::members(); };

}
