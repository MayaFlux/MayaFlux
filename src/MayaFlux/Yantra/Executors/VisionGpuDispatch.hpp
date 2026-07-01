#pragma once

#include "TextureExecutionContext.hpp"

#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"

namespace MayaFlux::Yantra {

/**
 * @file VisionGpuDispatch.hpp
 * @brief GPU execution layer for Kinesis::Vision::VisionSequence.
 *
 * run_gpu() mirrors VisionExecutor::run() in contract: same input types,
 * same VisionResult output. Internally it drives a VisionGpuContexts through
 * the sequence via swap_shader() + stage_image() + dispatch_core() per step,
 * keeping the working image GPU-resident between steps via OutputMode::IMAGE.
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
 * @brief Fixed set of TextureExecutionContexts covering every GPU-implemented
 *        VisionOp shape.
 *
 * Constructed once via make_vision_gpu_contexts() and reused across every
 * subsequent run_gpu() call. Never rebuilt inside run_gpu or per-step; the
 * bindings each member declares are fixed at construction and dictated
 * entirely by the shaders they drive, not by caller preference.
 *
 * There is exactly one correct binding layout per member, so this struct
 * carries no configuration surface. Callers with no reason to own contexts
 * explicitly should use the three-argument run_gpu() overload instead of
 * constructing this directly.
 */
struct MAYAFLUX_API VisionGpuContexts {
    TextureExecutionContext pixel; ///< Image pipeline. IMAGE mode. Drives
                                   ///< every op that reads/writes the
                                   ///< working image (Threshold, Sobel,
                                   ///< HarrisResponse, Downsample2x, etc).
    TextureExecutionContext structured; ///< Buffer-only readback. SCALAR mode.
                                        ///< Drives ops with no image output
                                        ///< of their own (ExtractPeaks).
    TextureExecutionContext labels; ///< Image + aux SSBO. IMAGE mode.
                                    ///< Drives ops needing both a resident
                                    ///< image output and structured aux
                                    ///< data (ConnectedComponents,
                                    ///< FindContours).

    /**
     * @brief Construct all three contexts in place with the one correct
     *        binding layout for every currently GPU-implemented VisionOp.
     *
     * TextureExecutionContext has no copy or move constructor (it owns GPU
     * resource handles), so each member is built directly in this
     * constructor's initializer list rather than assigned from a temporary.
     * Prefer make_vision_gpu_contexts() over calling this directly; it
     * returns a stable heap-owned instance suitable for storing and reusing.
     */
    VisionGpuContexts();
};

/**
 * @brief Heap-allocate a VisionGpuContexts with the one correct binding
 *        layout for every currently GPU-implemented VisionOp.
 *
 * Returned as a unique_ptr because TextureExecutionContext is immovable;
 * VisionGpuContexts cannot be returned by value. Call once and hold the
 * result. The instance is safe to reuse indefinitely across frames; no
 * member needs resetting between run_gpu() calls.
 *
 * @return Heap-owned, fully configured VisionGpuContexts.
 */
[[nodiscard]] MAYAFLUX_API std::unique_ptr<VisionGpuContexts> make_vision_gpu_contexts();

/**
 * @brief Execute a VisionSequence on the GPU through a caller-owned context set.
 *
 * Explicit form. contexts must have been constructed via
 * make_vision_gpu_contexts() or an equivalent caller-built VisionGpuContexts.
 * Reused across calls with no reset needed; construction is the caller's
 * responsibility and never happens inside this function.
 *
 * Prefer this overload only when explicit ownership is required, e.g. running
 * more than one independent GPU vision pipeline concurrently, each needing
 * its own context set. Otherwise use the three-argument overload.
 *
 * @param contexts Long-lived context set. Never constructed internally.
 * @param sequence Ordered steps to execute.
 * @param image    Input frame in eShaderReadOnlyOptimal layout.
 * @param w        Frame width in pixels.
 * @param h        Frame height in pixels.
 * @return         VisionResult matching the VisionExecutor::run() contract.
 */
[[nodiscard]] MAYAFLUX_API Kinesis::Vision::VisionResult run_gpu(
    VisionGpuContexts& contexts,
    const Kinesis::Vision::VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h);

/**
 * @brief Execute a VisionSequence on the GPU using a process-wide default
 *        context set.
 *
 * Convenience form for callers with no reason to own contexts explicitly.
 * The default VisionGpuContexts is constructed once on first call and
 * reused for every subsequent call to this overload. Not safe to call
 * concurrently from multiple threads; matches the existing single-caller
 * assumption of the explicit overload.
 *
 * @param sequence Ordered steps to execute.
 * @param image    Input frame in eShaderReadOnlyOptimal layout.
 * @param w        Frame width in pixels.
 * @param h        Frame height in pixels.
 * @return         VisionResult matching the VisionExecutor::run() contract.
 */
[[nodiscard]] MAYAFLUX_API Kinesis::Vision::VisionResult run_gpu(
    const Kinesis::Vision::VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h);

} // namespace MayaFlux::Yantra
