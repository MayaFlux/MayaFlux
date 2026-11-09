#pragma once

#include "TensorReader.hpp"
#include "TensorWriter.hpp"

#include <fstream>

namespace MayaFlux::IO {

/**
 * @class SafetensorsReader
 * @brief Reader for the safetensors format: a JSON index followed by packed tensor bytes.
 *
 * open() parses the index only. Each tensor is then read with one seek, at its
 * native element type, including bfloat16, float8 and sub-byte types. An index
 * entry with an element type this build does not know is skipped with a
 * warning; every other tensor stays readable.
 *
 * Usage:
 * @code
 * SafetensorsReader reader;
 * reader.open("model.safetensors");
 * auto bias = reader.read("layer.bias");
 * @endcode
 */
class MAYAFLUX_API SafetensorsReader : public TensorReader {
public:
    SafetensorsReader() = default;
    ~SafetensorsReader() override { close(); }

    /** @brief Register the format, with its writer, in the TensorFormatRegistry. */
    static void register_with_registry();

    /** @brief Whether leading bytes and file size look like a safetensors header. */
    [[nodiscard]] static bool probe(std::span<const std::byte> head, uint64_t file_size);

    [[nodiscard]] bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override { return m_open; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "safetensors" }; }

    [[nodiscard]] std::string format_name() const override { return "safetensors"; }
    [[nodiscard]] std::vector<Kakshya::TensorSpec> describe() const override;
    [[nodiscard]] std::map<std::string, std::string> tensor_metadata() const override { return m_metadata; }
    [[nodiscard]] std::optional<Kakshya::TensorData> read(std::string_view name) override;

private:
    struct Entry {
        Kakshya::TensorSpec spec;
        uint64_t begin {};
        uint64_t end {};
    };

    bool reject(std::string reason);

    std::ifstream m_file;
    std::vector<Entry> m_entries;
    std::map<std::string, std::string> m_metadata;
    uint64_t m_data_start {};
    bool m_open {};
};

/**
 * @class SafetensorsWriter
 * @brief Writer for the safetensors format.
 *
 * Orders tensors wider elements first, then by name, so every tensor stays
 * aligned to its element size. Pads the index to eight bytes and stores the
 * set's string metadata. Element types safetensors has no name for are refused.
 */
class MAYAFLUX_API SafetensorsWriter : public TensorWriter {
public:
    bool write(const std::string& filepath, const Kakshya::TensorSet& tensors) override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "safetensors" }; }

private:
    std::string m_last_error;
};

}
