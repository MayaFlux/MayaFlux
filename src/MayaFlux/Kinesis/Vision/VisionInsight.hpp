#pragma once

#include "VisionContext.hpp"

/**
 * @file VisionInsight.hpp
 * @brief Caller-supplied context and GPU-derived output for turning a raw
 *        VisionResult into something a caller actually wants: analysis
 *        today (VisionAnalysisContext/VisionAnalysis and their per-intent
 *        pieces), extraction context to follow. Split out of
 *        VisionContext.hpp, which stays scoped to running a VisionSequence
 *        itself, not to interpreting what came out of it.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Caller-supplied context for VisionIntent::TrackObjects.
 *
 * subject_bounds narrows which tracked points count as the subject when
 * more than one candidate is present in the raw TrackResult list, for the
 * frame that first identifies a subject. subject_track_id continues an
 * already-identified subject on later frames by its stable TrackResult::id
 * instead of re-filtering by region every call, which is the case
 * DetectFeaturesContext has no equivalent for: Keypoint carries no
 * persistent identity at all, only TrackResult does. Not derived either
 * way: the caller decides what the subject is, from whatever source (a
 * previous frame's result, a UI selection, another sensor).
 */
struct TrackObjectsContext {
    std::optional<BoundingBox> subject_bounds;
    std::optional<uint32_t> subject_track_id;
};

/**
 * @brief Composed context: caller-supplied data for whichever intents need
 *        it, consulted after a VisionRequest's resolved sequences have run.
 *
 * detect_features_bounds stands alone rather than in its own named
 * struct like TrackObjectsContext: Keypoint has no persistent identity to
 * grow a second field around the way TrackResult does, so a bare bounds
 * value is the whole story.
 *
 * FindElements/DetectEdges/EstimateMotion/MeasureAppearance have no field
 * here: FindElements's contours are already complete with nothing left for
 * the caller to supply, and the other three have no concrete
 * caller-suppliable need identified yet.
 */
struct VisionAnalysisContext {
    std::optional<TrackObjectsContext> track_objects;
    std::optional<BoundingBox> detect_features_bounds;
};

/**
 * @brief Track reduction, computed on GPU (Yantra's track_reduce.comp) from
 *        the (context-filtered) subject tracks: sum reduced to mean for
 *        centroid/velocity, min/max reduced to bounds. Not a CPU loop over
 *        the downloaded track list.
 */
struct TrackObjectsAnalysis {
    std::vector<TrackResult> tracks;
    glm::vec2 centroid {};
    glm::vec2 velocity {};
    BoundingBox bounds {};
};

/**
 * @brief Keypoint reduction, computed on GPU by the same reduction shader
 *        as TrackObjectsAnalysis (track_reduce.comp, fed previous = position
 *        so the velocity output is always zero and simply not surfaced
 *        here): centroid and bounds over every detected keypoint. Not a CPU
 *        min/max loop over the downloaded keypoint list.
 */
struct DetectFeaturesAnalysis {
    std::vector<Keypoint> keypoints;
    glm::vec2 centroid {};
    BoundingBox bounds {};
};

/**
 * @brief FindElements output: contours plus the per-component BoundingBoxes
 *        ConnectedComponents already computes on GPU as a matter of course.
 *
 * boxes is indexed by label 1..count (boxes[i] has label_id == i + 1),
 * independent of contours' order (contours may be reordered by
 * contour_topk_select.comp's area sort, or filtered by min_area). Join a
 * contour to its own box via contour.label_id: boxes[contour.label_id - 1].
 * A hole contour's label_id is 0 and has no corresponding box; a hole is
 * not itself a component.
 */
struct FindElementsAnalysis {
    std::vector<Contour> contours;
    std::vector<BoundingBox> boxes;
};

/**
 * @brief Composed analysis output: one populated field per active intent
 *        that produced a result this call.
 *
 * find_elements/detect_features are GPU-produced/GPU-reduced result types:
 * see FindElementsAnalysis and DetectFeaturesAnalysis respectively.
 * detect_edges/estimate_motion are exactly VisionResult::images.canny/flow.
 *
 * find_elements.contours arrives already sorted by area, largest first,
 * when the caller set FindContoursParams::max_contours > 0 on the request:
 * that cap is driven by contour_topk_select.comp, a real GPU top-K-by-area
 * reduction already in the FindContours pipeline, so "the largest
 * contour" is find_elements->contours.front() for a caller who asked for
 * one, with no separate derived field needed.
 *
 * track_objects/detect_features carry GPU-computed centroid/bounds (see
 * their own struct docs) rather than a bare vector: both are real reduction
 * shader dispatches, not CPU loops.
 *
 * detect_edges/estimate_motion have no derived field: nothing in the GPU
 * dispatch path reduces or downloads their content; VisionExtractor samples
 * them directly by region instead of Analyzer deriving a summary up front.
 * MeasureAppearance has no field at all: its VisionOp (Sobel) writes its
 * output only to the working image via the generic dispatch path, which
 * VisionGpuExecutor does not surface through any VisionResult field.
 */
struct VisionAnalysis {
    std::optional<FindElementsAnalysis> find_elements;
    std::optional<TrackObjectsAnalysis> track_objects;
    std::optional<DetectFeaturesAnalysis> detect_features;
    std::shared_ptr<Core::VKImage> detect_edges;
    std::shared_ptr<Core::VKImage> estimate_motion;
};

/**
 * @brief Per-channel mean/min/max over a sampled image region, computed on
 *        GPU (Yantra's region_sample.comp) rather than downloaded and
 *        reduced on the caller's side.
 *
 * The channel semantics depend entirely on what image was sampled: rgb for
 * a colour frame, r/g = flow vector with b = confidence and a = residual
 * for VisionAnalysis::estimate_motion, and so on. VisionExtractor does not
 * interpret the channels, only reduces them.
 */
struct FieldSample {
    glm::vec4 mean {};
    glm::vec4 min_val {};
    glm::vec4 max_val {};
};

} // namespace MayaFlux::Kinesis::Vision
