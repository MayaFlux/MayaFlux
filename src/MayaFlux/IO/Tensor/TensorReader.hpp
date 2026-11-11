#pragma once

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

namespace MayaFlux::IO {

/**
 * @struct TensorReadOptions
 * @brief Selection for TensorReader::read_tensors().
 */
struct TensorReadOptions {
    /** @brief Tensors to read, in this order. Empty reads every tensor in file order. */
    std::vector<std::string> names;
};

/**
 * @class TensorReader
 * @brief Typed FileReader boundary for formats that store named tensors.
 *
 * A concrete reader parses only the file's index on open(), so describe() and
 * tensor_metadata() answer without reading tensor data. read() then loads one
 * tensor at its native element type, with byte order converted to the host's,
 * so a large file can be inspected and read selectively. The inherited
 * DataVariant and container methods are unsupported.
 *
 * A new format derives from this class, registers a TensorFormat with the
 * TensorFormatRegistry, and nothing else changes.
 *
 * Usage:
 * @code
 * auto reader = io.open_tensor_reader("model.safetensors");
 * for (const auto& spec : reader->describe()) { }
 * auto weight = reader->read("encoder.0.weight");
 * auto everything = reader->read_tensors();
 * @endcode
 */
class MAYAFLUX_API TensorReader : public FileReader {
public:
    ~TensorReader() override = default;

    /** @brief Format identifier such as safetensors or npy. */
    [[nodiscard]] virtual std::string format_name() const = 0;

    /** @brief Specs of every tensor in the open file, in file order, without reading data. */
    [[nodiscard]] virtual std::vector<Kakshya::TensorSpec> describe() const = 0;

    /** @brief Free-form string metadata stored in the file, empty if the format has none. */
    [[nodiscard]] virtual std::map<std::string, std::string> tensor_metadata() const { return {}; }

    /**
     * @brief Read one tensor by name.
     * @return The tensor, or std::nullopt with get_last_error() set.
     */
    [[nodiscard]] virtual std::optional<Kakshya::TensorData> read(std::string_view name) = 0;

    /**
     * @brief Read a selection of tensors together with the file's metadata.
     * @return The set, or std::nullopt if any requested tensor is missing or unreadable.
     */
    [[nodiscard]] std::optional<Kakshya::TensorSet> read_tensors(const TensorReadOptions& options = {});

    /** @brief Spec of one tensor by name, without reading data. */
    [[nodiscard]] std::optional<Kakshya::TensorSpec> find_spec(std::string_view name) const;

    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;
    [[nodiscard]] std::vector<FileRegion> get_regions() const override { return {}; }
    std::vector<Kakshya::DataVariant> read_all() override;
    std::vector<Kakshya::DataVariant> read_region(const FileRegion&) override;
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer>) override;
    [[nodiscard]] std::vector<uint64_t> get_read_position() const override { return {}; }
    bool seek(const std::vector<uint64_t>&) override { return false; }
    [[nodiscard]] std::type_index get_data_type() const override;
    [[nodiscard]] std::type_index get_container_type() const override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }
    [[nodiscard]] bool supports_streaming() const override { return false; }
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 0; }
    [[nodiscard]] size_t get_num_dimensions() const override { return 0; }
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override { return {}; }

protected:
    /** @brief Record a failure reason and return false. */
    bool fail(std::string reason)
    {
        m_last_error = std::move(reason);
        return false;
    }

    std::string m_last_error;
    std::string m_path;
};

}
