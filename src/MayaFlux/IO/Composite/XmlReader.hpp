#pragma once

#include "CompositeReader.hpp"

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
    std::string m_filepath;
    uint64_t m_position {};
};

}
