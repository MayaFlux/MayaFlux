#pragma once

#include "XmlReader.hpp"

#include "MayaFlux/Kakshya/NDData/CompositeForest.hpp"

namespace MayaFlux::IO {

/**
 * @class XmlDocument
 * @brief A parsed XML document that owns its rows and hierarchy.
 *
 * Reads a file once and keeps the preorder Composite rows with their parent
 * index, so element views stay valid for as long as the document lives and no
 * reader has to stay open. Any consumer can bind described records from the
 * views, walk the forest, or select a branch as a Region. The document is
 * immutable and shared.
 *
 * Usage:
 * @code
 * std::string error;
 * auto document = XmlDocument::load("scene.xml", error);
 * if (document) {
 *     auto root = document->root_element();
 *     auto branch = document->forest().subtree_region(3);
 * }
 * @endcode
 */
class MAYAFLUX_API XmlDocument {
public:
    /**
     * @brief Parse a file into an owning document.
     * @param error Receives the reason when nullptr is returned.
     */
    [[nodiscard]] static std::shared_ptr<const XmlDocument> load(
        const std::string& path, std::string& error);

    /**
     * @brief Take ownership of rows in the XML node layout.
     * @param rows Rows laid out as xml_composite_layout in document order.
     * @param error Receives the reason when nullptr is returned.
     * @return The document, or nullptr when the layout differs or a row's
     * parent does not precede it.
     */
    [[nodiscard]] static std::shared_ptr<const XmlDocument> from_rows(
        Kakshya::CompositeArray rows, std::string& error);

    /** @brief The rows together with their parent index. */
    [[nodiscard]] const Kakshya::CompositeForest& forest() const noexcept { return *m_forest; }

    /** @brief Borrow the document root element. */
    [[nodiscard]] std::optional<XmlElementView> root_element() const noexcept;

    /** @brief Borrow an element by its row index. */
    [[nodiscard]] std::optional<XmlElementView> element_at(uint64_t row) const noexcept;

private:
    explicit XmlDocument(std::shared_ptr<const Kakshya::CompositeForest> forest) noexcept
        : m_forest(std::move(forest))
    {
    }

    std::shared_ptr<const Kakshya::CompositeForest> m_forest;
};

}
