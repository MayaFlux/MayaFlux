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

/**
 * @def MF_DESCRIBE_FUNCTION(Function)
 * @brief Requests the parameter names of a function at namespace scope.
 *
 * Name the function as an expression: &Geometry::horizontal_fader, an explicit
 * specialization such as &drag_with<float>, or a cast that picks one overload.
 * Include the header's generated sidecar (Name.signatures.hpp) once after the
 * last request. The mayaflux_signatures tool reads the request in its functions
 * mode and writes MayaFlux::Reflect::Generated::Signature<Function> into that
 * sidecar. In ordinary compilation the macro expands to nothing.
 */
#ifndef MF_DESCRIBE_FUNCTION
#define MF_DESCRIBE_FUNCTION(...)
#endif

namespace MayaFlux::Reflect::Generated {

template <typename T>
struct Description;

template <auto Pointer>
struct Member;

/**
 * @struct Signature
 * @brief The parameter names of a described function, which C++ cannot supply itself.
 *
 * A generated specialization holds name, required (how many leading parameters
 * have no default, so a call needs at least that many arguments), parameters
 * (every parameter name in order) and call<K>(values), which calls the function
 * by name with the first K elements of a tuple. The call is by name and not
 * through Function so the function's own default arguments apply to the ones
 * left out. The parameter types come from the function's own type.
 */
template <auto Function>
struct Signature;

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

/**
 * @concept DescribedFunction
 * @brief A function pointer whose generated sidecar defines Generated::Signature<Function>.
 */
template <auto Function>
concept DescribedFunction = requires { Generated::Signature<Function>::parameters; };

}
