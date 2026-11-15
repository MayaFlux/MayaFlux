#pragma once

#include "FactoryTable.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Attribute of a form element that names the function it calls.
 *
 * A call table reads it, the driver skips it, and the function's own
 * parameter names are the element's other attributes.
 */
inline constexpr std::string_view reserved_call_attribute { "call" };

/** @brief Attribute of an element that holds the initial value of its state. */
inline constexpr std::string_view reserved_state_attribute { "state" };

namespace Detail {

    template <typename F>
    struct FunctionTraits;

    template <typename R, typename... A>
    struct FunctionTraits<R (*)(A...)> {
        using Result = R;
        using Values = std::tuple<std::remove_cvref_t<A>...>;
        static constexpr size_t count = sizeof...(A);
    };

    template <typename R, typename... A>
    struct FunctionTraits<R (*)(A...) noexcept> : FunctionTraits<R (*)(A...)> { };

    template <typename R>
    struct FormState;

    template <typename T>
    struct FormState<Geometry::Form<T>> {
        using type = T;
    };

    template <auto Function, typename Source, typename Access, size_t Index, typename Values>
    bool decode_parameter(const Source& call, Values& values, std::array<bool, std::tuple_size_v<Values>>& given,
        std::string& error)
    {
        using Description = Reflect::Generated::Signature<Function>;
        using Parameter = std::tuple_element_t<Index, Values>;

        const auto text = Access::attribute(call, Description::parameters[Index]);
        if (!text)
            return true;
        if constexpr (Reflect::TextDecodable<Parameter>) {
            Reflect::TextError reason = Reflect::TextError::Invalid;
            if (!Reflect::TextCodec<Parameter>::decode(*text, std::get<Index>(values), reason)) {
                error = Reflect::located_message(Access::location(call),
                    "<" + std::string(Access::name(call)) + "> has an invalid " + std::string(Description::parameters[Index])
                        + " of " + std::string(*text));
                return false;
            }
            given[Index] = true;
            return true;
        } else {
            error = Reflect::located_message(Access::location(call),
                std::string(Description::parameters[Index]) + " of " + std::string(Description::name)
                    + " cannot be written in a document");
            return false;
        }
    }

    /**
     * Calls a described function with the arguments an element's attributes give by parameter
     * name. Arguments are the leading parameters, as in C++: a trailing run may be left to the
     * function's own defaults, which the sidecar's call applies because it calls by name, and a
     * later parameter cannot be given while an earlier one is skipped.
     */
    template <auto Function, typename Source, typename Access>
    std::optional<typename FunctionTraits<decltype(Function)>::Result> call_described(
        const Source& call, std::string& error)
    {
        using Traits = FunctionTraits<decltype(Function)>;
        using Result = typename Traits::Result;
        using Values = typename Traits::Values;
        using Description = Reflect::Generated::Signature<Function>;
        constexpr size_t count = Traits::count;
        constexpr size_t required = Description::required;
        static_assert(Description::parameters.size() == count, "the sidecar does not describe this function");

        const bool known = Access::for_each_attribute(call, [&](std::string_view name, std::string_view) {
            if (name == reserved_call_attribute
                || std::ranges::find(Description::parameters, name) != Description::parameters.end()) {
                return true;
            }
            error = Reflect::located_message(Access::location(call),
                "<" + std::string(Access::name(call)) + "> has an unsupported attribute " + std::string(name)
                    + " for " + std::string(Description::name));
            return false;
        });
        if (!known)
            return std::nullopt;

        Values values {};
        std::array<bool, count> given {};
        bool decoded = true;
        [&]<size_t... I>(std::index_sequence<I...>) {
            ((decoded = decoded && decode_parameter<Function, Source, Access, I>(call, values, given, error)), ...);
        }(std::make_index_sequence<count> {});
        if (!decoded)
            return std::nullopt;

        size_t provided = 0;
        while (provided < count && given[provided])
            ++provided;
        for (size_t i = provided + 1; i < count; ++i) {
            if (given[i]) {
                error = Reflect::located_message(Access::location(call),
                    std::string(Description::parameters[i]) + " is given without "
                        + std::string(Description::parameters[provided]));
                return std::nullopt;
            }
        }
        if (provided < required) {
            error = Reflect::located_message(Access::location(call),
                std::string(Description::name) + " needs " + std::string(Description::parameters[provided]));
            return std::nullopt;
        }

        std::optional<Result> result;
        [&]<size_t... N>(std::index_sequence<N...>) {
            ((provided == required + N
                 ? (result.emplace(Description::template call<required + N>(values)), true)
                 : false)
                || ...);
        }(std::make_index_sequence<count - required + 1> {});
        return result;
    }

}

