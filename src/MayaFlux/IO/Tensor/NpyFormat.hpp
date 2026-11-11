#pragma once

#include "TensorReader.hpp"
#include "TensorWriter.hpp"

#include <fstream>

namespace MayaFlux::IO {

/**
 * @class NpyReader
 * @brief Reader for a single NumPy .npy array, format versions 1 to 3.
 *
 * The array becomes one tensor named after the file stem. Fortran-ordered
 * files keep their data in place and are described with strides. Either byte
 * order is accepted and converted to the host's. Object, string, datetime,
 * structured and extended-precision types are refused with a reason.
 *
 * Usage:
 * @code
 * NpyReader reader;
 * reader.open("frames.npy");
 * auto frames = reader.read("frames");
 * @endcode
 */
class MAYAFLUX_API NpyReader : public TensorReader {
public:
    NpyReader() = default;
    NpyReader(const NpyReader&) = delete;
    NpyReader& operator=(const NpyReader&) = delete;
    ~NpyReader() override { close(); }

    /** @brief Register the format, with its writer, in the TensorFormatRegistry. */
    static void register_with_registry();

    /** @brief Whether the leading bytes are the numpy magic string. */
    [[nodiscard]] static bool probe(std::span<const std::byte> head, uint64_t file_size);

    [[nodiscard]] bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override { return m_open; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "npy" }; }

    [[nodiscard]] std::string format_name() const override { return "npy"; }
    [[nodiscard]] std::vector<Kakshya::TensorSpec> describe() const override;
    [[nodiscard]] std::optional<Kakshya::TensorData> read(std::string_view name) override;

private:
    std::ifstream m_file;
    Kakshya::TensorSpec m_spec;
    size_t m_data_offset {};
    bool m_big_endian {};
    bool m_open {};
};

/**
 * @class NpyWriter
 * @brief Writer for a single .npy array in dense C order.
 *
 * Takes a set holding exactly one tensor. Element types numpy has no name for,
 * such as bfloat16 and float8, are refused.
 */
class MAYAFLUX_API NpyWriter : public TensorWriter {
public:
    bool write(const std::string& filepath, const Kakshya::TensorSet& tensors) override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "npy" }; }

private:
    std::string m_last_error;
};

}
