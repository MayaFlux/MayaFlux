#pragma once

#include "TextureExecutionContext.hpp"

#include "MayaFlux/Kinesis/Vision/Pyramid.hpp"
#include "MayaFlux/Kinesis/Vision/VisionContext.hpp"

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
 * returns the GpuComputeConfig for any given VisionOp. Ops that are
 * expressible via ShaderSpec are assembled at call time (no .comp file);
 * ops requiring neighbourhood access or structured output name a .comp
 * file. Ops with no GPU implementation return a config with INVALID_SHADER.
 *
 * The caller is responsible for only passing sequences composed of ops
 * whose config is valid. run() logs an error and returns a default
 * VisionResult on encountering INVALID_SHADER mid-sequence.
 */

/**
 * @brief Cross-run state of the flow context.
 *
 * Two atlases are bound for their whole life. Which one holds the current
 * frame is the curr parity, passed to shaders as a push constant, so ping-pong
 * is a bit flip and never rewrites a descriptor. Detections, retained previous
 * points and tracks live in the flow context's shared buffers and never leave
 * the GPU except for the final tracks readback.
 */
struct FlowState {
    std::shared_ptr<Core::VKImage> atlas[2];
    Kinesis::Vision::PyramidLayout layout;

    /**
     * @brief Dense flow images, allocated on first use of OpticalFlowDense.
     *
     * flow_out holds the finest-level result at frame resolution, alternated
     * by the same parity as the atlases so a result stays valid for one more
     * run. The rest share the atlas layout: flow_lvl holds the coarser levels,
     * dense_a and dense_b are the box filter scratch, and dense_tensor holds
     * the previous frame's summed structure tensor and confidence.
     */
    std::shared_ptr<Core::VKImage> flow_out[2];
    std::shared_ptr<Core::VKImage> flow_lvl;
    std::shared_ptr<Core::VKImage> dense_a;
    std::shared_ptr<Core::VKImage> dense_b;
    std::shared_ptr<Core::VKImage> dense_tensor;
    std::shared_ptr<Core::VKImage> flow_vis;
    Kinesis::Vision::PyramidLayout dense_layout;

    /**
     * @brief Most recent flow image and track list produced from a frame that
     *        differed from the one before it.
     *
     * A repeated camera frame carries no motion, so a run on one republishes
     * these instead of a field of zeros. Before any result exists, the fresh
     * result is published as is.
     */
    std::shared_ptr<Core::VKImage> last_flow;
    std::vector<Kinesis::Vision::TrackResult> last_tracks;

    /**
     * @brief Bookkeeping for the device resident track export.
     *
     * The two export buffers are shared buffers of the flow context and
     * alternate per published frame, so a delivered buffer stays valid for one
     * more publish. export_slot is the buffer the next publish writes,
     * last_export the view of the most recent distinct frame, and
     * export_pending marks a submitted track sequence whose export has not
     * been delivered yet. export_view caches one handle per buffer, created on
     * first delivery, so publishing allocates nothing per frame.
     */
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> export_view[2];
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> last_export;
    uint32_t export_slot { 0 };
    bool export_pending { false };

