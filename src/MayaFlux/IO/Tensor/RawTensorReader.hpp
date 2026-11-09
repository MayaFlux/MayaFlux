#pragma once

#include "TensorReader.hpp"

#include <fstream>

namespace MayaFlux::IO {

/**
 * @struct RawTensorEntry
 * @brief Where one tensor sits in a headerless file, and what it is.
 */
struct RawTensorEntry {
    /** @brief Named layout of the tensor: dtype, shape, strides and offset. */
    Kakshya::TensorSpec spec;
    /** @brief Byte position of the tensor's storage; defaults to just after the previous entry. */
    std::optional<uint64_t> file_offset;
    /** @brief Whether the file stores multi-byte values most significant byte first. */
    bool big_endian {};
};

/**
 * @class RawTensorReader
 * @brief Reader for headerless binary files whose layout the caller supplies.
 *
 * Covers raw dumps, simple custom formats and any data whose layout lives in a
 * sidecar the caller has already parsed. It is chosen explicitly, never by
 * detection, because a raw file identifies nothing about itself.
 *
 * Usage:
 * @code
 * RawTensorReader reader;
 * reader.set_layout({ { TensorSpec::contiguous(TensorDType::of<float>(), { 480, 640 }, "depth"), 0 } });
 * reader.open("depth.bin");
 * auto depth = reader.read("depth");
 * @endcode
 */
class MAYAFLUX_API RawTensorReader : public TensorReader {
public:
    RawTensorReader() = default;
    RawTensorReader(const RawTensorReader&) = delete;
    RawTensorReader& operator=(const RawTensorReader&) = delete;
    ~RawTensorReader() override { close(); }

    /**
     * @brief Declare the tensors the file holds.
     * @return False, with get_last_error() set, for an invalid spec, a duplicate
     * or empty name, or device memory.
     */
    bool set_layout(std::vector<RawTensorEntry> entries);

    [[nodiscard]] bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override { return m_open; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return {}; }

    [[nodiscard]] std::string format_name() const override { return "raw"; }
    [[nodiscard]] std::vector<Kakshya::TensorSpec> describe() const override;
    [[nodiscard]] std::optional<Kakshya::TensorData> read(std::string_view name) override;

private:
    std::ifstream m_file;
    std::vector<RawTensorEntry> m_layout;
    std::vector<uint64_t> m_offsets;
    bool m_open {};
};

}
