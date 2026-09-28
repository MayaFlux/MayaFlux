#pragma once

#include "CompositeWriter.hpp"

#include <fstream>

namespace MayaFlux::IO {

/**
 * @class DelimitedTextWriter
 * @brief Streaming CSV and TSV writer for Composite arrays.
 *
 * The output delimiter defaults to the file extension. A header is
 * written by default, and every record ends with LF. Missing fields
 * become unquoted empty cells; present empty text becomes a quoted empty
 * cell so DelimitedTextReader can restore its presence bit. Numbers use
 * the shortest decimal form that parses back to the same value, and text
 * is quoted when CSV/TSV syntax requires it. A non-finite floating-point
 * value has no CSV form and fails the write.
 *
 * Usage:
 * @code
 * DelimitedTextWriter writer;
 * bool saved = writer.write("measurements.csv", array);
 * @endcode
 *
 * For large arrays, call open(), write_rows() with bounded slices,
 * and close(). Every slice must use the layout passed to open().
 */
class MAYAFLUX_API DelimitedTextWriter : public CompositeWriter {
public:
    DelimitedTextWriter() = default;
    ~DelimitedTextWriter() override;

    /**
     * @brief Select whether a header record is written.
     * @param enabled True by default.
     * @return False while a file is open.
     */
    bool set_write_header(bool enabled) noexcept;

    /**
     * @brief Override the extension-selected delimiter.
     * @param delimiter A nonzero character other than quote, CR, or LF.
     * @return False for an invalid delimiter or while a file is open.
     */
    bool set_delimiter(char delimiter) noexcept;

    /**
     * @brief Check for a .csv or .tsv output path.
     * @param filepath Candidate path.
     * @return True for a supported extension.
     */
    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    /**
     * @brief Create or truncate a CSV or TSV file.
     * @param filepath Output path; a relative path resolves through
     * resolve_write_path().
     * @param layout Field names and exact types for every written element.
     * @return True when ready to accept row slices.
     */
    bool open(
        const std::string& filepath,
        const Kakshya::CompositeLayout& layout) override;

    /**
     * @brief Encode a bounded slice using the open file's layout.
     * @param rows Elements to append; field names, types, and order must
     * match the layout passed to open().
     * @return True when every element was accepted. On failure, elements
     * before the failing one have already been written.
     */
    bool write_rows(const Kakshya::CompositeSlice& rows) override;

    /** @brief Flush the current file without closing it. */
    bool flush() override;

    /** @brief Finalize and close the current file. */
    bool close() override;

    /** @brief Whether an output file is open. */
    [[nodiscard]] bool is_open() const override { return m_file.is_open(); }

    /** @brief Supported extensions without dots. */
    [[nodiscard]] std::vector<std::string>
    get_supported_extensions() const override
    {
        return { "csv", "tsv" };
    }

    /** @brief Last encoding or file error, if any. */
    [[nodiscard]] std::string get_last_error() const override
    {
        return m_last_error;
    }

    /** @brief Number of data elements accepted since open(). */
    [[nodiscard]] size_t rows_written() const noexcept { return m_rows_written; }

private:
    std::ofstream m_file;
    std::optional<Kakshya::CompositeLayout> m_layout;
    std::string m_last_error;
    char m_delimiter {};
    char m_active_delimiter {};
    bool m_write_header { true };
    size_t m_rows_written {};

    [[nodiscard]] bool write_cell(std::string_view text, bool force_quote);
    [[nodiscard]] bool write_element(const Kakshya::Composite& element);
};

}
