#pragma once

#include "XmlReader.hpp"

#include "MayaFlux/Kakshya/NDData/CompositeForest.hpp"
#include "MayaFlux/Kakshya/NDData/CompositeInsertion.hpp"

namespace MayaFlux::IO {

struct XmlIndexedRow {
    std::string_view kind;
    std::string_view name;
    std::string_view value;
    std::optional<uint64_t> parent;
};

/** @brief Index parent links in an XML Composite row array. */
class XmlRowIndex {
public:
    explicit XmlRowIndex(const Kakshya::CompositeArray& rows)
        : m_rows(rows)
        , m_forest(Kakshya::index_composite_forest(rows, "parent", m_error, &m_error_row))
    {
    }

    [[nodiscard]] bool valid() const noexcept { return m_forest.has_value(); }
    [[nodiscard]] uint64_t error_row() const noexcept { return m_error_row; }
    [[nodiscard]] const std::string& error() const noexcept { return m_error; }
    [[nodiscard]] size_t size() const noexcept { return m_rows.size(); }

    [[nodiscard]] XmlIndexedRow row(size_t index) const
    {
        const auto source = m_rows.at(index);
        return {
            .kind = source->text("kind").value_or(std::string_view {}),
            .name = source->text("name").value_or(std::string_view {}),
            .value = source->text("value").value_or(std::string_view {}),
            .parent = source->get<uint64_t>("parent"),
        };
    }

    [[nodiscard]] std::span<const uint64_t> children(size_t index) const noexcept
    {
        return m_forest ? m_forest->children(index) : std::span<const uint64_t> {};
    }

private:
    const Kakshya::CompositeArray& m_rows;
    std::string m_error;
    uint64_t m_error_row {};
    std::optional<Kakshya::ForestIndex> m_forest;
};

/** @brief Append elements and attributes in the XML Composite row layout. */
class XmlRowAppender {
public:
    explicit XmlRowAppender(Kakshya::CompositeInsertion& insertion)
        : m_insertion(insertion)
    {
    }

    [[nodiscard]] std::optional<uint64_t> element(std::string_view name,
        std::optional<uint64_t> parent = std::nullopt)
    {
        const auto row = m_insertion.append();
        if (!m_insertion.set_text(row, "kind", "element")
            || !m_insertion.set_text(row, "name", name)
            || (parent && !m_insertion.set(row, "parent", *parent))) {
            return std::nullopt;
        }
        return static_cast<uint64_t>(row);
    }

    bool attribute(uint64_t parent, std::string_view name, std::string_view value)
    {
        const auto row = m_insertion.append();
        return m_insertion.set_text(row, "kind", "attribute")
            && m_insertion.set_text(row, "name", name)
            && m_insertion.set_text(row, "value", value)
            && m_insertion.set(row, "parent", parent);
    }

    /** @brief Append a tree with name, attributes, and children members. */
    template <typename Node>
    bool tree(const Node& node, std::optional<uint64_t> parent = std::nullopt)
    {
        const auto owner = element(node.name, parent);
        if (!owner)
            return false;
        for (const auto& [name, value] : node.attributes) {
            if (!attribute(*owner, name, value))
                return false;
        }
        for (const auto& child : node.children) {
            if (!tree(child, owner))
                return false;
        }
        return true;
    }

private:
    Kakshya::CompositeInsertion& m_insertion;
};

}
