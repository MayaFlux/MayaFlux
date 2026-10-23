#include "GpuDataFormat.hpp"

namespace MayaFlux::Kakshya {

size_t gpu_data_format_bytes(GpuDataFormat fmt) noexcept
{
    switch (fmt) {
    case GpuDataFormat::UINT8:
        return 1;
    case GpuDataFormat::UINT16:
        return 2;
    case GpuDataFormat::FLOAT32:
    case GpuDataFormat::INT32:
    case GpuDataFormat::UINT32:
        return 4;
    case GpuDataFormat::FLOAT64:
    case GpuDataFormat::VEC2_F32:
        return 8;
    case GpuDataFormat::VEC3_F32:
        return 12;
    case GpuDataFormat::VEC4_F32:
    case GpuDataFormat::VEC2_F64:
        return 16;
    case GpuDataFormat::VEC3_F64:
        return 24;
    case GpuDataFormat::VEC4_F64:
        return 32;
    case GpuDataFormat::MAT4_F32:
        return 64;
    default:
        return 0;
    }
}

size_t gpu_data_format_align(GpuDataFormat fmt) noexcept
{
    switch (fmt) {
    case GpuDataFormat::UINT8:
        return 1;
    case GpuDataFormat::UINT16:
        return 2;
    case GpuDataFormat::FLOAT32:
    case GpuDataFormat::INT32:
    case GpuDataFormat::UINT32:
        return 4;
    case GpuDataFormat::FLOAT64:
    case GpuDataFormat::VEC2_F32:
        return 8;
    case GpuDataFormat::VEC3_F32:
    case GpuDataFormat::VEC4_F32:
    case GpuDataFormat::MAT4_F32:
    case GpuDataFormat::VEC2_F64:
        return 16;
    case GpuDataFormat::VEC3_F64:
    case GpuDataFormat::VEC4_F64:
        return 32;
    default:
        return 0;
    }
}

size_t gpu_data_format_components(GpuDataFormat fmt) noexcept
{
    switch (fmt) {
    case GpuDataFormat::FLOAT32:
    case GpuDataFormat::FLOAT64:
    case GpuDataFormat::INT32:
    case GpuDataFormat::UINT32:
    case GpuDataFormat::UINT8:
    case GpuDataFormat::UINT16:
        return 1;
    case GpuDataFormat::VEC2_F32:
    case GpuDataFormat::VEC2_F64:
        return 2;
    case GpuDataFormat::VEC3_F32:
    case GpuDataFormat::VEC3_F64:
        return 3;
    case GpuDataFormat::VEC4_F32:
    case GpuDataFormat::VEC4_F64:
        return 4;
    case GpuDataFormat::MAT4_F32:
        return 16;
    default:
        return 0;
    }
}

size_t gpu_value_write(const GpuValue& value, std::span<std::byte> out) noexcept
{
    return std::visit([&](const auto& held) -> size_t {
        if (out.size() < sizeof(held)) {
            return 0;
        }
        std::memcpy(out.data(), &held, sizeof(held));
        return sizeof(held);
    },
        value);
}

namespace {

    template <typename Component>
    void store_components(std::span<const double> components, std::byte* out) noexcept
    {
        for (size_t i = 0; i < components.size(); ++i) {
            const Component converted = saturate_cast<Component>(components[i]);
            std::memcpy(out + i * sizeof(Component), &converted, sizeof(Component));
        }
    }

} // namespace

bool pack_gpu_components(
    GpuDataFormat dst,
    std::span<const double> components,
    std::span<std::byte> out) noexcept
{
    if (components.size() != gpu_data_format_components(dst) || out.size() < gpu_data_format_bytes(dst)) {
        return false;
    }

    switch (dst) {
    case GpuDataFormat::FLOAT32:
    case GpuDataFormat::VEC2_F32:
    case GpuDataFormat::VEC3_F32:
    case GpuDataFormat::VEC4_F32:
    case GpuDataFormat::MAT4_F32:
        store_components<float>(components, out.data());
        return true;
    case GpuDataFormat::FLOAT64:
    case GpuDataFormat::VEC2_F64:
    case GpuDataFormat::VEC3_F64:
    case GpuDataFormat::VEC4_F64:
        store_components<double>(components, out.data());
        return true;
    case GpuDataFormat::INT32:
        store_components<int32_t>(components, out.data());
        return true;
    case GpuDataFormat::UINT32:
        store_components<uint32_t>(components, out.data());
        return true;
    case GpuDataFormat::UINT8:
        store_components<uint8_t>(components, out.data());
        return true;
    case GpuDataFormat::UINT16:
        store_components<uint16_t>(components, out.data());
        return true;
    default:
        return false;
    }
}

} // namespace MayaFlux::Kakshya
