#pragma once

#include "FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/Composite.hpp"

namespace MayaFlux::Kakshya {
class CompositeContainer;
}

namespace MayaFlux::IO {

/**
 * @class CompositeReader
 * @brief Typed FileReader boundary for schema-bearing Composite data.
 *
 * Concrete readers return CompositeArray values without flattening their
 * layout or text storage into unrelated DataVariant channels. The inherited
 * DataVariant and SignalSourceContainer result methods are unsupported.
 * get_layout() exposes the schema a concrete reader established on open(),
 * so a caller can inspect field names and types before reading any data.
 *
 * Usage:
 * @code
 * std::unique_ptr<CompositeReader> reader =
 *     std::make_unique<DelimitedTextReader>();
 * if (reader->open("measurements.csv")) {
 *     auto first = reader->read_next(1024);
 *     auto remaining = reader->read_composite();
 * }
 * @endcode
 *
 * @note read_composite() reads from the current position. Call seek({0})
 * first to read the complete file after earlier bounded reads.
 */
class MAYAFLUX_API CompositeReader : public FileReader {
public:
    ~CompositeReader() override = default;

    /**
     * @brief Read all remaining elements into one owning array.
     * @return Array, including an empty array at end of file, or nullopt
     * on failure.
     */
    [[nodiscard]] virtual std::optional<Kakshya::CompositeArray>
    read_composite() = 0;

    /**
     * @brief Read at most max_elements from the current file position.
     * @param max_elements Maximum number of elements to read.
     * @return Owning array, including an empty array at end of file, or
     * nullopt on failure.
     */
    [[nodiscard]] virtual std::optional<Kakshya::CompositeArray>
    read_next(size_t max_elements) = 0;

    /**
     * @brief Inspect the schema established by open() without reading data.
     * @return Copy of the active layout, or std::nullopt when no file is
     * currently open or open() did not succeed in establishing one.
     * @note Returned by value: the copy remains valid after the reader is
     * closed or destroyed, unlike a borrowed CompositeArray::layout().
     */
    [[nodiscard]] virtual std::optional<Kakshya::CompositeLayout>
    get_layout() const = 0;

    /**
     * @brief Read an inclusive one-dimensional file region.
     * @param region Element bounds in start_coordinates[0] and
     * end_coordinates[0].
     * @return Owning array, or nullopt for invalid bounds or a short read.
     * @note Moves the reader position to the end of the selected region.
     */
    [[nodiscard]] std::optional<Kakshya::CompositeArray>
    read_composite_region(const FileRegion& region);

    /**
     * @brief Read remaining elements into a new CompositeContainer.
     * @param batch_size Default materialization batch size.
     * @return Owning container, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::CompositeContainer>
    create_composite_container(size_t batch_size = 1);

    /** @brief Composite data is not a DataVariant channel vector. */
    std::vector<Kakshya::DataVariant> read_all() override;
    /** @brief Use read_composite_region() for a schema-bearing result. */
    std::vector<Kakshya::DataVariant> read_region(const FileRegion&) override;
    /** @brief CompositeContainer is not a SignalSourceContainer. */
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;
    /** @brief CompositeContainer is not a SignalSourceContainer. */
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer>) override;

    [[nodiscard]] std::type_index get_data_type() const override;
    [[nodiscard]] std::type_index get_container_type() const override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

protected:
    std::string m_last_error;
};

}
