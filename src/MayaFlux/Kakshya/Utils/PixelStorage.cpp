#include "PixelStorage.hpp"

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Kakshya {

namespace {

    float decode_half(uint16_t h)
    {
        const uint32_t sign = static_cast<uint32_t>(h & 0x8000U) << 16;
        const uint32_t exp = (h >> 10) & 0x1FU;
        const uint32_t mant = h & 0x3FFU;

        uint32_t bits {};

        if (exp == 0) {
            if (mant == 0) {
                bits = sign;
            } else {
                uint32_t e = 0;
                uint32_t m = mant;
                while ((m & 0x400U) == 0) {
                    m <<= 1;
                    ++e;
                }
                m &= 0x3FFU;
                bits = sign | ((113U - e) << 23) | (m << 13);
            }
        } else if (exp == 0x1FU) {
            bits = sign | 0x7F800000U | (mant << 13);
        } else {
            bits = sign | ((exp - 15U + 127U) << 23) | (mant << 13);
        }

        float out {};
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    uint16_t encode_half(float f)
    {
        uint32_t bits {};
        std::memcpy(&bits, &f, sizeof(bits));

        const auto sign = static_cast<uint16_t>((bits >> 16) & 0x8000U);
        const int32_t exp = static_cast<int32_t>((bits >> 23) & 0xFFU) - 127 + 15;
        uint32_t mant = bits & 0x7FFFFFU;

        if (exp >= 0x1F)
            return static_cast<uint16_t>(sign | 0x7C00U);

        if (exp <= 0) {
            if (exp < -10)
                return sign;
            mant |= 0x800000U;
            return static_cast<uint16_t>(sign | (mant >> static_cast<uint32_t>(14 - exp)));
        }

        return static_cast<uint16_t>(
            sign | (static_cast<uint32_t>(exp) << 10) | (mant >> 13));
    }

} // namespace

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

double read_normalized_at(const DataVariant& v,
    ImageFormat format,
    const std::optional<DataDimension::ValueRange>& range,
    size_t elem_index)
{
    return std::visit(
        [&](const auto& vec) -> double {
            using T = typename std::decay_t<decltype(vec)>::value_type;

            if constexpr (std::is_same_v<T, uint8_t>
                || std::is_same_v<T, uint16_t>
                || std::is_same_v<T, float>) {

                if (elem_index >= vec.size())
                    return 0.0;

                double raw {};
                double type_max = 1.0;

                if constexpr (std::is_same_v<T, uint8_t>) {
                    raw = static_cast<double>(vec[elem_index]);
                    type_max = 255.0;
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    if (is_float_format(format)) {
                        raw = static_cast<double>(decode_half(vec[elem_index]));
                    } else {
                        raw = static_cast<double>(vec[elem_index]);
                        type_max = 65535.0;
                    }
                } else {
                    raw = static_cast<double>(vec[elem_index]);
                }

                if (!range)
                    return raw / type_max;

                if (range->invalid && raw == *range->invalid)
                    return std::numeric_limits<double>::quiet_NaN();

                const double span = range->max - range->min;
                if (span <= 0.0)
                    return 0.0;

                return std::clamp((raw - range->min) / span, 0.0, 1.0);
            } else {
                return 0.0;
            }
        },
        v);
}

double read_normalized_at(const DataVariant& v, ImageFormat format, size_t elem_index)
{
    return read_normalized_at(v, format, std::nullopt, elem_index);
}

void write_normalized_at(DataVariant& v,
    ImageFormat format,
    const std::optional<DataDimension::ValueRange>& range,
    size_t elem_index,
    double value)
{
    std::visit(
        [&](auto& vec) {
            using T = typename std::decay_t<decltype(vec)>::value_type;

            if constexpr (std::is_same_v<T, uint8_t>
                || std::is_same_v<T, uint16_t>
                || std::is_same_v<T, float>) {

                if (elem_index >= vec.size())
                    return;

                double raw {};

                if (range) {
                    if (std::isnan(value)) {
                        if (!range->invalid)
                            return;
                        raw = *range->invalid;
                    } else {
                        raw = range->min
                            + std::clamp(value, 0.0, 1.0) * (range->max - range->min);
                    }
                } else if constexpr (std::is_same_v<T, uint8_t>) {
                    raw = std::clamp(value * 255.0, 0.0, 255.0);
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    raw = is_float_format(format)
                        ? value
                        : std::clamp(value * 65535.0, 0.0, 65535.0);
                } else {
                    raw = value;
                }

                if constexpr (std::is_same_v<T, uint8_t>) {
                    vec[elem_index] = static_cast<uint8_t>(std::clamp(raw, 0.0, 255.0));
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    if (is_float_format(format)) {
                        vec[elem_index] = encode_half(static_cast<float>(raw));
                    } else {
                        vec[elem_index] = static_cast<uint16_t>(std::clamp(raw, 0.0, 65535.0));
                    }
                } else {
                    vec[elem_index] = static_cast<float>(raw);
                }
            }
        },
        v);
}

void write_normalized_at(DataVariant& v, ImageFormat format,
    size_t elem_index, double value)
{
    write_normalized_at(v, format, std::nullopt, elem_index, value);
}

bool format_has_variant_storage(ImageFormat format)
{
    const uint32_t channels = Portal::Graphics::TextureLoom::get_channel_count(format);
    return channels > 0
        && storage_element_size(format) * channels
        == Portal::Graphics::TextureLoom::get_bytes_per_pixel(format);
}

bool is_depth_format(ImageFormat format)
{
    switch (format) {
    case ImageFormat::DEPTH16:
    case ImageFormat::DEPTH24:
    case ImageFormat::DEPTH32F:
    case ImageFormat::DEPTH24_STENCIL8:
        return true;
    default:
        return false;
    }
}

} // namespace MayaFlux::Kakshya
