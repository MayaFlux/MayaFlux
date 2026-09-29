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

    float mean_speed_px { 0.0F };
    float mean_age { 0.0F };
    uint32_t max_age { 0 };
    uint32_t tracked_count { 0 };
    uint32_t lost_count { 0 };
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
    float mean_response { 0.0F };
};

/**
 * @brief Per-element shape descriptors, parallel to FindElementsAnalysis::boxes
 *        (shapes[i] describes boxes[i]).
 *
 * compactness is 4*pi*area/perimeter^2 in pixel units (1.0 for a perfect
 * circle, lower for elongated or irregular shapes). aspect_ratio is the
 * element's own bounding box width/height in pixel units.
 */
struct ElementShape {
    float compactness { 0.0F };
    float aspect_ratio { 0.0F };
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
 *
 * shapes and nearest_neighbor_distance_px are both parallel to boxes (one
 * entry per real component, holes excluded): shapes[i] is boxes[i]'s own
 * compactness/aspect ratio, nearest_neighbor_distance_px[i] is the pixel
 * distance from boxes[i]'s centroid to the closest other box's centroid
 * (0 when boxes has fewer than two elements).
 */
struct FindElementsAnalysis {
    std::vector<Contour> contours;
    std::vector<BoundingBox> boxes;
    std::vector<ElementShape> shapes;
    std::vector<float> nearest_neighbor_distance_px;
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

/**
 * @brief MeasureAppearance output: whole-frame photometric characterisation,
 *        computed on GPU (region_sample.comp over VisionResult::images.sobel,
 *        and a 256-bin brightness histogram over VisionResult::gray).
 *
 * mean_brightness is the histogram's own weighted mean, in [0, 1].
 */
struct MeasureAppearanceAnalysis {
    FieldSample gradient;
    std::array<uint32_t, 256> histogram {};
    float mean_brightness { 0.0F };
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
 * detect_edges/estimate_motion have no derived field of their own beyond
 * motion_activity: VisionExtractor::sample() answers "what is it like here"
 * for a region a caller supplies, on demand.
 */
struct VisionAnalysis {
    std::optional<FindElementsAnalysis> find_elements;
    std::optional<TrackObjectsAnalysis> track_objects;
    std::optional<DetectFeaturesAnalysis> detect_features;
    std::optional<MeasureAppearanceAnalysis> measure_appearance;
    std::shared_ptr<Core::VKImage> detect_edges;
    std::shared_ptr<Core::VKImage> estimate_motion;

    /** @brief VisionResult::motion_energy from an active EstimateMotion sequence. */
    std::optional<float> motion_activity;
};

} // namespace MayaFlux::Kinesis::Vision
