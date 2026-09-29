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
 * @brief Colour representation FindElements segments in, which decides the
 *        conversion its chain leads with.
 *
 * Gray leads with RgbaToGray, Hsv with RgbaToHsv (channels are then hue,
 * saturation, value), Rgba with no conversion at all.
 */
enum class SegmentationSpace : uint8_t {
    Gray,
    Rgba,
    Hsv,
};

/**
 * @brief Threshold applied to the converted frame. Multichannel selection
 *        and bands live in the params themselves (ChannelMask, ChannelBand).
 */
using SegmentationMethod = std::variant<OtsuParams, ThresholdAdaptiveParams, ThresholdParams>;

/**
 * @brief How FindElements separates foreground from background. The default
 *        is grayscale Otsu.
 */
struct SegmentationRequest {
    SegmentationSpace space { SegmentationSpace::Gray };
    SegmentationMethod method { OtsuParams {} };
};

/**
 * @brief Parameters for VisionIntent::FindElements.
 */
struct FindElementsRequest {
    ConnectedComponentsParams components {};
    FindContoursParams contours {};
    SegmentationRequest segmentation {};
};

/**
 * @brief Parameters for VisionIntent::TrackObjects.
 *
 * region confines detection and tracking to a normalised rectangle by
 * leading the chain with Confine. Results are remapped to full-frame
 * coordinates on the GPU before they reach VisionAnalysis. When
 * EstimateMotion is also active it shares this lane, so its flow image
 * covers the same region (VisionAnalysis::motion_region). Changing region
 * between calls resets the analyzer's flow state, since flow compared across
 * different content is meaningless.
 */
struct TrackObjectsRequest {
    HarrisParams harris {};
    ExtractPeaksParams peaks {};
    TrackKeypointsParams track {};
    std::optional<BoundingBox> region;
};

/**
 * @brief Parameters for VisionIntent::DetectFeatures.
 *
 * region confines detection to a normalised rectangle by leading the chain
 * with Confine. Keypoints are remapped to full-frame coordinates on the GPU.
 */
struct DetectFeaturesRequest {
    HarrisParams harris {};
    ExtractPeaksParams peaks {};
    std::optional<BoundingBox> region;
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
 * @brief Picks a VisionResult field to seed a later sequence's image
 *        argument with, instead of the request's own raw frame.
 */
using SeedAccessor = std::shared_ptr<Core::VKImage> (*)(const VisionResult&);

/**
 * @brief One VisionSequence to run, and where its image argument comes from.
 *
 * seed_field null means seed from the request's own raw frame. Otherwise,
 * run sequences[seed_lane] first and pass seed_field(its result) as the
 * image argument in place of the raw frame.
 */
struct ResolvedSequence {
    VisionSequence sequence;
    size_t seed_lane { 0 };
    SeedAccessor seed_field { nullptr };
};

/**
 * @brief Resolve a VisionRequest into the ResolvedSequences needed to
 *        satisfy every active intent.
 *
 * TrackObjects and EstimateMotion, when both active, resolve into one
 * sequence sharing a tail; neither can be split back apart afterward.
 * Every other active intent gets its own sequence, seeded from whichever
 * leading steps it has in common with the first sequence (matched by
 * VisionOp and params) instead of recomputing them.
 *
 * @param request Active intents and their parameters.
 * @return Ordered ResolvedSequences to run against the same frame; empty
 *         when no intent in request.intents has a populated parameter
 *         field.
 */
[[nodiscard]] std::vector<ResolvedSequence> resolve(const VisionRequest& request);

} // namespace MayaFlux::Kinesis::Vision
