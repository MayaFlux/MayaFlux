#pragma once

#include "MayaFlux/Kakshya/NDData/CompositeAccess.hpp"

namespace MayaFlux::IO {

/**
 * @class CompositeWriter
 * @brief Typed writer contract for schema-bearing Composite files.
 *
 * A concrete writer chooses a file format and encodes Composite fields
 * without exposing unrestricted byte writes. A file is opened with one
 * layout, then receives one or more bounded slices before close().
 *
 * Usage:
 * @code
 * bool save_measurements(
 *     CompositeWriter& writer, const Kakshya::CompositeArray& array)
 * {
 *     return writer.write("measurements.csv", array);
 * }
 * @endcode
 *
 * @note The source array must remain alive throughout each write_rows()
 * call. The writer does not retain the borrowed slice.
 */
class MAYAFLUX_API CompositeWriter {
public:
    virtual ~CompositeWriter() = default;

    /**
     * @brief Check whether this writer supports the target path.
     * @param filepath Output path.
     * @return True for a supported file format.
     */
    [[nodiscard]] virtual bool can_write(
        const std::string& filepath) const = 0;

    /**
     * @brief Open a new file with the schema all subsequent slices must use.
     * @param filepath Output path.
     * @param layout Finalized or declarative Composite field layout.
     * @return True when the file is ready to receive elements.
     */
    virtual bool open(
        const std::string& filepath,
        const Kakshya::CompositeLayout& layout) = 0;

    /**
     * @brief Write a bounded slice of elements.
     * @param rows Borrowed elements whose field names, types, and order
     * must match the layout passed to open().
     * @return True if every element was accepted by the writer. On failure,
     * elements before the failing one may already have been written.
     */
    virtual bool write_rows(const Kakshya::CompositeSlice& rows) = 0;

    /**
     * @brief Flush buffered output without closing the file.
     * @return True when all buffered output has been written.
     */
    virtual bool flush() = 0;

    /**
     * @brief Finish the file and release its output stream.
     * @return True when the file was finalized successfully.
     */
    virtual bool close() = 0;

    /**
     * @brief Check whether a file is currently open for writing.
     * @return True when write_rows() may be called.
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Get supported file extensions without dots.
     * @return Extensions handled by the concrete writer.
     */
    [[nodiscard]] virtual std::vector<std::string>
    get_supported_extensions() const = 0;

    /**
     * @brief Get the last writer error.
     * @return Error text, or an empty string if there is no error.
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;

    /**
     * @brief Write a complete array in one call.
     *
     * Opens with the array's layout, writes all its elements, then closes
     * the file. A failed write still closes the file before returning, and
     * a partial file may remain.
     *
     * @param filepath Output path.
     * @param array Source array.
     * @return True only when open, write, and close all succeed.
     */
    bool write(
        const std::string& filepath,
        const Kakshya::CompositeArray& array);
};

}
