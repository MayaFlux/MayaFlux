#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

namespace MayaFlux::IO::Detail {

/** @brief Decoded header of one .npy array. */
struct NpyHeader {
    Kakshya::TensorSpec spec;
    bool big_endian {};
    size_t data_offset {};
};

/**
 * @brief Total prefix size (magic, version, length field and header) of a .npy array.
 * @param leading At least the first twelve bytes, or the whole array if shorter.
 * @param why Receives the reason on failure.
 * @return Offset at which the array data starts, or std::nullopt.
 */
[[nodiscard]] std::optional<size_t> npy_prefix_size(std::span<const std::byte> leading, std::string& why);

/**
 * @brief Decode a .npy prefix into a tensor spec.
 *
 * Fortran-ordered arrays become strided specs, so no data is rearranged.
 *
 * @param prefix Exactly the bytes npy_prefix_size() reported.
 * @param name Name given to the tensor.
 * @param why Receives the reason on failure.
 */
[[nodiscard]] std::optional<NpyHeader> parse_npy_header(
    std::span<const std::byte> prefix, std::string_view name, std::string& why);

/**
 * @brief Encode a tensor as a complete .npy array in dense C order.
 * @param why Receives the reason when numpy cannot represent the tensor.
 */
[[nodiscard]] std::optional<std::vector<std::byte>> encode_npy(
    const Kakshya::TensorData& tensor, std::string& why);

}
