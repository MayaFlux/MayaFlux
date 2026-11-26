#pragma once

#include "ElementHit.hpp"
#include "FactoryTable.hpp"

#include "MayaFlux/Transitive/Reflect/DescribedCall.hpp"

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

    template <typename R>
    struct FormState;

    template <typename T>
    struct FormState<Geometry::Form<T>> {
        using type = T;
    };

    /** A Placer that places a form with an initial state on a Surface and returns its element id. */
    template <typename Placer, typename State>
    Placer form_placer(Geometry::Form<State> form, State initial)
    {
        return Placer([form = std::move(form), initial = std::move(initial)](
                          Surface& surface, std::string& error) -> std::optional<uint32_t> {
            Tend<State> tend {
                .form = form,
                .initial = initial,
            };
            tend.place(surface);
            if (!tend.result) {
                error = "could not place the element";
                return std::nullopt;
            }
            return tend.result->element.id;
        });
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
     * The element supplies the state, and the form element its call and arguments. The hit is what
     * the element's own spatial children say, written after the form's.
     */
    using Maker = std::function<std::optional<Placer>(
        const Source&, const Source&, const ElementHit&, std::string&)>;

    /**
     * @brief Register a described function that returns a Geometry::Form.
     * @tparam Function Pointer to the function, such as &Geometry::horizontal_fader.
     * @param name The name a document calls it by, the function's own name when empty.
     */
    template <auto Function>
        requires Reflect::DescribedFunction<Function>
    void expose_form(std::string name = {})
    {
        using Traits = Reflect::FunctionTraits<decltype(Function)>;
        using State = typename Detail::FormState<typename Traits::Result>::type;

        if (name.empty()) {
            const std::string_view full = Reflect::Generated::Signature<Function>::name;
            name = std::string(full.substr(full.rfind("::") == std::string_view::npos ? 0 : full.rfind("::") + 2));
        }

        m_makers.insert_or_assign(std::move(name),
            Maker([](const Source& call, const Source& element, const ElementHit& hit,
                      std::string& error) -> std::optional<Placer> {
                static constexpr std::array ignored { reserved_call_attribute };
                auto form = Reflect::call_described<Function, Source, Access>(call, error, ignored);
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

                apply_hit(*form, hit);
                return Detail::form_placer<Placer, State>(std::move(*form), std::move(initial));
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
