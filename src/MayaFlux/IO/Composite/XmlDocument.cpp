#include "XmlDocument.hpp"

namespace MayaFlux::IO {

std::shared_ptr<const XmlDocument> XmlDocument::load(const std::string& path, std::string& error)
{
    XmlReader reader;
    if (!reader.open(path)) {
        error = reader.get_last_error();
        return nullptr;
    }
    auto rows = reader.take_rows();
    if (!rows) {
        error = "No XML rows were parsed";
        return nullptr;
    }
    return from_rows(std::move(*rows), error);
}

std::shared_ptr<const XmlDocument> XmlDocument::from_rows(
    Kakshya::CompositeArray rows, std::string& error)
{
    if (!rows.layout().same_structure(xml_composite_layout())) {
        error = "Rows do not use the XML node layout";
        return nullptr;
    }
    auto forest = Kakshya::CompositeForest::build(std::move(rows), "parent", error);
    if (!forest)
        return nullptr;
    if (!forest->index().preorder()) {
        error = "XML rows are not in document order";
        return nullptr;
    }
    return std::shared_ptr<const XmlDocument>(new XmlDocument(std::move(forest)));
}

std::optional<XmlElementView> XmlDocument::root_element() const noexcept
{
    const auto& rows = m_forest->rows();
    const auto& index = m_forest->index();
    for (size_t row = 0; row < rows.size(); row += index.subtree_size(row)) {
        const auto source = rows.at(row);
        if (source && source->text("kind") == std::string_view("element"))
            return XmlElementView(rows, index, row);
    }
    return std::nullopt;
}

std::optional<XmlElementView> XmlDocument::element_at(uint64_t row) const noexcept
{
    const auto& rows = m_forest->rows();
    if (row >= rows.size())
        return std::nullopt;
    const auto source = rows.at(static_cast<size_t>(row));
    if (!source || source->text("kind") != std::string_view("element"))
        return std::nullopt;
    return XmlElementView(rows, m_forest->index(), static_cast<size_t>(row));
}

}
