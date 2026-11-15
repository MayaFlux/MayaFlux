#pragma once

#include "CompositeWriter.hpp"
#include "XmlReader.hpp"

namespace MayaFlux::IO {

/**
 * @class XmlWriter
 * @brief Write preorder XML Composite rows as one document.
 *
 * Rows use xml_composite_layout(). Parent indices refer to the complete row
 * sequence across write_rows() calls. A parent must be an open ancestor;
 * attributes must precede child content. The writer builds a DOM and saves
 * it on flush() or close(). It preserves supported node order and values,
 * but does not promise byte-identical output. Output is UTF-8, and a source
 * declaration's encoding attribute is updated to UTF-8 when present.
 *
 * Usage:
 * @code
 * XmlWriter writer;
 * bool saved = writer.write("controls.xml", nodes);
 * @endcode
 *
 * @note open() creates or truncates the output path. A failed write prevents
 * finalization. After a successful flush, later rows can still be appended
 * and the file is updated by the next flush or close.
 */
class MAYAFLUX_API XmlWriter : public CompositeWriter {
public:
    XmlWriter();
    ~XmlWriter() override;

    /** @brief Accept paths with the .xml extension. */
    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    /** @brief Start a document using xml_composite_layout(). */
    bool open(const std::string& filepath,
        const Kakshya::CompositeLayout& layout) override;

    /**
     * @brief Append preorder rows whose parent indices are global.
     * @return False on invalid structure or a field value that cannot be
     * represented as XML; earlier accepted rows remain in the DOM.
     */
    bool write_rows(const Kakshya::CompositeSlice& rows) override;

    /** @brief Save the current document, which must contain one root. */
    bool flush() override;

    /** @brief Save and release the document, or return false after failure. */
    bool close() override;

    /** @brief Whether a document is being assembled. */
    [[nodiscard]] bool is_open() const override { return m_open; }

    /** @brief Supported extension without a dot. */
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "xml" }; }

    /** @brief Last structure or output error. */
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

private:
    struct Impl;
    bool write_row(const Kakshya::Composite& row);

    std::unique_ptr<Impl> m_xml;
    std::vector<bool> m_content_started;
    std::vector<uint64_t> m_ancestors;
    std::string m_filepath;
    std::string m_last_error;
    bool m_open {};
    bool m_failed {};
    bool m_declaration_seen {};
    bool m_doctype_seen {};
    size_t m_root_count {};
};

}
