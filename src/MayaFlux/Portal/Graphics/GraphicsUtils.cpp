#include "GraphicsUtils.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Portal::Graphics {

size_t element_type_bytes(GpuBufferBinding::ElementType et) noexcept
{
    switch (et) {
    case GpuBufferBinding::ElementType::FLOAT32:
        return Kakshya::gpu_data_format_bytes(Kakshya::GpuDataFormat::FLOAT32);
    case GpuBufferBinding::ElementType::UINT32:
        return Kakshya::gpu_data_format_bytes(Kakshya::GpuDataFormat::UINT32);
    case GpuBufferBinding::ElementType::INT32:
        return Kakshya::gpu_data_format_bytes(Kakshya::GpuDataFormat::INT32);
    case GpuBufferBinding::ElementType::PASSTHROUGH:
    case GpuBufferBinding::ElementType::IMAGE_STORAGE:
    case GpuBufferBinding::ElementType::IMAGE_SAMPLED:
    default:
        return 0;
    }
}

} // namespace MayaFlux::Portal::Graphics
