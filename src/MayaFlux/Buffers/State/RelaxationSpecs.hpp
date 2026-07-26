#pragma once

#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers::RelaxationSpecs {

using Portal::Graphics::BindingDirection;
using Portal::Graphics::KernelOp;
using Portal::Graphics::KernelTemplate;
using Portal::Graphics::ShaderSpec;

/**
 * @brief Jacobi-style diffusion rule: each cell blends toward the average
 *        of its 8 Moore neighbors at the given rate.
 *
 * Built on KernelTemplate::Stencil with KernelOp::WeightedBlend. PC layout
 * is width, height, rate, neighbor_scale — neighbor_scale defaults to
 * 1/8 to produce a true average over 8 neighbors.
 *
 * @param rate Blend rate per generation, 0 (no change) to 1 (full adopt).
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec jacobi_diffusion(float rate = 0.2F)
{
    auto spec = ShaderSpec::Assemble {}
                    .tmpl(KernelTemplate::Stencil)
                    .ssbo("state_in", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
                    .ssbo("state_out", BindingDirection::Output, Kakshya::GpuDataFormat::FLOAT32)
                    .pc("width", Kakshya::GpuDataFormat::UINT32)
                    .pc("height", Kakshya::GpuDataFormat::UINT32)
                    .pc("rate", Kakshya::GpuDataFormat::FLOAT32)
                    .pc("neighbor_scale", Kakshya::GpuDataFormat::FLOAT32)
                    .op(KernelOp::WeightedBlend)
                    .workgroup(16, 16)
                    .build();
    (void)rate;
    return spec;
}

/**
 * @brief Binary state-to-vertex emit: nonzero state draws at full scale,
 *        zero state degenerates to a zero-scale (invisible) vertex.
 *
 * Input state format: UINT32. Output: VEC4_F32 per vertex.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec emit_binary()
{
    return ShaderSpec::Assemble {}
        .ssbo("cell_state", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
        .ssbo("vertices", BindingDirection::Output, Kakshya::GpuDataFormat::VEC4_F32)
        .pc("threshold", Kakshya::GpuDataFormat::FLOAT32)
        .op(KernelOp::CompareGE)
        .workgroup(256)
        .build();
}

/**
 * @brief Scalar-ramp state-to-vertex emit: float state value scaled and
 *        offset into a visible output range.
 *
 * Input state format: FLOAT32. Output: VEC4_F32 per vertex.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec emit_scalar_ramp()
{
    return ShaderSpec::Assemble {}
        .ssbo("cell_state", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
        .ssbo("vertices", BindingDirection::Output, Kakshya::GpuDataFormat::VEC4_F32)
        .pc("scale", Kakshya::GpuDataFormat::FLOAT32)
        .pc("offset", Kakshya::GpuDataFormat::FLOAT32)
        .op(KernelOp::ScaleOffset)
        .workgroup(256)
        .build();
}

} // namespace MayaFlux::Buffers::RelaxationSpecs
