#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

namespace MayaFlux::IO::Detail {

/** @brief Whether this machine stores multi-byte values least significant byte first. */
[[nodiscard]] bool host_is_little_endian() noexcept;

/**
 * @brief Bytes of one independently ordered component of an element.
 *
 * A complex element is two components; every other element is one. Zero for
 * sub-byte types, which have no byte order.
 */
[[nodiscard]] size_t component_bytes(Kakshya::TensorDType dtype) noexcept;

/** @brief Reverse the byte order of every component in place. */
void swap_components(std::span<std::byte> data, size_t component_bytes) noexcept;

/** @brief Element type for a safetensors dtype name such as F32 or BF16. */
[[nodiscard]] std::optional<Kakshya::TensorDType> safetensors_dtype(std::string_view name) noexcept;

/** @brief Safetensors dtype name for an element type, when the format has one. */
[[nodiscard]] std::optional<std::string_view> safetensors_name(Kakshya::TensorDType dtype) noexcept;

/** @brief Element type and byte order read from a numpy type string. */
struct NpyTypestr {
    Kakshya::TensorDType dtype;
    bool big_endian {};
};

/**
 * @brief Parse a numpy type string such as <f4, >i8 or |b1.
 * @param why Receives the reason when the type is not supported.
 */
[[nodiscard]] std::optional<NpyTypestr> npy_dtype(std::string_view typestr, std::string& why);

/** @brief Little-endian numpy type string for an element type, when numpy has one. */
[[nodiscard]] std::optional<std::string> npy_typestr(Kakshya::TensorDType dtype);

/** @brief First bytes of a file together with its total size. */
struct FileHead {
    std::vector<std::byte> bytes;
    uint64_t file_size {};
};

/** @brief Read up to @p limit leading bytes of a file, or std::nullopt if it cannot be opened. */
[[nodiscard]] std::optional<FileHead> read_file_head(const std::string& path, size_t limit);

/** @brief Lowercase extension of a path without its dot. */
[[nodiscard]] std::string lower_extension(const std::string& path);

/**
 * @brief Elements of a tensor as dense C-order bytes.
 * @param tensor Source tensor.
 * @param dense Receives a gathered copy only when the tensor is strided; the
 * returned span then points into it, so keep it alive while the span is used.
 * @return The bytes, or std::nullopt when the tensor is invalid.
 */
[[nodiscard]] std::optional<std::span<const std::byte>> dense_bytes(
    const Kakshya::TensorData& tensor, std::optional<Kakshya::TensorData>& dense);

}
