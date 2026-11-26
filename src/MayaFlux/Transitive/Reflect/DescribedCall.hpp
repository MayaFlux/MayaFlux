#pragma once

#include "AttributeChecks.hpp"
#include "Describe.hpp"
#include "TextCodec.hpp"

namespace MayaFlux::Reflect {

/** @brief The result, argument values and argument count of a function pointer type. */
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

namespace Detail {

    template <typename T, typename... Tables>
    inline constexpr size_t name_table_index = [] {
        constexpr std::array<bool, sizeof...(Tables)> holds {
            std::is_same_v<std::remove_cvref_t<typename Tables::value_type::second_type>, T>...
        };
        size_t index = 0;
        while (index < holds.size() && !holds[index])
            ++index;
        return index;
    }();

    template <auto Function, typename Access, size_t Index, typename Source, typename Values, typename... Tables>
    bool read_argument(const Source& call, Values& values, std::array<bool, std::tuple_size_v<Values>>& given,
        std::string& error, const std::tuple<const Tables&...>& tables)
    {
        using Signature = Generated::Signature<Function>;
        using Parameter = std::tuple_element_t<Index, Values>;
        constexpr size_t table = name_table_index<Parameter, Tables...>;

        const auto text = Access::attribute(call, Signature::parameters[Index]);
        if (!text)
            return true;

        if constexpr (table < sizeof...(Tables)) {
            const auto& named = std::get<table>(tables);
            const auto found = named.find(std::string_view(*text));
            if (found == named.end()) {
                error = located_message(Access::location(call),
                    "<" + std::string(Access::name(call)) + "> refers to unknown "
                        + std::string(Signature::parameters[Index]) + " " + std::string(*text));
                return false;
            }
            std::get<Index>(values) = found->second;
        } else if constexpr (TextDecodable<Parameter>) {
            TextError reason = TextError::Invalid;
            if (!TextCodec<Parameter>::decode(*text, std::get<Index>(values), reason)) {
                error = located_message(Access::location(call),
                    "<" + std::string(Access::name(call)) + "> has an invalid "
                        + std::string(Signature::parameters[Index]) + " of " + std::string(*text));
                return false;
            }
        } else {
            error = located_message(Access::location(call),
                std::string(Signature::parameters[Index]) + " of " + std::string(Signature::name)
                    + " cannot be written in a document");
            return false;
        }
        given[Index] = true;
        return true;
    }

}

/**
 * @brief Call a described function with the arguments an element's attributes give by parameter name.
 *
 * The function's generated Signature sidecar (MF_DESCRIBE_FUNCTION) supplies the
 * parameter names and its own type their types. Arguments are the leading
 * parameters, as in C++: a trailing run may be left to the function's own
 * defaults, and a later parameter cannot be given while an earlier one is
 * skipped.
 *
 * An attribute is decoded with TextCodec, unless a table for the parameter's
 * type is passed: its text is then a name in that table, and the entry is the
 * argument. That is how an element refers to what cannot be written down, such
 * as an object that already exists or a callable. The first table of a type is
 * the one used. A parameter that is neither decodable nor covered by a table
 * can only be left to its default.
 *
 * @code
 * std::map<std::string, std::shared_ptr<Voice>, std::less<>> voices { { "low", low } };
 * std::string error;
 *
 * // <call voice="low" gain="0.5"/>
 * auto result = Reflect::call_described<&make_route>(element, error, {}, voices);
 * @endcode
 *
 * @tparam Function Pointer to a described function that returns a value.
 * @param call    Element holding the arguments as attributes.
 * @param error   Receives a located message when nullopt is returned.
 * @param ignored Attributes of the element that are not arguments.
 * @param tables  Maps from name to value, keyed by std::string and searchable
 *                with a std::string_view, such as Memory::FlatMap or std::map
 *                with std::less<>.
 * @return What the function returned, or nullopt.
 */
template <auto Function, typename Source, typename Access = RecordAccess<Source>, typename... Tables>
    requires DescribedFunction<Function>
[[nodiscard]] std::optional<typename FunctionTraits<decltype(Function)>::Result> call_described(
    const Source& call, std::string& error,
    std::span<const std::string_view> ignored = {},
    const Tables&... tables)
{
    using Traits = FunctionTraits<decltype(Function)>;
    using Result = typename Traits::Result;
    using Values = typename Traits::Values;
    using Signature = Generated::Signature<Function>;
    constexpr size_t count = Traits::count;
    constexpr size_t required = Signature::required;
    static_assert(Signature::parameters.size() == count, "the sidecar does not describe this function");

    const bool known = Access::for_each_attribute(call, [&](std::string_view name, std::string_view) {
        if (std::ranges::find(ignored, name) != ignored.end()
            || std::ranges::find(Signature::parameters, name) != Signature::parameters.end()) {
            return true;
        }
        error = located_message(Access::location(call),
            "<" + std::string(Access::name(call)) + "> has an unsupported attribute " + std::string(name)
                + " for " + std::string(Signature::name));
        return false;
    });
    if (!known)
        return std::nullopt;

    Values values {};
    std::array<bool, count> given {};
    const std::tuple<const Tables&...> named { tables... };
    bool read = true;
    [&]<size_t... I>(std::index_sequence<I...>) {
        ((read = read && Detail::read_argument<Function, Access, I>(call, values, given, error, named)), ...);
    }(std::make_index_sequence<count> {});
    if (!read)
        return std::nullopt;

    size_t provided = 0;
    while (provided < count && given[provided])
        ++provided;
    for (size_t i = provided + 1; i < count; ++i) {
        if (given[i]) {
            error = located_message(Access::location(call),
                std::string(Signature::parameters[i]) + " is given without "
                    + std::string(Signature::parameters[provided]));
            return std::nullopt;
        }
    }
    if (provided < required) {
        error = located_message(Access::location(call),
            std::string(Signature::name) + " needs " + std::string(Signature::parameters[provided]));
        return std::nullopt;
    }

    std::optional<Result> result;
    [&]<size_t... N>(std::index_sequence<N...>) {
        ((provided == required + N
             ? (result.emplace(Signature::template call<required + N>(values)), true)
             : false)
            || ...);
    }(std::make_index_sequence<count - required + 1> {});
    return result;
}

}
