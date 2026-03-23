#pragma once

#include <bit>

namespace MayaFlux::Transitive::Protocol {

//=============================================================================
// Padding
//=============================================================================

/**
 * @brief Round len up to the next 4-byte boundary.
 *
 * Used by OSC and any other protocol requiring 4-byte field alignment.
 * Example: padded_size(5) == 8, padded_size(4) == 4.
 */
[[nodiscard]] constexpr size_t padded_size(size_t len) noexcept
{
    return (len + 3) & ~size_t(3);
}

//=============================================================================
// Read primitives
//=============================================================================

/**
 * @brief Read a null-terminated string from a byte buffer.
 *
 * Advances offset by padded_size(len + 1). Safe: will not read past max_len.
 *
 * @param data    Buffer start.
 * @param max_len Total buffer size in bytes.
 * @param offset  Current read position, advanced on return.
 * @return Parsed string, empty if offset is already at or past max_len.
 */
[[nodiscard]] inline std::string read_string(
    const uint8_t* data, size_t max_len, size_t& offset)
{
    if (offset >= max_len) {
        return {};
    }
    const auto* start = reinterpret_cast<const char*>(data + offset);
    size_t len = strnlen(start, max_len - offset);
    std::string result(start, len);
    offset += padded_size(len + 1);
    return result;
}

/**
 * @brief Read a big-endian int32 from a byte buffer.
 *
 * Advances offset by 4. Caller must ensure offset + 4 <= buffer size.
 *
 * @param data   Buffer start.
 * @param offset Current read position, advanced by 4 on return.
 */
[[nodiscard]] inline int32_t read_int32(const uint8_t* data, size_t& offset)
{
    int32_t val {};
    std::memcpy(&val, data + offset, 4);
    offset += 4;
    if constexpr (std::endian::native == std::endian::little) {
        val = static_cast<int32_t>(std::byteswap(static_cast<uint32_t>(val)));
    }
    return val;
}

/**
 * @brief Read a big-endian IEEE 754 float from a byte buffer.
 *
 * Advances offset by 4. Caller must ensure offset + 4 <= buffer size.
 *
 * @param data   Buffer start.
 * @param offset Current read position, advanced by 4 on return.
 */
[[nodiscard]] inline float read_float(const uint8_t* data, size_t& offset)
{
    uint32_t raw {};
    std::memcpy(&raw, data + offset, 4);
    offset += 4;
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    float val {};
    std::memcpy(&val, &raw, 4);
    return val;
}

/**
 * @brief Read a length-prefixed blob from a byte buffer.
 *
 * Format: [int32 size (big-endian)][data, padded to 4-byte boundary].
 * Advances offset past the blob. Returns empty vector on invalid size.
 *
 * @param data    Buffer start.
 * @param max_len Total buffer size in bytes.
 * @param offset  Current read position, advanced on return.
 */
[[nodiscard]] inline std::vector<uint8_t> read_blob(
    const uint8_t* data, size_t max_len, size_t& offset)
{
    int32_t blob_size = read_int32(data, offset);
    if (blob_size < 0
        || offset + static_cast<size_t>(blob_size) > max_len) {
        return {};
    }
    std::vector<uint8_t> result(
        data + offset, data + offset + blob_size);
    offset += padded_size(static_cast<size_t>(blob_size));
    return result;
}

//=============================================================================
// Write primitives
//=============================================================================

/**
 * @brief Append a null-terminated string padded to 4-byte boundary.
 *
 * Writes the string bytes, a null terminator, then zero-padding.
 *
 * @param out Destination buffer.
 * @param str String to write.
 */
inline void write_string(std::vector<uint8_t>& out, const std::string& str)
{
    out.insert(out.end(), str.begin(), str.end());
    size_t padded = padded_size(str.size() + 1);
    out.resize(out.size() + (padded - str.size()), 0);
}

/**
 * @brief Append a big-endian int32.
 *
 * @param out Destination buffer.
 * @param val Value to write.
 */
inline void write_int32(std::vector<uint8_t>& out, int32_t val)
{
    auto raw = static_cast<uint32_t>(val);
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(&raw);
    out.insert(out.end(), bytes, bytes + 4);
}

/**
 * @brief Append a big-endian IEEE 754 float.
 *
 * @param out Destination buffer.
 * @param val Value to write.
 */
inline void write_float(std::vector<uint8_t>& out, float val)
{
    uint32_t raw {};
    std::memcpy(&raw, &val, 4);
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(&raw);
    out.insert(out.end(), bytes, bytes + 4);
}

/**
 * @brief Append a length-prefixed blob padded to 4-byte boundary.
 *
 * Format: [int32 size (big-endian)][data, zero-padded to 4-byte boundary].
 *
 * @param out  Destination buffer.
 * @param blob Bytes to write.
 */
inline void write_blob(
    std::vector<uint8_t>& out, const std::vector<uint8_t>& blob)
{
    write_int32(out, static_cast<int32_t>(blob.size()));
    out.insert(out.end(), blob.begin(), blob.end());
    size_t padded = padded_size(blob.size());
    out.resize(out.size() + (padded - blob.size()), 0);
}

} // namespace MayaFlux::Transitive::Protocol
