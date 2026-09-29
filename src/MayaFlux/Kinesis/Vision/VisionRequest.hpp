#pragma once

#include "VisionContext.hpp"
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
 * @brief Selects a VisionResult field to seed a later ResolvedSequence's
 *        image argument with, in place of the request's own raw frame.
 *
 * A plain function pointer, not std::function: resolve() picks one once per
 * request, the caller calls it once per sequence run. No allocation, no
 * lookup, matching ImageOutputs' own "direct field, not a keyed container"
 * shape.
 */
using SeedAccessor = std::shared_ptr<Core::VKImage> (*)(const VisionResult&);

/**
 * @brief One VisionSequence to run, plus where its image argument comes from.
 *
 * seed_field null means seed from the request's own raw frame: true for the
 * first entry always, and for any later entry that shares nothing
 * image-producing with it. Non-null means run sequences[seed_lane] first,
 * call seed_field on its VisionResult, and pass that in place of the raw
 * frame; the leading steps that produced it are already stripped from
 * sequence itself, so paying for them again would be pure repeated work for
 * an identical result.
 */
struct ResolvedSequence {
    VisionSequence sequence;
    size_t seed_lane { 0 };
    SeedAccessor seed_field { nullptr };
};

/**
 * @brief Resolve a VisionRequest into the ResolvedSequences needed to
 *        satisfy every active intent, sharing whatever leading work two
 *        active intents' chains turn out to need in common.
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
 * The first entry always seeds from the raw frame. Every later entry is
 * compared against the first entry's own chain, leading step by leading
 * step (VisionOp and params, via hash_vision_step), for as long as each
 * matching step also has a named VisionResult field to hand its output off
 * through (VisionResult::gray for RgbaToGray, VisionResult::images.* for
 * every other image-producing op) - a match stops at the first divergence
 * or the first step with nothing to seed from (ExtractPeaks, or any other
 * structured/buffer-only op), never partway through a still-matching image
 * op. Only TrackObjects and DetectFeatures currently share anything beyond
 * RgbaToGray (both run HarrisResponse then ExtractPeaks with their own,
 * independently settable params), but nothing here is specific to that
 * pair: any future chain sharing a longer or different leading run gets the
 * same treatment for free.
 *
 * A caller does not thread anything by hand: seed_lane/seed_field on each
 * entry say exactly which prior result's which field to pass as the image
 * argument, or to use the request's own raw frame when both are default.
 *
 * @param request Active intents and their parameters.
 * @return Ordered ResolvedSequences to run against the same frame; empty
 *         when no intent in request.intents has a populated parameter
 *         field.
 */
[[nodiscard]] std::vector<ResolvedSequence> resolve(const VisionRequest& request);

} // namespace MayaFlux::Kinesis::Vision
