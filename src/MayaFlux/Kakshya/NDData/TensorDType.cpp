#include "TensorDType.hpp"

namespace MayaFlux::Kakshya {

std::optional<size_t> TensorDType::storage_bytes(uint64_t count) const noexcept
{
    const size_t width = element_bits();
    if (width == 0 || count > (std::numeric_limits<size_t>::max() - 7) / width)
        return std::nullopt;
    return (static_cast<size_t>(count) * width + 7) / 8;
}

std::string TensorDType::name() const
{
    std::string base;
    switch (kind) {
    case TensorDTypeKind::INT:
        base = "i" + std::to_string(bits);
        break;
    case TensorDTypeKind::UINT:
        base = "u" + std::to_string(bits);
        break;
    case TensorDTypeKind::FLOAT:
        base = "f" + std::to_string(bits);
        break;
    case TensorDTypeKind::OPAQUE_HANDLE:
        base = "handle" + std::to_string(bits);
        break;
    case TensorDTypeKind::BFLOAT:
        base = "bf" + std::to_string(bits);
        break;
    case TensorDTypeKind::COMPLEX:
        base = "c" + std::to_string(bits);
        break;
    case TensorDTypeKind::BOOL:
        base = "bool";
        break;
    case TensorDTypeKind::FLOAT8_E3M4:
        base = "f8e3m4";
        break;
    case TensorDTypeKind::FLOAT8_E4M3:
        base = "f8e4m3";
        break;
    case TensorDTypeKind::FLOAT8_E4M3B11FNUZ:
        base = "f8e4m3b11fnuz";
        break;
    case TensorDTypeKind::FLOAT8_E4M3FN:
        base = "f8e4m3fn";
        break;
    case TensorDTypeKind::FLOAT8_E4M3FNUZ:
        base = "f8e4m3fnuz";
        break;
    case TensorDTypeKind::FLOAT8_E5M2:
        base = "f8e5m2";
        break;
    case TensorDTypeKind::FLOAT8_E5M2FNUZ:
        base = "f8e5m2fnuz";
        break;
    case TensorDTypeKind::FLOAT8_E8M0FNU:
        base = "f8e8m0fnu";
        break;
    case TensorDTypeKind::FLOAT6_E2M3FN:
        base = "f6e2m3fn";
        break;
    case TensorDTypeKind::FLOAT6_E3M2FN:
        base = "f6e3m2fn";
        break;
    case TensorDTypeKind::FLOAT4_E2M1FN:
        base = "f4e2m1fn";
        break;
    case TensorDTypeKind::BCOMPLEX:
        base = "bc" + std::to_string(bits);
        break;
    }
    return lanes == 1 ? base : base + "x" + std::to_string(lanes);
}

std::optional<GpuDataFormat> TensorDType::gpu_format() const noexcept
{
    if (kind == TensorDTypeKind::FLOAT && bits == 32) {
        switch (lanes) {
        case 1:
            return GpuDataFormat::FLOAT32;
        case 2:
            return GpuDataFormat::VEC2_F32;
        case 3:
            return GpuDataFormat::VEC3_F32;
        case 4:
            return GpuDataFormat::VEC4_F32;
        default:
            return std::nullopt;
        }
    }
    if (kind == TensorDTypeKind::FLOAT && bits == 64) {
        switch (lanes) {
        case 1:
            return GpuDataFormat::FLOAT64;
        case 2:
            return GpuDataFormat::VEC2_F64;
        case 3:
            return GpuDataFormat::VEC3_F64;
        case 4:
            return GpuDataFormat::VEC4_F64;
        default:
            return std::nullopt;
        }
    }
    if (lanes != 1)
        return std::nullopt;
    if (kind == TensorDTypeKind::INT && bits == 32)
        return GpuDataFormat::INT32;
    if (kind == TensorDTypeKind::UINT && bits == 32)
        return GpuDataFormat::UINT32;
    if (kind == TensorDTypeKind::UINT && bits == 8)
        return GpuDataFormat::UINT8;
    if (kind == TensorDTypeKind::UINT && bits == 16)
        return GpuDataFormat::UINT16;
    if (kind == TensorDTypeKind::FLOAT && bits == 16)
        return GpuDataFormat::UINT16;
    return std::nullopt;
}

}
