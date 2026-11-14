#pragma once

#include "CompositeReader.hpp"

#include "MayaFlux/Kakshya/ForestIndex.hpp"

namespace MayaFlux::IO {

/**
 * @brief Composite row layout used by the XML reader and writer.
 *
 * Rows are in document preorder. The optional parent field holds the global
 * index of the owning element or declaration; top-level nodes have no parent.
 * Attribute rows immediately follow their owner and precede child nodes.
 * Kind is element, attribute, text, cdata, comment, pi, declaration, or
 * doctype. Name and value are present where that node kind supplies them.
 */
[[nodiscard]] MAYAFLUX_API Kakshya::CompositeLayout xml_composite_layout();

enum class XmlFieldKind { Attribute, Element };

struct XmlFieldView;

/**
 * @brief Borrowed XML element with named attributes and immediate child elements.
 * @note The view and its strings are invalid after the reader is closed,
 * reopened, moved, or destroyed.
 */
class MAYAFLUX_API XmlElementView {
public:
    [[nodiscard]] uint64_t row() const noexcept { return m_row; }
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::optional<std::string_view> attribute(std::string_view name) const noexcept;

    /**
     * @brief Visit attributes and immediate child elements in document order.
     * @return False if a boolean visitor stops traversal; true otherwise.
     */
    template <typename Fn>
    bool for_each_field(Fn&& visitor) const;

private:
    friend class XmlReader;

    XmlElementView(const Kakshya::CompositeArray& rows,
        const Kakshya::ForestIndex& forest, size_t row) noexcept
        : m_rows(&rows)
        , m_forest(&forest)
        , m_row(row)
    {
    }

    const Kakshya::CompositeArray* m_rows;
    const Kakshya::ForestIndex* m_forest;
    size_t m_row;
};

/**
 * @brief One attribute or immediate child element borrowed from an XML element.
 * Attribute value is text; element is present only for a child element.
 */
struct XmlFieldView {
    XmlFieldKind kind;
    std::string_view name;
    std::string_view value;
    uint64_t row;
    std::optional<XmlElementView> element;
};

template <typename Fn>
bool XmlElementView::for_each_field(Fn&& visitor) const
{
    const size_t end = m_row + m_forest->subtree_size(m_row);
    for (size_t index = m_row + 1; index < end; index += m_forest->subtree_size(index)) {
        const auto source = m_rows->at(index);
        if (!source)
            continue;

        const auto kind = source->text("kind");
        if (!kind || (*kind != "attribute" && *kind != "element"))
            continue;

        const auto field_name = source->text("name");
        if (!field_name)
            continue;

        const XmlFieldView field {
            .kind = *kind == "attribute" ? XmlFieldKind::Attribute : XmlFieldKind::Element,
            .name = *field_name,
            .value = source->text("value").value_or(std::string_view {}),
            .row = static_cast<uint64_t>(index),
            .element = *kind == "element"
                ? std::optional<XmlElementView>(XmlElementView(*m_rows, *m_forest, index))
                : std::nullopt,
        };
        if constexpr (std::same_as<std::invoke_result_t<Fn&, const XmlFieldView&>, bool>) {
            if (!std::invoke(visitor, field))
                return false;
        } else {
            std::invoke(visitor, field);
        }
    }
    return true;
}

/**
 * @class XmlReader
 * @brief Read an XML document into preorder Composite rows.
 *
 * The complete document is parsed on open(). Bounded reads copy rows from
 * that owning array and retain global parent indices. This is not streaming
 * XML input. Parsed character data, CDATA, comments, processing instructions,
 * declarations, and doctypes remain separate rows. Entity references are
 * expanded by pugixml, so the result preserves document semantics rather
 * than original source bytes.
 *
 * Usage:
 * @code
 * XmlReader reader;
 * if (reader.open("controls.xml")) {
 *     auto layout = reader.get_layout();
 *     auto nodes = reader.read_composite();
 * }
 * @endcode
 *
 * @note The input limit is 64 MiB, one million rows, and 256 element levels.
 * A batch is not a standalone document because its parent indices refer to
 * the complete preorder sequence.
 */
class MAYAFLUX_API XmlReader : public CompositeReader {
public:
    /** @brief Accept paths with the .xml extension. */
    [[nodiscard]] bool can_read(const std::string& filepath) const override;

    /**
     * @brief Parse an XML file and establish the fixed node layout.
     * @return True for a parsed document with one root element.
     */
    bool open(const std::string& filepath,
        FileReadOptions options = FileReadOptions::ALL) override;

    /** @brief Release parsed rows and reset the read position. */
    void close() override;

    /** @brief Whether parsed rows are available. */
    [[nodiscard]] bool is_open() const override { return m_rows.has_value(); }

    /** @brief XML format, MIME type, and source file size. */
    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;

    /** @brief XML nodes do not declare FileRegion values. */
    [[nodiscard]] std::vector<FileRegion> get_regions() const override { return {}; }

    /** @brief Copy all rows remaining after the current position. */
    [[nodiscard]] std::optional<Kakshya::CompositeArray> read_composite() override;

    /**
     * @brief Copy at most max_elements rows and advance the position.
     * @return An empty array at end of document, or nullopt on failure.
     */
    [[nodiscard]] std::optional<Kakshya::CompositeArray> read_next(size_t max_elements) override;

    /** @brief Copy the finalized node layout while open. */
    [[nodiscard]] std::optional<Kakshya::CompositeLayout> get_layout() const override;

    /** @brief Borrow the document root element, independent of the row read position. */
    [[nodiscard]] std::optional<XmlElementView> root_element() const noexcept;

    /** @brief Borrow an element by its global XML row index. */
    [[nodiscard]] std::optional<XmlElementView> element_at(uint64_t row) const noexcept;

    /** @brief Borrow the parsed XML rows while the reader remains open. */
    [[nodiscard]] const Kakshya::CompositeArray* rows_view() const noexcept
    {
        return m_rows ? &*m_rows : nullptr;
    }

    /** @brief Global index of the next row. */
    [[nodiscard]] std::vector<uint64_t> get_read_position() const override { return { m_position }; }

    /** @brief Seek to one row index, including the end position. */
    bool seek(const std::vector<uint64_t>& position) override;

    /** @brief Supported extension without a dot. */
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "xml" }; }

    /** @brief The parser loads the complete XML document on open(). */
    [[nodiscard]] bool supports_streaming() const override { return false; }

    /** @brief Suggested size for bounded Composite copies. */
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 1024; }

    /** @brief The document is exposed as one row axis. */
    [[nodiscard]] size_t get_num_dimensions() const override { return 1; }

    /** @brief Total number of rows while open. */
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override;

private:
    std::optional<Kakshya::CompositeArray> m_rows;
    std::optional<Kakshya::ForestIndex> m_forest;
    std::string m_filepath;
    uint64_t m_position {};
};

}