    uint32_t curr { 0 };
    bool have_prev { false };
    bool curr_ready { false };
    bool buffers_ready { false };
    Portal::Graphics::FenceID build_fence { Portal::Graphics::INVALID_FENCE };
};

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
    TextureExecutionContext cc_pipeline;
    TextureExecutionContext ingest; ///< Sampled-in, rgba32f-storage-out.
                                    ///< IMAGE mode. Runs vision_ingest.comp
                                    ///< at the top of a fresh run to convert
                                    ///< a non-storage seed frame before any
                                    ///< rgba32f op reads it.
    TextureExecutionContext flow; ///< Pyramidal optical flow. Fixed layout:
                                  ///< gray source image, two rgba16f pyramid
                                  ///< atlases, the dense flow images, and
                                  ///< shared buffers for detections, retained
                                  ///< points, tracks, and selection state.
                                  ///< Drives TrackKeypoints and
                                  ///< OpticalFlowDense, and reuses the
                                  ///< extract_peaks layout unchanged.

    FlowState flow_state;

    /**
     * @brief Walk state for the current run: sequence position, geometry,
     *        working image, and the result under construction.
     *
     * Lives here rather than as a run local so a deferred step's yield
     * preserves it without copying. Reset by begin() at the top of a fresh
     * run; left intact across a suspension.
     */
    Kinesis::Vision::GpuVisionPass pass;

    /**
     * @brief Input image the current walk started from.
     *
     * Ops needing the original frame rather than the working image read this
     * (contour_march stages it at binding 1). Set on a fresh run and untouched
     * across suspensions, so a polling call uses the frame the sequence began
     * on rather than whatever argument it was passed.
     *
     * GPU-only. The CPU pass has no equivalent: its handle is an index into a
     * reused slot pool, so retaining the input handle would retain a slot whose
     * contents the ping-pong overwrites.
     */
    std::shared_ptr<Core::VKImage> source;

    /**
     * @brief Shader currently bound on pixel, and the image staged into it.
     *
     * TextureExecutionContext does not report its own state, so run tracks
     * it. Held here rather than as a run local: a swap_shader or stage_image
     * elided on one call stays elided on the next. The allocated output
     * dimensions live in pass.storage_w / pass.storage_h.
     */
    GpuComputeConfig bound_config;
    std::shared_ptr<Core::VKImage> bound_staged;

    /**
     * @brief Outstanding work at a deferred step, and the point to resume.
     *
     * fence is INVALID_FENCE when nothing is outstanding. While live, run
     * polls it and does nothing else. The working image, geometry, and
     * partial result the resumed sequence needs are already in pass, which
     * is not reset while a suspension is active.
     */
    struct Suspension {
        Portal::Graphics::FenceID fence { Portal::Graphics::INVALID_FENCE };

        /**
         * @brief Work to run once the fence has signaled, before the walk
         *        resumes. Empty for steps that need none.
         *
         * Lets a deferred step read back or commit state on completion
         * rather than on submission. Dropped without running by reset().
         */
        std::function<void(VisionGpuContexts&)> finalize;

        [[nodiscard]] bool is_active() const
        {
            return fence != Portal::Graphics::INVALID_FENCE;
        }
    };

    Suspension suspended;

    /// op_ingest's dispatch + trailing-barrier fences. Not awaited by run();
    /// reaped by reap_ingest_fences on the next fresh run and in reset().
    Portal::Graphics::FenceID ingest_fence { Portal::Graphics::INVALID_FENCE };
    Portal::Graphics::FenceID ingest_barrier_fence { Portal::Graphics::INVALID_FENCE };

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
     * @brief GpuComputeConfig for a given VisionOp and its parameters.
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
    [[nodiscard]] static GpuComputeConfig config(
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

    /**
     * @brief Abandon outstanding work and clear the resume point.
     *
     * Waits on the fence before releasing it, then discards the retained
     * working image and partial result. Call when the pixel source changes
     * so the next run starts a fresh sequence. Safe with nothing outstanding.
     */
    void reset();

    /**
     * @brief True when a deferred step has work outstanding and the next run
     *        will poll rather than start a fresh sequence.
     */
    [[nodiscard]] bool is_suspended() const
    {
        return m_contexts && m_contexts->suspended.is_active();
    }

    VisionGpuExecutor() = default;
    ~VisionGpuExecutor() = default;
    VisionGpuExecutor(const VisionGpuExecutor&) = delete;
    VisionGpuExecutor& operator=(const VisionGpuExecutor&) = delete;
    VisionGpuExecutor(VisionGpuExecutor&&) = delete;
    VisionGpuExecutor& operator=(VisionGpuExecutor&&) = delete;

private:
    std::unique_ptr<VisionGpuContexts> m_contexts;
};

} // namespace MayaFlux::Yantra
