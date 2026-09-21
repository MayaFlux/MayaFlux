#pragma once

#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers::RelaxationSpecs {

using Portal::Graphics::BindingDirection;
using Portal::Graphics::KernelOp;
using Portal::Graphics::KernelTemplate;
using Portal::Graphics::ShaderSpec;

/**
 * @brief Jacobi-style diffusion rule: each cell blends toward the average
 *        of its 8 Moore neighbors.
 *
 * Built on KernelTemplate::Stencil with KernelOp::WeightedBlend. PC layout
 * is width, height, rate, neighbor_scale. Supply rate and neighbor_scale
 * through GridConfig::Stage::constants in that order. A neighbor_scale of
 * 1/8 produces a true average over the eight Moore neighbors.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec jacobi_diffusion()
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
    return spec;
}

} // namespace MayaFlux::Buffers::RelaxationSpecs
