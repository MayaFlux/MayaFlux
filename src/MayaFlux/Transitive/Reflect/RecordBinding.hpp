#pragma once

#include "TextBinding.hpp"

namespace MayaFlux::Reflect {

/**
 * @brief How a record binding reads one source element of a document tree.
 *
 * Specialize for each source type. A specialization provides:
 * name(source), location(source) as a diagnostic position,
 * attribute(source, name) returning optional text,
 * for_each_attribute(source, visit) with visit(name, text), and
 * for_each_child(source, visit) with visit(child). Visitors return false to stop.
 */
template <typename Source>
struct RecordAccess;

/**
 * @class RecordBinding
 * @brief Bind described attributes and configured child collections of a source element.
 *
 * Attributes overlay onto the described Input by member name. Required
 * attributes, ignored attributes and repeated children with shared defaults
 * are configured on the binding. Any source with a RecordAccess specialization
 * can be bound.
 */
template <Described Input, typename Source,
    typename Access = RecordAccess<Source>>
class RecordBinding {
public:
    RecordBinding& required(std::string name)
    {
        m_required.push_back(std::move(name));
        return *this;
    }

    /**
     * @brief Leave these attributes to other readers of the same element.
     *
     * Listed names are neither decoded nor rejected. Described members still
     * decode strictly, so do not list one.
     */
    RecordBinding& ignore(std::initializer_list<std::string_view> names)
    {
        for (const auto name : names)
            m_ignored.emplace_back(name);
        return *this;
    }

    template <auto Pointer>
    RecordBinding& repeated(std::string child_name,
        std::string defaults_name = {},
        std::string required_item_attribute = {},
        size_t minimum = 0)
    {
        using Values = std::remove_cvref_t<decltype(std::declval<Input&>().*Pointer)>;
        using Item = typename Values::value_type;
        static_assert(std::same_as<Values, std::vector<Item>>);
        static_assert(Described<Item>);

        ChildBinding binding;
        binding.name = std::move(child_name);
        binding.defaults_name = std::move(defaults_name);
        binding.decode = [name = binding.name, defaults_name = binding.defaults_name,
                             required_item_attribute = std::move(required_item_attribute), minimum](
                             Input& input, const Source& source, std::string& error,
                             uint64_t* error_location) {
            Item defaults {};
            bool defaults_seen = false;
            size_t count = 0;
            auto& values = input.*Pointer;
            const bool traversed = Access::for_each_child(source, [&](const Source& child) {
                if (error_location)
                    *error_location = Access::location(child);

                const auto child_name = Access::name(child);
                if (!defaults_name.empty() && child_name == defaults_name) {
                    if (defaults_seen || count != 0) {
                        error = "<" + defaults_name + "> must appear once before the "
                            + name + " children";
                        return false;
                    }
                    defaults_seen = true;
                    if (!required_item_attribute.empty()
                        && Access::attribute(child, required_item_attribute)) {
                        error = "<" + defaults_name + "> has an unsupported attribute "
                            + required_item_attribute;
                        return false;
                    }
                    if (has_children(child)) {
                        error = "<" + defaults_name + "> may not have children";
                        return false;
                    }
                    return overlay(defaults, child, error);
                }
                if (child_name != name)
                    return true;
                if (!required_item_attribute.empty()
                    && !Access::attribute(child, required_item_attribute)) {
                    error = "<" + label(child) + "> needs a "
                        + required_item_attribute + " attribute";
                    return false;
                }
                if (has_children(child)) {
                    error = "<" + name + "> may not have children";
                    return false;
                }
                Item item = defaults;
                if (!overlay(item, child, error))
                    return false;
                values.push_back(std::move(item));
                ++count;
                return true;
            });
            if (!traversed)
                return false;
            if (count < minimum) {
                if (error_location)
                    *error_location = Access::location(source);
                error = "<" + std::string(Access::name(source)) + "> needs at least "
                    + std::to_string(minimum) + " " + name + " children";
                return false;
            }
            return true;
        };
        m_children.push_back(std::move(binding));
        return *this;
    }

    /**
     * @brief Decode the source element into a new Input.
     * @param error_location Receives the position of the failing element when given.
     */
    [[nodiscard]] std::optional<Input> read(const Source& source,
        std::string& error, uint64_t* error_location = nullptr) const
    {
        if (error_location)
            *error_location = Access::location(source);
        for (const auto& name : m_required) {
            if (!Access::attribute(source, name)) {
                error = "<" + label(source) + "> needs a " + name + " attribute";
                return std::nullopt;
            }
        }

        Input input {};
        const bool overlaid = overlay_text_fields(input,
            [&](const auto& visit) {
                return Access::for_each_attribute(source,
                    [&](std::string_view name, std::string_view value) {
                        return std::ranges::find(m_ignored, name) != m_ignored.end()
                            || visit(name, value);
                    });
            }, label(source), error);
        if (!overlaid)
            return std::nullopt;
        const bool known_children = Access::for_each_child(source, [&](const Source& child) {
            const auto child_name = Access::name(child);
            const bool known = std::ranges::any_of(m_children, [&](const ChildBinding& binding) {
                return child_name == binding.name
                    || (!binding.defaults_name.empty() && child_name == binding.defaults_name);
            });
            if (known)
                return true;
            if (error_location)
                *error_location = Access::location(child);
            error = "<" + std::string(child_name) + "> is not allowed in <"
                + std::string(Access::name(source)) + ">";
            return false;
        });
        if (!known_children)
            return std::nullopt;
        for (const ChildBinding& binding : m_children) {
            if (!binding.decode(input, source, error, error_location))
                return std::nullopt;
        }
        return input;
    }

private:
    template <Described T>
    static bool overlay(T& target, const Source& source, std::string& error)
    {
        return overlay_text_fields(target,
            [&](const auto& visit) {
                return Access::for_each_attribute(source, visit);
            }, label(source), error);
    }

    static std::string label(const Source& source)
    {
        const auto id = Access::attribute(source, "id");
        return id ? std::string(*id) : std::string(Access::name(source));
    }

    static bool has_children(const Source& source)
    {
        bool found = false;
        Access::for_each_child(source, [&](const Source&) {
            found = true;
            return false;
        });
        return found;
    }

    struct ChildBinding {
        std::string name;
        std::string defaults_name;
        std::function<bool(Input&, const Source&, std::string&, uint64_t*)> decode;
    };

    std::vector<std::string> m_required;
    std::vector<std::string> m_ignored;
    std::vector<ChildBinding> m_children;
};

}
