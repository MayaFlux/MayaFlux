#pragma once

/**
 * @file Word.hpp
 * @brief Samples read as fixed width binary words.
 *
 * A value in [-1, 1] maps to the offset binary word round((x + 1) / 2 * (2^bits - 1)):
 * -1 is all zeros, +1 is all ones, 0 is half scale. Complementing every bit
 * negates the value, and bit width is limited to 24 so a word fits an element of a
 * coefficient array exactly.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief The offset binary word of a value, clamped to [-1, 1].
 * @param bits Word width, clamped to 1 to 24
 */
[[nodiscard]] MAYAFLUX_API uint32_t to_word(double value, uint32_t bits) noexcept;

/**
 * @brief The value of an offset binary word; bits above the width are ignored.
 * @param bits Word width, clamped to 1 to 24
 */
[[nodiscard]] MAYAFLUX_API double from_word(uint32_t word, uint32_t bits) noexcept;

} // namespace MayaFlux::Kinesis::Discrete
