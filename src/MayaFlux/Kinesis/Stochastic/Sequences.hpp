#pragma once

#include <glm/glm.hpp>

/**
 * Values that depend only on their index and seed, so any number of threads can
 * draw them in any order.
 */
namespace MayaFlux::Kinesis::Stochastic {

/** @brief 32 bit permutation hash (PCG output function). */
[[nodiscard]] constexpr uint32_t pcg_hash(uint32_t value) noexcept
{
    const uint32_t state = value * 747796405U + 2891336453U;
    const uint32_t word = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U;
    return (word >> 22U) ^ word;
}

/** @brief The top 24 bits of a word as a value in [0, 1). */
[[nodiscard]] constexpr float unit_from_bits(uint32_t bits) noexcept
{
    return static_cast<float>(bits >> 8U) * 0x1.0p-24F;
}

/** @brief Uniform value in [0, 1) for an index under a seed. */
[[nodiscard]] constexpr float hashed_unit(uint32_t index, uint32_t seed = 0U) noexcept
{
    return unit_from_bits(pcg_hash(index + pcg_hash(seed)));
}

/**
 * @brief Base two radical inverse of an index, in [0, 1).
 * @param scramble Exclusive or applied to the result bits; zero leaves the sequence as it is.
 */
[[nodiscard]] constexpr float radical_inverse(uint32_t index, uint32_t scramble = 0U) noexcept
{
    uint32_t bits = (index << 16U) | (index >> 16U);
    bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
    bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
    bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
    bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
    return unit_from_bits(bits ^ scramble);
}

/** @brief Point @p index of a Hammersley set of @p count points in the unit square. */
[[nodiscard]] inline glm::vec2 hammersley_point(uint32_t index, uint32_t count, uint32_t scramble = 0U) noexcept
{
    return { (static_cast<float>(index) + 0.5F) / static_cast<float>(count), radical_inverse(index, scramble) };
}

/**
 * @brief Point @p index of the R2 additive recurrence in the unit square.
 *
 * Needs no point count, so a set can grow without moving the points it has.
 */
[[nodiscard]] inline glm::vec2 additive_recurrence(uint32_t index) noexcept
{
    const double x = 0.5 + 0.7548776662466927 * static_cast<double>(index);
    const double y = 0.5 + 0.5698402909980532 * static_cast<double>(index);
    return { std::min(static_cast<float>(x - std::floor(x)), 0x1.fffffep-1F),
        std::min(static_cast<float>(y - std::floor(y)), 0x1.fffffep-1F) };
}

} // namespace MayaFlux::Kinesis::Stochastic
