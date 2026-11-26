#pragma once

#include "DescribedCall.hpp"
#include "FunctionCall.hpp"

#include "MayaFlux/Transitive/Memory/FlatMap.hpp"

#include <typeindex>

namespace MayaFlux::Reflect {

template <>
struct RecordAccess<FunctionCall> {
    static std::string_view name(const FunctionCall& call) { return call.first; }
    static uint64_t location(const FunctionCall&) { return 0; }

    static std::optional<std::string_view> attribute(const FunctionCall& call, std::string_view name)
    {
        const auto found = call.second.find(name);
        if (found == call.second.end())
            return std::nullopt;
        return std::string_view(found->second);
    }

    template <typename Visitor>
    static bool for_each_attribute(const FunctionCall& call, Visitor&& visit)
    {
        for (const auto& [key, value] : call.second) {
            if (!visit(std::string_view(key), std::string_view(value)))
                return false;
        }
        return true;
    }

    template <typename Visitor>
    static bool for_each_child(const FunctionCall&, Visitor&&)
    {
        return true;
    }
};

/**
 * @class FunctionTable
 * @brief Described functions that make a value from a FunctionCall.
 *
 * A plain value the caller owns; nothing is registered in advance. Each exposed
 * function is described with MF_DESCRIBE_FUNCTION and returns what it makes,
 * usually a std::function. make() reads its arguments as call_described() does.
 *
 * @code
 * using Voices = std::map<std::string, std::shared_ptr<Voice>, std::less<>>;
 *
 * Reflect::FunctionTable<Voices> calls;
 * calls.expose<&make_route>();
 *
 * Reflect::FunctionCall call { "make_route", { { "voice", "low" } } };
 * std::string error;
 * auto route = calls.make<std::function<void()>>(call, error, voices);
 * @endcode
 *
 * @tparam Tables Name to value maps an argument may refer into, one per type.
 */
template <typename... Tables>
class FunctionTable {
public:
    /**
     * @brief Add a described function.
     * @tparam Function Pointer to the function.
     * @param name The name a call uses, the function's own name when empty.
     */
    template <auto Function>
        requires DescribedFunction<Function>
    void expose(std::string name = {})
    {
        using Result = typename FunctionTraits<decltype(Function)>::Result;

        if (name.empty()) {
            const std::string_view full = Generated::Signature<Function>::name;
            const size_t scope = full.rfind("::");
            name = std::string(scope == std::string_view::npos ? full : full.substr(scope + 2));
        }

        m_entries.insert_or_assign(std::move(name),
            Entry { std::type_index(typeid(Result)),
                Maker([](const FunctionCall& call, void* made, std::string& error,
                          const Tables&... tables) {
                    auto result = call_described<Function>(call, error, {}, tables...);
                    if (!result)
                        return false;
                    static_cast<std::optional<Result>*>(made)->emplace(std::move(*result));
                    return true;
                }) });
    }

    /**
     * @brief Make what a call names.
     * @tparam Made The return type of the function the call names.
     * @param error Receives the reason when nullopt is returned: no function of
     * that name, one that makes another type, or arguments it does not accept.
     */
    template <typename Made>
    [[nodiscard]] std::optional<Made> make(const FunctionCall& call, std::string& error,
        const Tables&... tables) const
    {
        const auto found = m_entries.find(call.first);
        if (found == m_entries.end()) {
            error = "no function named " + call.first;
            return std::nullopt;
        }
        if (found->second.first != std::type_index(typeid(Made))) {
            error = call.first + " does not make what is asked for";
            return std::nullopt;
        }
        std::optional<Made> made;
        if (!found->second.second(call, &made, error, tables...))
            return std::nullopt;
        return made;
    }

    /** @brief Add every function of another table under a name prefix. */
    bool merge(const FunctionTable& other, std::string_view prefix, std::string& error, bool replace = false)
    {
        return m_entries.merge_prefixed(other.m_entries, prefix, error, replace);
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_entries.find(name) != m_entries.end();
    }

    /** @brief Exposed names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_entries)
            result.push_back(entry.first);
        return result;
    }

private:
    using Maker = std::function<bool(const FunctionCall&, void*, std::string&, const Tables&...)>;
    using Entry = std::pair<std::type_index, Maker>;

    Memory::FlatMap<std::string, Entry, std::less<>> m_entries;
};

}
