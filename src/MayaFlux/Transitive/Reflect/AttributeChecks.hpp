#pragma once

#include "RecordBinding.hpp"

namespace MayaFlux::Reflect {

/**
 * @brief A diagnostic message prefixed with the location of the element it concerns.
 * @param location Position reported by RecordAccess::location, such as an XML row.
 */
[[nodiscard]] inline std::string located_message(uint64_t location, std::string_view message)
{
    return "row " + std::to_string(location) + ": " + std::string(message);
}

/**
 * @class AttributeIndex
 * @brief The values of one attribute across a tree, each claimed by one element.
 *
 * Claiming a value records the element that holds it, and a second claim of the
 * same value is refused with the location of the first. It serves any source
 * with a RecordAccess, so the same check holds for an XML document, a script
 * or any other element tree.
 *
 * Usage:
 * @code
 * AttributeIndex<Element> ids("id");
 * std::string error;
 * for (const Element& element : elements) {
 *     if (!ids.claim(element, error))
 *         return false;
 * }
 * @endcode
 *
 * @tparam Source Element type with a RecordAccess specialization.
 */
template <typename Source, typename Access = RecordAccess<Source>>
class AttributeIndex {
public:
    /** @param attribute Name of the attribute whose values are claimed. */
    explicit AttributeIndex(std::string attribute)
        : m_attribute(std::move(attribute))
    {
    }

    /**
     * @brief Claim the attribute's value for an element.
     * @param error Receives a located message when false is returned: the
     * attribute is missing or empty, or another element already holds the value.
     */
    bool claim(const Source& source, std::string& error)
    {
        const auto value = Access::attribute(source, m_attribute);
        if (!value || value->empty()) {
            error = located_message(Access::location(source),
                "<" + std::string(Access::name(source)) + "> needs a non-empty " + m_attribute);
            return false;
        }
        const auto [slot, inserted] = m_claimed.emplace(std::string(*value), Access::location(source));
        if (!inserted) {
            error = located_message(Access::location(source),
                m_attribute + " " + std::string(*value) + " is already used at row "
                    + std::to_string(slot->second));
            return false;
        }
        return true;
    }

    [[nodiscard]] bool contains(std::string_view value) const
    {
        return m_claimed.find(value) != m_claimed.end();
    }

    /** @brief The location of the element that holds a value, or nullopt. */
    [[nodiscard]] std::optional<uint64_t> location_of(std::string_view value) const
    {
        const auto found = m_claimed.find(value);
        if (found == m_claimed.end())
            return std::nullopt;
        return found->second;
    }

    [[nodiscard]] const std::string& attribute() const noexcept { return m_attribute; }

private:
    std::string m_attribute;
    std::map<std::string, uint64_t, std::less<>> m_claimed;
};

/**
 * @brief Check that an element's attribute names a value that exists.
 *
 * @param attribute Name of the referring attribute, which also names what it refers to.
 * @param exists Callable taking the value and returning whether it is known,
 * for instance AttributeIndex::contains or a lookup in any container.
 * @param error Receives a located message when nullopt is returned: the attribute
 * is missing, or its value is unknown.
 * @return The value of the attribute.
 */
template <typename Source, typename Access = RecordAccess<Source>, typename Exists>
[[nodiscard]] std::optional<std::string_view> check_reference(const Source& source,
    std::string_view attribute, Exists&& exists, std::string& error)
{
    const auto value = Access::attribute(source, attribute);
    if (!value) {
        error = located_message(Access::location(source),
            "<" + std::string(Access::name(source)) + "> needs a " + std::string(attribute) + " attribute");
        return std::nullopt;
    }
    if (!std::invoke(std::forward<Exists>(exists), *value)) {
        error = located_message(Access::location(source),
            "<" + std::string(Access::name(source)) + "> refers to unknown " + std::string(attribute)
                + " " + std::string(*value));
        return std::nullopt;
    }
    return value;
}

}
