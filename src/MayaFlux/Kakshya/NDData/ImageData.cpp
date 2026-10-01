#include "ImageData.hpp"

namespace MayaFlux::Kakshya {

bool ImageData::is_consistent() const
{
    using F = Portal::Graphics::ImageFormat;

    const bool has_u8 = std::holds_alternative<std::vector<uint8_t>>(pixels);
    const bool has_u16 = std::holds_alternative<std::vector<uint16_t>>(pixels);
    const bool has_f32 = std::holds_alternative<std::vector<float>>(pixels);

    switch (format) {
    case F::R8:
    case F::RG8:
    case F::RGBA8:
    case F::BGRA8:
        return has_u8;

    case F::R16:
    case F::RG16:
    case F::RGBA16:
        return has_u16;

    case F::R16F:
    case F::RG16F:
    case F::RGBA16F:
        return has_u16;

    case F::R32F:
    case F::RG32F:
    case F::RGBA32F:
        return has_f32;

    default:
        return false;
    }
}

} // namespace MayaFlux::Kakshya
