#pragma once

namespace MayaFlux::Kakshya {

/**
 * @brief IEEE 754 binary16 to binary32. Exact for every pattern; a NaN keeps its payload.
 */
[[nodiscard]] MAYAFLUX_API float half_to_float(uint16_t bits) noexcept;

/**
 * @brief IEEE 754 binary32 to binary16, rounding to nearest with ties to even.
 *
 * Values past the largest half become infinity, values below the smallest
 * subnormal become a signed zero, and a NaN stays a NaN.
 */
[[nodiscard]] MAYAFLUX_API uint16_t float_to_half(float value) noexcept;

/**
 * @brief Decode a binary floating point pattern with the given field widths.
 *
 * The pattern is sign, exponent and mantissa from the top, with an IEEE style
 * bias and an all-ones exponent for infinity and NaN. Half is 5 and 10,
 * bfloat16 is 8 and 7.
 */
[[nodiscard]] MAYAFLUX_API double decode_small_float(
    uint32_t bits, int exponent_bits, int mantissa_bits) noexcept;

/**
 * @brief Encode a double as a binary floating point pattern with the given field widths.
 *
 * Rounds once, to nearest with ties to even, straight from the double, so a
 * float widened to double is never rounded twice. Overflow becomes infinity
 * and NaN becomes a quiet NaN.
 */
[[nodiscard]] MAYAFLUX_API uint32_t encode_small_float(
    double value, int exponent_bits, int mantissa_bits) noexcept;

}
