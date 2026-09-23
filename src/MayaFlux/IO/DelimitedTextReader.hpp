#pragma once

#include "CompositeReader.hpp"

#include <fstream>

namespace MayaFlux::IO {

/**
 * @class DelimitedTextReader
 * @brief Incremental CSV and TSV reader producing CompositeArray values.
 *
 * A header row supplies field names by default. Without a supplied layout,
 * every field is stored as UTF-8 text, preserving values without speculative
 * numeric inference. Supply a CompositeLayout before open() to select exact
 * arithmetic field types; header names must then match its field order.
 * Empty numeric cells and missing trailing cells remain absent. An
 * unquoted empty text cell is absent; a quoted empty cell is present.
 * char, signed char, and unsigned char fields parse as small integers via
 * from_chars, the same path as every other arithmetic type; a cell must
 * contain a number such as "65", not a literal character such as "A".
 *
 * Usage:
 * @code
 * CompositeLayout layout;
 * layout.add_field<std::string>("name");
 * layout.add_field<double>("score");
 * DelimitedTextReader reader;
 *
 * reader.set_layout(std::move(layout));
 * if (reader.open("scores.csv")) {
 *     while (auto batch = reader.read_next(512)) {
 *         if (batch->empty())
 *             break;
 *     }
 * }
 * @endcode
 *
 * CSV and TSV share the same quoting rules. The delimiter defaults to the
 * file extension and can be overridden before open().
 */
class MAYAFLUX_API DelimitedTextReader : public CompositeReader {
public:
    DelimitedTextReader() = default;
    ~DelimitedTextReader() override;

    /**
     * @brief Set an exact field layout before opening a file.
     * @param layout Named arithmetic or text fields in file order.
     * @return False when a file is already open.
     */
    bool set_layout(Kakshya::CompositeLayout layout);

    /**
     * @brief Select whether the first record supplies field names.
     * @param enabled True by default; a layout is required when false.
     * @return False when a file is already open.
     */
    bool set_has_header(bool enabled) noexcept;

    /**
     * @brief Override the delimiter chosen from .csv or .tsv.
     * @param delimiter A nonzero character other than quote, CR, or LF.
     * @return False for an invalid delimiter or an open file.
     */
    bool set_delimiter(char delimiter) noexcept;

    /**
     * @brief Check for the .csv or .tsv extension.
     * @param filepath Candidate file path.
     */
    [[nodiscard]] bool can_read(const std::string& filepath) const override;

    /**
     * @brief Open a CSV or TSV file and establish its Composite layout.
     * @param filepath Source path.
     * @param options FileReader options.
     * @return True when the file and header are valid.
     */
    bool open(const std::string& filepath,
        FileReadOptions options = FileReadOptions::ALL) override;

    /** @brief Close the input and release its active layout. */
    void close() override;

    /** @brief Whether a file is currently open. */
    [[nodiscard]] bool is_open() const override { return m_file.is_open(); }

    /** @brief File metadata and ordered field names, when open. */
    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;

    /** @brief Delimited text does not declare semantic file regions. */
    [[nodiscard]] std::vector<FileRegion> get_regions() const override { return {}; }

    /**
     * @brief Consume all remaining records into one array.
     * @return Owning array or nullopt on parse or conversion failure.
     */
    [[nodiscard]] std::optional<Kakshya::CompositeArray>
    read_composite() override;

    /**
     * @brief Consume a bounded number of records.
     * @param max_elements Maximum records to return; zero returns an empty
     * array without advancing.
     * @return Owning array, empty at end of file, or nullopt on failure.
     */
    [[nodiscard]] std::optional<Kakshya::CompositeArray>
    read_next(size_t max_elements) override;

    /** @brief Copy the layout established by open(), if available. */
    [[nodiscard]] std::optional<Kakshya::CompositeLayout>
    get_layout() const override
    {
        return m_layout;
    }

    /** @brief Zero-based index of the next data record. */
    [[nodiscard]] std::vector<uint64_t> get_read_position() const override
    {
        return { m_row_position };
    }

    /**
     * @brief Seek to a zero-based data record by replaying from the first.
     * @param position One record index.
     * @return False for an invalid or out-of-range position.
     * @note On an out-of-range position, the reader is left at the end of
     * file (get_read_position() reflects how far the replay reached, and
     * subsequent read_next() calls return an empty array), not at an
     * undefined position.
     */
    bool seek(const std::vector<uint64_t>& position) override;

    /** @brief Supported file extensions without dots. */
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override
    {
        return { "csv", "tsv" };
    }

    /** @brief Bounded sequential reads are supported. */
    [[nodiscard]] bool supports_streaming() const override { return true; }

    /** @brief Suggested number of records per bounded read. */
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 1024; }

    /** @brief Delimited data has one element axis. */
    [[nodiscard]] size_t get_num_dimensions() const override { return 1; }

    /** @brief Total element count is not known without scanning the file. */
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override { return {}; }

private:
    struct ParsedField {
        std::string value;
        bool quoted {};
    };

    std::ifstream m_file;
    std::string m_filepath;
    std::optional<Kakshya::CompositeLayout> m_requested_layout;
    std::optional<Kakshya::CompositeLayout> m_layout;

    std::streampos m_data_start {};
    uint64_t m_row_position {};

    char m_delimiter {};
    char m_active_delimiter {};
    bool m_has_header { true };
    bool m_at_end {};

    [[nodiscard]] bool read_record(
        std::vector<ParsedField>& fields, bool& at_end);

    [[nodiscard]] bool append_record(
        Kakshya::CompositeArray& array,
        const std::vector<ParsedField>& fields);
};

}
