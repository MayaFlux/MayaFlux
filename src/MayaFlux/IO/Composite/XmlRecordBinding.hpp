#pragma once

#include "XmlAttributeBinding.hpp"

namespace MayaFlux::IO {

template <typename Node>
struct XmlRecordAccess;

template <>
struct XmlRecordAccess<XmlElementView> {
    static std::string_view name(const XmlElementView& node) { return node.name(); }
    static uint64_t row(const XmlElementView& node) { return node.row(); }

    static std::optional<std::string_view> attribute(const XmlElementView& node,
        std::string_view name)
    {
        return node.attribute(name);
    }

    template <typename Visitor>
    static bool for_each_attribute(const XmlElementView& node, Visitor&& visit)
    {
        return node.for_each_field([&](const XmlFieldView& field) {
            return field.kind == XmlFieldKind::Element || visit(field.name, field.value);
        });
    }

    template <typename Visitor>
    static bool for_each_child(const XmlElementView& node, Visitor&& visit)
    {
        return node.for_each_field([&](const XmlFieldView& field) {
            return field.kind == XmlFieldKind::Attribute || visit(*field.element);
        });
    }
};

/** @brief Bind described attributes and configured XML child collections. */
template <Reflect::Described Input, typename Node,
    typename Access = XmlRecordAccess<Node>>
class XmlRecordBinding {
public:
    XmlRecordBinding& required(std::string name)
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
    XmlRecordBinding& ignore(std::initializer_list<std::string_view> names)
    {
        for (const auto name : names)
            m_ignored.emplace_back(name);
        return *this;
    }

    template <auto Pointer>
    XmlRecordBinding& repeated(std::string child_name,
        std::string defaults_name = {},
        std::string required_item_attribute = {},
        size_t minimum = 0)
    {
        using Values = std::remove_cvref_t<decltype(std::declval<Input&>().*Pointer)>;
        using Item = typename Values::value_type;
        static_assert(std::same_as<Values, std::vector<Item>>);
        static_assert(Reflect::Described<Item>);

        ChildBinding binding;
        binding.name = std::move(child_name);
        binding.defaults_name = std::move(defaults_name);
        binding.decode = [name = binding.name, defaults_name = binding.defaults_name,
                             required_item_attribute = std::move(required_item_attribute), minimum](
                             Input& input, const Node& node, std::string& error,
                             uint64_t* error_row) {
            Item defaults {};
            bool defaults_seen = false;
            size_t count = 0;
            auto& values = input.*Pointer;
            const bool traversed = Access::for_each_child(node, [&](const Node& child) {
                if (error_row)
                    *error_row = Access::row(child);

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
                if (error_row)
                    *error_row = Access::row(node);
                error = "<" + std::string(Access::name(node)) + "> needs at least "
                    + std::to_string(minimum) + " " + name + " children";
                return false;
            }
            return true;
        };
        m_children.push_back(std::move(binding));
        return *this;
    }

    [[nodiscard]] std::optional<Input> read(const Node& node,
        std::string& error, uint64_t* error_row = nullptr) const
    {
        if (error_row)
            *error_row = Access::row(node);
        for (const auto& name : m_required) {
            if (!Access::attribute(node, name)) {
                error = "<" + label(node) + "> needs a " + name + " attribute";
                return std::nullopt;
            }
        }

        Input input {};
        const bool overlaid = overlay_xml_attributes(input,
            [&](const auto& visit) {
                return Access::for_each_attribute(node,
                    [&](std::string_view name, std::string_view value) {
                        return std::ranges::find(m_ignored, name) != m_ignored.end()
                            || visit(name, value);
                    });
            }, label(node), error);
        if (!overlaid)
            return std::nullopt;
        const bool known_children = Access::for_each_child(node, [&](const Node& child) {
            const auto child_name = Access::name(child);
            const bool known = std::ranges::any_of(m_children, [&](const ChildBinding& binding) {
                return child_name == binding.name
                    || (!binding.defaults_name.empty() && child_name == binding.defaults_name);
            });
            if (known)
                return true;
            if (error_row)
                *error_row = Access::row(child);
            error = "<" + std::string(child_name) + "> is not allowed in <"
                + std::string(Access::name(node)) + ">";
            return false;
        });
        if (!known_children)
            return std::nullopt;
        for (const ChildBinding& binding : m_children) {
            if (!binding.decode(input, node, error, error_row))
                return std::nullopt;
        }
        return input;
    }

private:
    template <Reflect::Described T>
    static bool overlay(T& target, const Node& node, std::string& error)
    {
        return overlay_xml_attributes(target,
            [&](const auto& visit) {
                return Access::for_each_attribute(node, visit);
            }, label(node), error);
    }

    static std::string label(const Node& node)
    {
        const auto id = Access::attribute(node, "id");
        return id ? std::string(*id) : std::string(Access::name(node));
    }

    static bool has_children(const Node& node)
    {
        bool found = false;
        Access::for_each_child(node, [&](const Node&) {
            found = true;
            return false;
        });
        return found;
    }

    struct ChildBinding {
        std::string name;
        std::string defaults_name;
        std::function<bool(Input&, const Node&, std::string&, uint64_t*)> decode;
    };

    std::vector<std::string> m_required;
    std::vector<std::string> m_ignored;
    std::vector<ChildBinding> m_children;
};

}
