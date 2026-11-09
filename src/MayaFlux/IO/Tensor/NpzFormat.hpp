#pragma once

#include "TensorReader.hpp"
#include "TensorWriter.hpp"

namespace MayaFlux::IO {

namespace Detail {
    struct NpzArchive;
}

/**
 * @class NpzReader
 * @brief Reader for NumPy .npz archives: a zip file of .npy arrays.
 *
 * Each archive entry ending in .npy is one tensor named after the entry.
 * open() reads only each entry's header, so describe() is cheap even for a
 * compressed archive; read() decompresses a single entry. Entries numpy
 * cannot describe to this reader are skipped with a warning.
 *
 * Usage:
 * @code
 * NpzReader reader;
 * reader.open("dataset.npz");
 * auto labels = reader.read("labels");
 * @endcode
 */
class MAYAFLUX_API NpzReader : public TensorReader {
public:
    NpzReader();
    NpzReader(const NpzReader&) = delete;
    NpzReader& operator=(const NpzReader&) = delete;
    ~NpzReader() override;

    /** @brief Register the format, with its writer, in the TensorFormatRegistry. */
    static void register_with_registry();

    [[nodiscard]] bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override { return m_open; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "npz" }; }

    [[nodiscard]] std::string format_name() const override { return "npz"; }
    [[nodiscard]] std::vector<Kakshya::TensorSpec> describe() const override;
    [[nodiscard]] std::optional<Kakshya::TensorData> read(std::string_view name) override;

private:
    struct Entry {
        uint32_t index {};
        Kakshya::TensorSpec spec;
        size_t data_offset {};
        bool big_endian {};
    };

    std::unique_ptr<Detail::NpzArchive> m_archive;
    std::vector<Entry> m_entries;
    bool m_open {};
};

/**
 * @class NpzWriter
 * @brief Writer for NumPy .npz archives, one stored (uncompressed) .npy entry per tensor.
 *
 * Entry names are the tensor names. A tensor numpy has no element type for is
 * refused, and so is the whole set.
 */
class MAYAFLUX_API NpzWriter : public TensorWriter {
public:
    bool write(const std::string& filepath, const Kakshya::TensorSet& tensors) override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "npz" }; }

private:
    std::string m_last_error;
};

}
