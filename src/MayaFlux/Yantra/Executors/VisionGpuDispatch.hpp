#pragma once

#include "ShaderSpecBinding.hpp"
#include "TextureExecutionContext.hpp"

#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"

namespace MayaFlux::Yantra {

/**
 * @file VisionGpuDispatch.hpp
 * @brief GPU execution layer for Kinesis::Vision::VisionSequence.
 *
 * run_gpu() mirrors VisionExecutor::run() in contract: same input types,
 * same VisionResult output. Internally it drives a single
 * TextureExecutionContext through the sequence via swap_shader() +
 * stage_image() + dispatch_core() per step, keeping the working image
 * GPU-resident between steps via OutputMode::IMAGE.
 *
 * vision_gpu_config() is the inspectable shader config table. It returns
 * the GpuShaderConfig for any given VisionOp. Ops that are expressible
 * via ShaderSpec are assembled at call time (no .comp file); ops requiring
 * neighbourhood access or structured output name a .comp file. Ops with
 * no GPU implementation return a config with INVALID_SHADER.
 *
 * The caller is responsible for only passing sequences composed of ops
 * whose config is valid. run_gpu() logs an error and returns a default
 * VisionResult on encountering INVALID_SHADER mid-sequence.
 */

/**
 * @brief GpuShaderConfig for a given VisionOp and its parameters.
 *
 * Assembled ops (Threshold, NormalizeInplace, NormalizeRange, RgbaToGray,
 * GrayToRgba) produce a config via ShaderSpec::Assemble + config_from_spec
 * with no .comp file. All other implemented ops reference a .comp path
 * under Portal/Shaders/Vision/. Unimplemented ops return a config with
 * shader_id == INVALID_SHADER.
 *
 * @param op     VisionOp to look up.
 * @param params Parameters for that op; used to derive push constant size
 *               for assembled ops.
 */
[[nodiscard]] MAYAFLUX_API GpuShaderConfig vision_gpu_config(
    Kinesis::Vision::VisionOp op,
    const Kinesis::Vision::VisionParams& params);

/**
 * @brief Execute a VisionSequence on the GPU through one TextureExecutionContext.
 *
 * The context must have been constructed with OutputMode::IMAGE and the
 * standard vision binding layout (storage image at binding 0, sampled
 * image at binding 1). It is mutated per step via swap_shader() and
 * stage_image(); no other external state is required.
 *
 * @param ctx      Execution context. Reused across frames with no reset needed.
 * @param sequence Ordered steps to execute.
 * @param image    Input frame in eShaderReadOnlyOptimal layout.
 * @param w        Frame width in pixels.
 * @param h        Frame height in pixels.
 * @return         VisionResult matching the VisionExecutor::run() contract.
 */
[[nodiscard]] MAYAFLUX_API Kinesis::Vision::VisionResult run_gpu(
    TextureExecutionContext& ctx,
    const Kinesis::Vision::VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h);

} // namespace MayaFlux::Yantra
