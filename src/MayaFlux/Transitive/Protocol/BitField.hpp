#pragma once

namespace MayaFlux::Transitive::Protocol {

//=============================================================================
// Bit fields
//=============================================================================

/**
 * @brief Read an unsigned bit field, least significant bit first.
 *
 * Bit zero is the lowest bit of byte zero, matching HID report layout.
 * Bits that fall past the end of the buffer read as zero. A width above
 * 64 is clamped to 64.
 *
 * @param bytes      Source buffer.
 * @param bit_offset Position of the field's lowest bit.
 * @param bit_size   Field width in bits.
 */
[[nodiscard]] constexpr uint64_t load_bits_le(
    std::span<const uint8_t> bytes, size_t bit_offset, size_t bit_size) noexcept
{
    const size_t width = bit_size < 64 ? bit_size : 64;
    uint64_t raw = 0;

    for (size_t i = 0; i < width; ++i) {
        const size_t bit = bit_offset + i;
        const size_t byte = bit / 8;
        if (byte >= bytes.size()) {
            break;
        }
        raw |= static_cast<uint64_t>((bytes[byte] >> (bit % 8)) & 1U) << i;
    }

    return raw;
}

/**
 * @brief Interpret the low bits of a value as two's complement.
 *
 * Bits above the width are ignored. A width of zero yields zero and a
 * width of 64 or more returns the value reinterpreted unchanged.
 *
 * @param raw  Value holding the field in its low bits.
 * @param bits Field width in bits.
 */
[[nodiscard]] constexpr int64_t sign_extend(uint64_t raw, size_t bits) noexcept
{
    if (bits == 0) {
        return 0;
    }
    if (bits >= 64) {
        return static_cast<int64_t>(raw);
    }

    const uint64_t sign = uint64_t { 1 } << (bits - 1);
    raw &= (sign << 1) - 1;
    return static_cast<int64_t>((raw ^ sign) - sign);
}

/**
 * @brief Read a two's complement bit field, least significant bit first.
 *
 * Same layout rules as load_bits_le, with the result sign extended from
 * the field width.
 *
 * @param bytes      Source buffer.
 * @param bit_offset Position of the field's lowest bit.
 * @param bit_size   Field width in bits.
 */
[[nodiscard]] constexpr int64_t load_signed_bits_le(
    std::span<const uint8_t> bytes, size_t bit_offset, size_t bit_size) noexcept
{
    return sign_extend(load_bits_le(bytes, bit_offset, bit_size), bit_size);
}

} // namespace MayaFlux::Transitive::Protocol
