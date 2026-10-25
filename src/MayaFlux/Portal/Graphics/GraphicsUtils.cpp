#include "GraphicsUtils.hpp"

#include "ShaderFoundry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
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

bool needs_triangulation(PrimitiveTopology topology, PolygonMode mode)
{
    const bool is_line = topology == PrimitiveTopology::LINE_LIST
        || topology == PrimitiveTopology::LINE_STRIP;
    const bool is_triangle = topology == PrimitiveTopology::TRIANGLE_LIST
        || topology == PrimitiveTopology::TRIANGLE_STRIP
        || topology == PrimitiveTopology::TRIANGLE_FAN;

    auto& foundry = get_shader_foundry();
    if (!foundry.is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "needs_triangulation: ShaderFoundry is not initialized");
        return false;
    }

    const auto features = foundry.get_physical_device().getFeatures();
    if (is_line) {
        return !features.geometryShader
            || mode == PolygonMode::POINT
            || (mode != PolygonMode::FILL && !features.fillModeNonSolid);
    }

    return is_triangle && mode != PolygonMode::FILL && !features.fillModeNonSolid;
}

} // namespace MayaFlux::Portal::Graphics
