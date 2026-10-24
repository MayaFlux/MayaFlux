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
    const bool all_points = topology == PrimitiveTopology::POINT_LIST
        && mode == PolygonMode::POINT;

    auto& foundry = get_shader_foundry();
    if (!foundry.is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "needs_triangulation: ShaderFoundry is not initialized");
        return false;
    }

    const auto features = foundry.get_physical_device().getFeatures();
    const bool unsupported_polygon_mode = mode != PolygonMode::FILL
        && !all_points
        && !features.fillModeNonSolid;
    const bool unsupported_line_topology = is_line && !features.geometryShader;

    return unsupported_polygon_mode || unsupported_line_topology;
}

} // namespace MayaFlux::Portal::Graphics
