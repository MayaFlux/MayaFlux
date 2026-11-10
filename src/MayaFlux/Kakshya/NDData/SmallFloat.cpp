#include "SmallFloat.hpp"

namespace MayaFlux::Kakshya {

float half_to_float(uint16_t bits) noexcept
{
    const uint32_t sign = static_cast<uint32_t>(bits & 0x8000U) << 16;
    const uint32_t exponent = (bits >> 10) & 0x1FU;
    const uint32_t mantissa = bits & 0x3FFU;

    uint32_t pattern {};
    if (exponent == 0) {
        if (mantissa == 0) {
            pattern = sign;
        } else {
            uint32_t shifts = 0;
            uint32_t normalised = mantissa;
            while ((normalised & 0x400U) == 0) {
                normalised <<= 1;
                ++shifts;
            }
            normalised &= 0x3FFU;
            pattern = sign | ((113U - shifts) << 23) | (normalised << 13);
        }
    } else if (exponent == 0x1FU) {
        pattern = sign | 0x7F800000U | (mantissa << 13);
    } else {
        pattern = sign | ((exponent - 15U + 127U) << 23) | (mantissa << 13);
    }

    float value {};
    std::memcpy(&value, &pattern, sizeof(value));
    return value;
}

uint16_t float_to_half(float value) noexcept
{
    uint32_t pattern {};
    std::memcpy(&pattern, &value, sizeof(pattern));

    const uint32_t sign = (pattern >> 16) & 0x8000U;
    const auto field = static_cast<int32_t>((pattern >> 23) & 0xFFU);
    uint32_t mantissa = pattern & 0x7FFFFFU;

    if (field == 0xFF) {
        if (mantissa == 0)
            return static_cast<uint16_t>(sign | 0x7C00U);
        return static_cast<uint16_t>(sign | 0x7C00U | 0x200U | (mantissa >> 13));
    }

    const int32_t exponent = field - 127 + 15;
    if (exponent >= 0x1F)
        return static_cast<uint16_t>(sign | 0x7C00U);

    if (exponent <= 0) {
        if (exponent < -10)
            return static_cast<uint16_t>(sign);
        mantissa |= 0x800000U;
        const auto shift = static_cast<uint32_t>(14 - exponent);
        uint32_t half = mantissa >> shift;
        const uint32_t round_bit = 1U << (shift - 1U);
        if ((mantissa & round_bit) != 0U && ((mantissa & (round_bit - 1U)) != 0U || (half & 1U) != 0U))
            ++half;
        return static_cast<uint16_t>(sign | half);
    }

    uint32_t half = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    const uint32_t remainder = mantissa & 0x1FFFU;
    if (remainder > 0x1000U || (remainder == 0x1000U && (half & 1U) != 0U))
        ++half;
    return static_cast<uint16_t>(sign | half);
}

double decode_small_float(uint32_t bits, int exponent_bits, int mantissa_bits) noexcept
{
    const int bias = (1 << (exponent_bits - 1)) - 1;
    const uint32_t exponent_all = (1U << exponent_bits) - 1U;
    const bool negative = ((bits >> (exponent_bits + mantissa_bits)) & 1U) != 0U;
    const uint32_t exponent = (bits >> mantissa_bits) & exponent_all;
    const uint32_t mantissa = bits & ((1U << mantissa_bits) - 1U);

    double value {};
    if (exponent == exponent_all) {
        value = mantissa != 0U ? std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::infinity();
    } else if (exponent == 0U) {
        value = std::ldexp(static_cast<double>(mantissa), 1 - bias - mantissa_bits);
    } else {
        value = std::ldexp(static_cast<double>(mantissa | (1U << mantissa_bits)),
            static_cast<int>(exponent) - bias - mantissa_bits);
    }
    return negative ? -value : value;
}

uint32_t encode_small_float(double value, int exponent_bits, int mantissa_bits) noexcept
{
    const int bias = (1 << (exponent_bits - 1)) - 1;
    const uint32_t exponent_all = (1U << exponent_bits) - 1U;
    const uint32_t sign = std::signbit(value) ? (1U << (exponent_bits + mantissa_bits)) : 0U;

    if (std::isnan(value))
        return sign | (exponent_all << mantissa_bits) | (1U << (mantissa_bits - 1));

    value = std::fabs(value);
    if (std::isinf(value))
        return sign | (exponent_all << mantissa_bits);
    if (value == 0.0)
        return sign;

    int exponent = 0;
    std::frexp(value, &exponent);
    const int unbiased = exponent - 1;
    const int min_exponent = 1 - bias;

    if (unbiased < min_exponent) {
        const auto steps = static_cast<uint32_t>(std::nearbyint(std::ldexp(value, mantissa_bits - min_exponent)));
        return sign | steps;
    }

    auto steps = static_cast<uint32_t>(std::nearbyint(std::ldexp(value, mantissa_bits - unbiased)));
    int biased = unbiased + bias;
    if (steps == (2U << mantissa_bits)) {
        steps = 1U << mantissa_bits;
        ++biased;
    }
    if (std::cmp_greater_equal(biased, exponent_all))
        return sign | (exponent_all << mantissa_bits);

    return sign | (static_cast<uint32_t>(biased) << mantissa_bits) | (steps - (1U << mantissa_bits));
}

}
