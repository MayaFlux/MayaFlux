#pragma once

/**
 * @file OrderKey.hpp
 * @brief Doubles as 64-bit keys whose unsigned order is their numeric order.
 *
 * A key is stored as two 32-bit words, low word first, which is the byte
 * layout of a little-endian uint64_t and of a uvec2 (x low, y high). Comparing
 * high words and then low words as unsigned integers orders doubles exactly,
 * with no fp64 arithmetic. -0.0 and +0.0 share one key and every NaN maps to
 * the largest key, so NaN sorts last.
 */

namespace MayaFlux::Kinesis::Discrete {

namespace detail {
    inline constexpr uint64_t order_sign_bit = uint64_t { 1 } << 63U;
    inline constexpr uint64_t order_nan_key = ~uint64_t { 0 };
}

/**
 * @brief The order key of a value.
 */
[[nodiscard]] constexpr uint64_t order_key(double value) noexcept
{
    if (value != value) {
        return detail::order_nan_key;
    }
    const auto bits = std::bit_cast<uint64_t>(value + 0.0);
    return (bits & detail::order_sign_bit) != 0U ? ~bits : bits | detail::order_sign_bit;
}

/**
 * @brief The value of an order key. The key of any NaN reads back as a quiet NaN.
 */
[[nodiscard]] constexpr double from_order_key(uint64_t key) noexcept
{
    if (key == detail::order_nan_key) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const uint64_t bits = (key & detail::order_sign_bit) != 0U ? key & ~detail::order_sign_bit : ~key;
    return std::bit_cast<double>(bits);
}

/**
 * @brief Writes the order key of every value, stopping at the shorter span.
 */
inline void order_keys(std::span<const double> values, std::span<uint64_t> keys) noexcept
{
    const size_t count = std::min(values.size(), keys.size());
    for (size_t i = 0; i < count; ++i) {
        keys[i] = order_key(values[i]);
    }
}

} // namespace MayaFlux::Kinesis::Discrete
