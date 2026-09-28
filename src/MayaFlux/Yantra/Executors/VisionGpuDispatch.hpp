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
     * @brief Track count of the last distinct frame, independent of whether a
     *        host list was produced for it, and whether the submitted track
     *        sequence should read its tracks back to the host.
     */
    uint32_t last_track_count { 0 };
    bool host_pending { true };

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
    TextureExecutionContext component_contours; ///< Shared image and buffer
                                                ///< pipeline for
                                                ///< ConnectedComponents and
                                                ///< FindContours. Holds labels,
                                                ///< component bounds, trace
                                                ///< points, and contour
                                                ///< metadata across the two
                                                ///< steps.
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
     * @brief Host tracks decoded on demand from a result's exported buffer.
     *
     * For sequences run with export_tracks and without host_tracks, where the
     * structured result is empty. Reads the buffer through its host mapping,
     * so it is meant for occasional host access, not per frame use on a
     * device local buffer. The buffer is valid for one more run after the
     * result was delivered.
     *
     * @param result A result whose tracks_buffer came from this executor.
     * @return The exported tracks, or empty when the result carries no
     *         readable export.
     */
    [[nodiscard]] static std::vector<Kinesis::Vision::TrackResult> read_exported_tracks(
        const Kinesis::Vision::VisionResult& result);

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
     * @brief Isolate one ConnectedComponents label's silhouette on GPU,
     *        through the explicit context set that produced the label
     *        buffer.
     *
     * Dispatches vision_label_select.comp through contexts.component_contours,
     * the same context ConnectedComponents/FindContours already own: reads
     * dense_label in place at its existing binding, no cross-context buffer
     * copy. Requires a ConnectedComponents step to have already run against
     * these exact contexts (this run or an earlier one); dense_label holds
     * whichever frame's labels were computed last.
     *
     * @param contexts     The same context set a prior run() populated.
     * @param source       Frame to select pixels from, eShaderReadOnlyOptimal.
     * @param target_label 1-based label id, matching BoundingBox::label_id /
     *                     Contour::label_id from that same run.
     * @param w            Frame width in pixels.
     * @param h            Frame height in pixels.
     * @return             New image, transparent everywhere outside the
     *                     selected label's silhouette.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> select_label(
        VisionGpuContexts& contexts,
        const std::shared_ptr<Core::VKImage>& source,
        uint32_t target_label,
        uint32_t w, uint32_t h);

    /**
     * @brief select_label() using this instance's own lazily-constructed
     *        context set. See the explicit-contexts overload for the
     *        ConnectedComponents-must-have-already-run requirement.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> select_label(
        const std::shared_ptr<Core::VKImage>& source,
        uint32_t target_label,
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

    /**
     * @brief 2D Gaussian kernel for convolution, cached by (radius, sigma
     *        bit pattern).
     *
     * Sigma is a tuning parameter that rarely changes frame to frame;
     * recomputing exp() over (2*radius+1)^2 taps and reallocating the
     * kernel every call is pure repeated work for an identical result.
     *
     * @param radius Radius of the kernel in pixels. Kernel size is (2*radius + 1)^2.
     * @param sigma  Standard deviation of the Gaussian.
     * @return       Normalized kernel weights as a flat vector in row-major order.
     */
    const std::vector<float>& gaussian_kernel_2d(uint32_t radius, float sigma);

    /**
     * @brief Apply an Otsu threshold to the current image.
     *
     * Computes the histogram and threshold on the GPU, then replaces the
     * working image with the binary result and publishes it as debug_labels.
     *
     * @return The resulting image and the image it was derived from.
     */
    Kinesis::Vision::GpuVisionPass::Completed op_threshold_otsu(VisionGpuContexts& contexts);

    /**
     * @brief Apply morphological opening or closing to the current image.
     *
     * Opening erodes then dilates; closing dilates then erodes. The final
     * image becomes the working image for the following step.
     *
     * @return The resulting image and the image it was derived from.
     */
    Kinesis::Vision::GpuVisionPass::Completed op_open_close(
        VisionGpuContexts& contexts,
        Kinesis::Vision::VisionOp op,
        const Kinesis::Vision::MorphParams& p);

    /**
     * @brief Detect edges in the current image with the Canny pipeline.
     *
     * Applies smoothing, gradients, suppression, threshold classification,
     * and hysteresis. The final edge image becomes the working image and is
     * published as debug_labels.
     *
     * @return The resulting edge image and the original input image.
     */
    Kinesis::Vision::GpuVisionPass::Completed op_canny(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::VisionParams& params,
        const Kinesis::Vision::CannyParams& p);

    /**
     * @brief Compute the Harris response of the current image.
     *
     * Packs and smooths image gradients before evaluating the response. The
     * response image becomes the working image for a following ExtractPeaks.
     *
     * @return The response image and the original input image.
     */
    Kinesis::Vision::GpuVisionPass::Completed op_harris_response(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::HarrisParams& p);

    /**
     * @brief Extract and rank keypoints from the current response image.
     *
     * Produces structured host keypoints unless TrackKeypoints follows
     * immediately. In that sequence, tracking extracts detections directly
     * into its GPU buffers.
     */
    void op_extract_peaks(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::ExtractPeaksParams& p);

    /**
     * @brief Label components in the current image on the GPU.
     *
     * Produces component counts and bounds for structured output. When
     * FindContours follows immediately, the labels stay on the GPU for that
     * step and component bounds are not read back to the host.
     */
    void op_connected_components(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::ConnectedComponentsParams& p);

    /**
     * @brief Trace contours from the immediately preceding component labels.
     *
     * Publishes either structured contours or a rendered contour image,
     * according to the step parameters.
     *
     * @return False if ConnectedComponents is not the preceding step.
     */
    bool op_find_contours(
        VisionGpuContexts& contexts,
        const Kinesis::Vision::FindContoursParams& p);

    /**
     * @brief Track the previous frame's keypoints into the current frame.
     *
     * Adjacency mirrors ConnectedComponents into FindContours: ExtractPeaks
     * must immediately precede, and its work is folded into this step so the
     * detections are written straight into the flow context's buffers. The
     * whole step is one dependency sequence: peaks, one Lucas-Kanade dispatch
     * per pyramid level from coarse to fine, and the selection phases that
     * build the next point list. Tracks persist: survivors carry over with
     * their id and age, and new detections fill only the freed capacity,
     * strongest first, one per grid cell. The first frame after a reset runs
     * peaks and selection only.
     *
     * @return True when the step was deferred and the run must suspend.
     */
    bool op_track_keypoints(VisionGpuContexts& contexts, const Kinesis::Vision::VisionStep& step);

    /**
     * @brief Dense optical flow from the previous frame to the current one.
     *
     * Needs an earlier RgbaToGray step, whose gray frame the pyramid hook has
     * already turned into the current atlas. The whole solve is one dependency
     * sequence: for each level from coarsest to finest, the previous frame's
     * structure tensor and then a number of warp, box sum and solve
     * iterations. The flow of a finer level starts from the coarser one
     * upsampled and doubled, seeded inside the first warp of the level.
     *
     * The first frame after a reset has no previous frame, so it only ends
     * the frame and produces no flow.
     *
     * @return True when the step was deferred and the run must suspend.
     */
    bool op_dense_flow(VisionGpuContexts& contexts, const Kinesis::Vision::VisionStep& step);

    /**
     * @brief Build the current frame's pyramid atlas from the working gray image.
     *
     * Submitted as one un-awaited dependency sequence, one fused dispatch per
     * level. The level 0 stage carries a hazard on the gray image, which
     * orders any later dispatch that overwrites it after this read. The
     * fence is reaped on the next fresh run and in reset().
     */
    static void build_flow_pyramid(VisionGpuContexts& contexts, uint32_t requested_levels);

    /**
     * @brief Run any work a finished step owes the flow context.
     *
     * The step that produces the gray image feeds it to the flow context
     * before the next pixel dispatch overwrites it, when a TrackKeypoints or
     * OpticalFlowDense step lies ahead. Sequences without one never take this
     * branch, and when both are present the pyramid gets the larger level
     * count.
     */
    static void after_step(VisionGpuContexts& contexts, size_t index);
};

} // namespace MayaFlux::Yantra
