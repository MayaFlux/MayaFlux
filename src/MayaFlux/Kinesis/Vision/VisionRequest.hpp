#pragma once

#include "VisionIntent.hpp"
#include "VisionOp.hpp"

/**
 * @file VisionRequest.hpp
 * @brief Per-intent parameters and resolution into a set of VisionSequences.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Parameters for VisionIntent::FindElements.
 */
struct FindElementsRequest {
    ConnectedComponentsParams components {};
    FindContoursParams contours {};
};

/**
 * @brief Parameters for VisionIntent::TrackObjects.
 */
struct TrackObjectsRequest {
    HarrisParams harris {};
    ExtractPeaksParams peaks {};
    TrackKeypointsParams track {};
};

/**
 * @brief Parameters for VisionIntent::DetectFeatures.
 */
struct DetectFeaturesRequest {
    HarrisParams harris {};
    ExtractPeaksParams peaks {};
};

/**
 * @brief Parameters for VisionIntent::DetectEdges.
 */
struct DetectEdgesRequest {
    CannyParams canny {};
};

/**
 * @brief Parameters for VisionIntent::EstimateMotion.
 */
struct EstimateMotionRequest {
    OpticalFlowDenseParams flow {};
};

/**
 * @brief Parameters for VisionIntent::MeasureAppearance.
 *
 * Empty: its VisionOp (Sobel) takes no parameters. Present as a named type
 * rather than a bare flag so VisionRequest's per-intent fields stay uniform.
 */
struct MeasureAppearanceRequest { };

/**
 * @brief Composed request: which VisionIntents are active, and each active
 *        one's parameters.
 *
 * A field is read by resolve() only when its VisionIntent bit is set in
 * intents; an unset bit's field is ignored even if populated, and a set bit
 * with no populated field resolves to nothing for that intent.
 */
struct VisionRequest {
    VisionIntent intents { VisionIntent::NONE };

    std::optional<FindElementsRequest> find_elements;
    std::optional<TrackObjectsRequest> track_objects;
    std::optional<DetectFeaturesRequest> detect_features;
    std::optional<DetectEdgesRequest> detect_edges;
    std::optional<EstimateMotionRequest> estimate_motion;
    std::optional<MeasureAppearanceRequest> measure_appearance;
};

/**
 * @brief Resolve a VisionRequest into the VisionSequences needed to satisfy
 *        every active intent.
 *
 * TrackObjects and EstimateMotion, when both active, resolve into one
 * sequence: VisionGpuExecutor::after_step already builds one shared flow
 * pyramid for whichever of TrackKeypoints/OpticalFlowDense is ahead, so
 * they share a tail rather than forking. Every other active intent gets its
 * own sequence, since each one's chain overwrites the single working-image
 * slot a VisionSequence walk carries, and nothing lets two chains diverge
 * from a shared point and both continue (VisionOp::Snapshot is a one-way
 * output tap, not a restore point).
 *
 * Only the first returned VisionSequence begins with RgbaToGray. Every
 * VisionGpuExecutor::run() call re-ingests its image argument and clears
 * its per-run memo unconditionally (GpuVisionPass::begin), so nothing
 * carries between separate run() calls automatically. The caller is
 * responsible for running the first sequence, then passing its result's
 * VisionResult::gray (not the original frame) as the image argument to
 * run() for every subsequent sequence in the returned list. gray is a
 * storage image, so op_ingest passes it straight through with no
 * re-conversion, and RgbaToGray is not paid for twice.
 *
 * @param request Active intents and their parameters.
 * @return Ordered VisionSequences to run against the same frame; empty when
 *         no intent in request.intents has a populated parameter field.
 */
[[nodiscard]] std::vector<VisionSequence> resolve(const VisionRequest& request);

} // namespace MayaFlux::Kinesis::Vision
