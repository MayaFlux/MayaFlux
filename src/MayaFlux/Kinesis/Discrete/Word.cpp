#include "Word.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr uint32_t max_bits = 24;

    [[nodiscard]] double top(uint32_t bits) noexcept
    {
        return static_cast<double>((uint32_t { 1 } << std::clamp(bits, uint32_t { 1 }, max_bits)) - 1U);
    }

}

uint32_t to_word(double value, uint32_t bits) noexcept
{
    const double clamped = std::clamp(value, -1.0, 1.0);
    return static_cast<uint32_t>(std::lrint((clamped + 1.0) * 0.5 * top(bits)));
}

double from_word(uint32_t word, uint32_t bits) noexcept
{
    const double highest = top(bits);
    const auto masked = static_cast<double>(word & static_cast<uint32_t>(highest));
    return masked / highest * 2.0 - 1.0;
}

} // namespace MayaFlux::Kinesis::Discrete
