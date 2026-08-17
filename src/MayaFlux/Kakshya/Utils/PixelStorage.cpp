#include "PixelStorage.hpp"

namespace MayaFlux::Kakshya {

using Portal::Graphics::ImageFormat;

size_t storage_element_size(ImageFormat format)
{
    switch (format) {
    case ImageFormat::R8:
    case ImageFormat::RG8:
    case ImageFormat::RGB8:
    case ImageFormat::RGBA8:
    case ImageFormat::RGBA8_SRGB:
    case ImageFormat::BGRA8:
    case ImageFormat::BGRA8_SRGB:
    case ImageFormat::DEPTH24:
    case ImageFormat::DEPTH24_STENCIL8:
        return 1;

    case ImageFormat::R16:
    case ImageFormat::RG16:
    case ImageFormat::RGBA16:
    case ImageFormat::R16F:
    case ImageFormat::RG16F:
    case ImageFormat::RGBA16F:
    case ImageFormat::DEPTH16:
        return 2;

    case ImageFormat::R32F:
    case ImageFormat::RG32F:
    case ImageFormat::RGBA32F:
    case ImageFormat::DEPTH32F:
        return 4;

    default:
        return 1;
    }
}

bool is_float_format(ImageFormat format)
{
    switch (format) {
    case ImageFormat::R16F:
    case ImageFormat::RG16F:
    case ImageFormat::RGBA16F:
    case ImageFormat::R32F:
    case ImageFormat::RG32F:
    case ImageFormat::RGBA32F:
    case ImageFormat::DEPTH32F:
        return true;
    default:
        return false;
    }
}

DataVariant make_empty_storage(ImageFormat format, size_t element_count)
{
    switch (storage_element_size(format)) {
    case 2:
        return std::vector<uint16_t>(element_count, 0U);
    case 4:
        return std::vector<float>(element_count, 0.0F);
    default:
        return std::vector<uint8_t>(element_count, 0U);
    }
}

double read_normalized_at(const DataVariant& v, ImageFormat format, size_t elem_index)
{
    return std::visit(
        [format, elem_index](const auto& vec) -> double {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            if (elem_index >= vec.size())
                return 0.0;
            if constexpr (std::is_same_v<T, uint8_t>) {
                return static_cast<double>(vec[elem_index]) / 255.0;
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return is_float_format(format)
                    ? static_cast<double>(vec[elem_index])
                    : static_cast<double>(vec[elem_index]) / 65535.0;
            } else if constexpr (std::is_same_v<T, float>) {
                return static_cast<double>(vec[elem_index]);
            } else {
                return 0.0;
            }
        },
        v);
}

void write_normalized_at(DataVariant& v, ImageFormat format, size_t elem_index, double value)
{
    std::visit(
        [format, elem_index, value](auto& vec) {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            if (elem_index >= vec.size())
                return;
            if constexpr (std::is_same_v<T, uint8_t>) {
                vec[elem_index] = static_cast<uint8_t>(
                    std::clamp(value * 255.0, 0.0, 255.0));
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                if (is_float_format(format)) {
                    vec[elem_index] = static_cast<uint16_t>(value);
                } else {
                    vec[elem_index] = static_cast<uint16_t>(
                        std::clamp(value * 65535.0, 0.0, 65535.0));
                }
            } else if constexpr (std::is_same_v<T, float>) {
                vec[elem_index] = static_cast<float>(value);
            }
        },
        v);
}

}