/**
 * @class CallTable
 * @brief Functions a document calls by name, with the function's own parameter names as attributes.
 *
 * A plain value like every table here. Registering a function needs nothing written about it: its
 * generated Signature sidecar (MF_DESCRIBE_FUNCTION) supplies the parameter names, its own type
 * supplies their types, and each attribute is decoded with TextCodec. A form element names the
 * function and gives its arguments, as C++ passes them:
 *
 * @code
 * table.expose_form<&Geometry::horizontal_fader>();
 * // <element name="volume" state="0.5">
 * //     <form call="horizontal_fader" bounds="-0.5 -0.1 0.5 0.1" handle_w="0.04"/>
 * // </element>
 * @endcode
 *
 * A function returning a Geometry::Form of T makes an element whose state is a T, written in
 * the element's state attribute as TextCodec of T reads it, and a T{} when it is absent, as
 * Tend does.
 *
 * @tparam Source Element type with a Reflect::RecordAccess specialization.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
class CallTable {
public:
    using Placer = typename FactoryTable<Source, Access>::Placer;

    /**
     * @brief Decodes a form element and the element holding it into a Placer, or nullopt with an error.
     *
     * The element supplies the state, and the form element its call and arguments.
     */
    using Maker = std::function<std::optional<Placer>(const Source&, const Source&, std::string&)>;

    /**
     * @brief Register a described function that returns a Geometry::Form.
     * @tparam Function Pointer to the function, such as &Geometry::horizontal_fader.
     * @param name The name a document calls it by, the function's own name when empty.
     */
    template <auto Function>
        requires Reflect::DescribedFunction<Function>
    void expose_form(std::string name = {})
    {
        using Traits = Detail::FunctionTraits<decltype(Function)>;
        using State = typename Detail::FormState<typename Traits::Result>::type;

        if (name.empty()) {
            const std::string_view full = Reflect::Generated::Signature<Function>::name;
            name = std::string(full.substr(full.rfind("::") == std::string_view::npos ? 0 : full.rfind("::") + 2));
        }

        m_makers.insert_or_assign(std::move(name),
            Maker([](const Source& call, const Source& element, std::string& error) -> std::optional<Placer> {
                auto form = Detail::call_described<Function, Source, Access>(call, error);
                if (!form)
                    return std::nullopt;

                State initial {};
                if (const auto text = Access::attribute(element, reserved_state_attribute)) {
                    Reflect::TextError reason = Reflect::TextError::Invalid;
                    if (!Reflect::TextCodec<State>::decode(*text, initial, reason)) {
                        error = Reflect::located_message(Access::location(element),
                            "<" + std::string(Access::name(element)) + "> has an invalid state of " + std::string(*text));
                        return std::nullopt;
                    }
                }

                return Placer([form = std::move(*form), initial = std::move(initial)](
                                  Surface& surface, std::string& place_error) -> std::optional<uint32_t> {
                    Tend<State> tend {
                        .form = form,
                        .initial = initial,
                    };
                    tend.place(surface);
                    if (!tend.result) {
                        place_error = "could not place the element";
                        return std::nullopt;
                    }
                    return tend.result->element.id;
                });
            }));
    }

    /** @brief Add every call of another table under a name prefix. */
    bool merge(const CallTable& other, std::string_view prefix, std::string& error, bool replace = false)
    {
        return m_makers.merge_prefixed(other.m_makers, prefix, error, replace);
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_makers.find(name) != m_makers.end();
    }

    /** @brief Registered names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_makers)
            result.push_back(entry.first);
        return result;
    }

    /** @brief The maker of this name, or nullptr. */
    [[nodiscard]] const Maker* find(std::string_view name) const
    {
        const auto found = m_makers.find(name);
        return found == m_makers.end() ? nullptr : &found->second;
    }

private:
    Memory::FlatMap<std::string, Maker, std::less<>> m_makers;
};

}
