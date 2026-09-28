#pragma once

#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Kinesis/Vision/VisionRequest.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "MayaFlux/Yantra/FunctionalOperation.hpp"

/**
 * @file VisionAnalyzer.hpp
 * @brief One stop shop: given a VisionRequest, a VisionAnalysisContext, and
 *        a frame, resolves the request, runs it, and runs the actual
 *        analysis on GPU (track_reduce.comp), returning a finished
 *        Kinesis::Vision::VisionAnalysis directly.
 *
 * run() is the real entry point: intent and params as actual function
 * parameters, not smuggled through Datum metadata. The ComputeOperation
 * path (run_operation, inherited from FunctionalOperation) is a thin
 * adapter underneath for graph compatibility. It pulls the same
 * parameters out of Datum metadata and calls run(); the work happens
 * inside run(), not here.
 */

namespace MayaFlux::Yantra {

/**
 * @class VisionAnalyzer
 * @brief FunctionalOperation<vector<DataVariant>, vector<DataVariant>>
 *        whose real API is run(): resolve the request, run every resolved
 *        VisionSequence against a persistent VisionGpuExecutor, then run
 *        track_reduce.comp (a real GPU reduction shader) over the
 *        (context-filtered) subject tracks for centroid/velocity/bounds.
 *
 * Owns its VisionGpuExecutor for the same reason VisionProcessor does:
 * cross-frame state (retained optical flow atlases and detections) lives
 * on the executor's contexts and must survive between calls.
 *
 * request/context are call parameters, not stored configuration: resolve()
 * is cheap (pure CPU, no GPU work), so there is nothing to cache by
 * pre-configuring the request ahead of time.
 */
class MAYAFLUX_API VisionAnalyzer
    : public FunctionalOperation<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>> {
public:
    using Base = FunctionalOperation<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>>;

    VisionAnalyzer();

    /**
     * @brief Resolve request, run it, and run the GPU analysis. The real
     *        entry point.
     *
     * @param request Which intents to satisfy and their run parameters.
     * @param context Caller-supplied subject filtering, applied after run.
     * @param image   GPU image in eShaderReadOnlyOptimal layout.
     * @param w       Frame width in pixels.
     * @param h       Frame height in pixels.
     */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis run(
        const Kinesis::Vision::VisionRequest& request,
        const Kinesis::Vision::VisionAnalysisContext& context,
        const std::shared_ptr<Core::VKImage>& image,
        uint32_t w, uint32_t h);

    /**
     * @brief Abandon outstanding work and clear retained executor state.
     *
     * Call when the pixel source changes (camera switch, video seek).
     */
    void reset();

private:
    /**
     * @brief ComputeOperation adapter: pulls request/context/width/height
     *        out of Datum metadata, uploads input.data[0], calls run().
     *        See run() for the real API.
     */
    output_type run_operation(const input_type& input);

    /**
     * @brief Dispatch track_reduce.comp over tracks and fill
     *        centroid/velocity/bounds on the result. No-op (default
     *        fields) when tracks is empty.
     */
    [[nodiscard]] Kinesis::Vision::TrackObjectsAnalysis reduce_tracks(
        const std::vector<Kinesis::Vision::TrackResult>& tracks);

    /**
     * @brief Dispatch track_reduce.comp over keypoint positions (fed as
     *        their own previous, so velocity reduces to zero and is simply
     *        not surfaced) and fill centroid/bounds on the result. No-op
     *        (default fields) when keypoints is empty. Same shader as
     *        reduce_tracks, not a duplicate: a keypoint has no persistent
     *        identity or previous position of its own, but its bounds and
     *        centroid are the same min/max/sum reduction either way.
     */
    [[nodiscard]] Kinesis::Vision::DetectFeaturesAnalysis reduce_keypoints(
        const std::vector<Kinesis::Vision::Keypoint>& keypoints);

    std::unique_ptr<VisionGpuExecutor> m_executor;
    std::shared_ptr<ShaderExecutionContext<>> m_track_reducer;

    std::shared_ptr<Core::VKImage> m_upload_image;
    uint32_t m_upload_w { 0 };
    uint32_t m_upload_h { 0 };
};

} // namespace MayaFlux::Yantra
