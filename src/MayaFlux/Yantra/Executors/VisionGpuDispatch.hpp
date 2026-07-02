#pragma once

#include "TextureExecutionContext.hpp"

#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"

namespace MayaFlux::Yantra {

/**
 * @file VisionGpuDispatch.hpp
 * @brief GPU execution layer for Kinesis::Vision::VisionSequence.
 *
 * VisionGpuExecutor::run() mirrors VisionExecutor::run() in contract: same
 * input types, same VisionResult output. Internally it drives a
 * VisionGpuContexts through the sequence via swap_shader() + stage_image()
 * + dispatch_core() per step, keeping the working image GPU-resident
 * between steps via OutputMode::IMAGE.
 *
 * VisionGpuExecutor::config() is the inspectable shader config table. It
 * returns the GpuShaderConfig for any given VisionOp. Ops that are
 * expressible via ShaderSpec are assembled at call time (no .comp file);
 * ops requiring neighbourhood access or structured output name a .comp
 * file. Ops with no GPU implementation return a config with INVALID_SHADER.
 *
 * The caller is responsible for only passing sequences composed of ops
 * whose config is valid. run() logs an error and returns a default
 * VisionResult on encountering INVALID_SHADER mid-sequence.
 */

/**
 * @brief Fixed set of TextureExecutionContexts covering every GPU-implemented
 *        VisionOp shape.
 *
 * Owned exclusively by VisionGpuExecutor, which lazily constructs and holds
 * one instance per executor via m_contexts. Never rebuilt inside run() or
 * per-step; the bindings each member declares are fixed at construction and
 * dictated entirely by the shaders they drive, not by caller preference.
 *
 * There is exactly one correct binding layout per member, so this struct
 * carries no configuration surface.
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
     * Constructed lazily by VisionGpuExecutor on first run(); never
     * constructed directly by callers.
     */
    VisionGpuContexts();
};

/**
 * @class VisionGpuExecutor
 * @brief Stateful GPU dispatch engine for VisionSequence execution.
 *
 * Owns a lazily-constructed VisionGpuContexts (m_contexts) plus any
 * op-specific persistent GPU state that doesn't belong on a shared
 * context (e.g. ConnectedComponents' ping-pong output image). Mirrors
 * VisionExecutor's ownership model on the CPU side: construct one
 * instance per independent pipeline, hold it, call run() every frame.
 *
 * Immovable: TextureExecutionContext owns GPU resource handles with no
 * copy or move constructor, so VisionGpuContexts and therefore
 * VisionGpuExecutor cannot be copied or moved either. Construct once,
 * hold by reference, pointer, or shared_ptr, reuse indefinitely.
 *
 * Not safe to call run() concurrently from multiple threads on the same
 * instance. Independent pipelines running concurrently should each own
 * a separate VisionGpuExecutor instance.
 */
class MAYAFLUX_API VisionGpuExecutor {
public:
    /**
     * @brief GpuShaderConfig for a given VisionOp and its parameters.
     *
     * Assembled ops (Threshold, NormalizeInplace, NormalizeRange, RgbaToGray,
     * GrayToRgba) produce a config via ShaderSpec::Assemble + config_from_spec
     * with no .comp file. All other implemented ops reference a .comp path
     * under Portal/Shaders/Vision/. Unimplemented ops return a config with
     * shader_id == INVALID_SHADER.
     *
     * Stateless; does not depend on or affect m_contexts.
     *
     * @param op     VisionOp to look up.
     * @param params Parameters for that op; used to derive push constant size
     *               for assembled ops.
     */
    [[nodiscard]] static GpuShaderConfig config(
        Kinesis::Vision::VisionOp op,
        const Kinesis::Vision::VisionParams& params);

    /**
     * @brief Execute a VisionSequence on the GPU through an explicit context set.
     *
     * contexts is caller-supplied rather than the instance's own lazily-built
     * m_contexts. Reused across calls with no reset needed; construction is
     * the caller's responsibility and never happens inside this function.
     *
     * @param contexts Long-lived context set. Never constructed internally.
     * @param sequence Ordered steps to execute.
     * @param image    Input frame in eShaderReadOnlyOptimal layout.
     * @param w        Frame width in pixels.
     * @param h        Frame height in pixels.
     * @return         VisionResult matching the VisionExecutor::run() contract.
     */
    [[nodiscard]] Kinesis::Vision::VisionResult run(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::VisionSequence& sequence,
        const std::shared_ptr<Core::VKImage>& image,
        uint32_t w, uint32_t h);

    /**
     * @brief Execute a VisionSequence on the GPU using this instance's own
     *        lazily-constructed context set.
     *
     * m_contexts is built on first call and reused for every subsequent
     * call to this overload on the same VisionGpuExecutor instance. The
     * primary entry point; prefer this over the explicit-contexts overload
     * unless a caller specifically needs to inspect or share a
     * VisionGpuContexts across multiple calls outside this class.
     *
     * @param sequence Ordered steps to execute.
     * @param image    Input frame in eShaderReadOnlyOptimal layout.
     * @param w        Frame width in pixels.
     * @param h        Frame height in pixels.
     * @return         VisionResult matching the VisionExecutor::run() contract.
     */
    [[nodiscard]] Kinesis::Vision::VisionResult run(
        const Kinesis::Vision::VisionSequence& sequence,
        const std::shared_ptr<Core::VKImage>& image,
        uint32_t w, uint32_t h);

    VisionGpuExecutor() = default;
    ~VisionGpuExecutor() = default;
    VisionGpuExecutor(const VisionGpuExecutor&) = delete;
    VisionGpuExecutor& operator=(const VisionGpuExecutor&) = delete;
    VisionGpuExecutor(VisionGpuExecutor&&) = delete;
    VisionGpuExecutor& operator=(VisionGpuExecutor&&) = delete;

private:
    std::unique_ptr<VisionGpuContexts> m_contexts;
    std::shared_ptr<Core::VKImage> m_cc_ping_pong_b;
    uint32_t m_cc_ping_pong_w { 0 };
    uint32_t m_cc_ping_pong_h { 0 };
};

} // namespace MayaFlux::Yantra
