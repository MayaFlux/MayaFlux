#include "ImageData.hpp"

namespace MayaFlux::Kakshya {

std::optional<ImageData> ImageData::allocate(
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    ImageFormat format)
{
    using F = ImageFormat;

    if (width == 0 || height == 0 || channels == 0) {
        return std::nullopt;
    }

    ImageData result;
    result.width = width;
    result.height = height;
    result.channels = channels;
    result.format = format;

    const size_t element_count = static_cast<size_t>(width) * height * channels;

    switch (format) {
    case F::R8:
    case F::RG8:
    case F::RGBA8:
    case F::BGRA8:
        result.pixels.emplace<std::vector<uint8_t>>(element_count);
        break;

    case F::R16:
    case F::RG16:
    case F::RGBA16:
    case F::R16F:
    case F::RG16F:
    case F::RGBA16F:
        result.pixels.emplace<std::vector<uint16_t>>(element_count);
        break;

    case F::R32F:
    case F::RG32F:
    case F::RGBA32F:
        result.pixels.emplace<std::vector<float>>(element_count);
        break;

    default:
        return std::nullopt;
    }

    return result;
}

bool ImageData::is_consistent() const
{
    using F = ImageFormat;

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
