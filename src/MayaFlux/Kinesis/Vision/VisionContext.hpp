#pragma once

#include "ConnectedComponents.hpp"
#include "Features.hpp"
#include "Gradient.hpp"
#include "OpticalFlow.hpp"
#include "VisionOp.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Portal::Graphics {
struct GpuBufferHandle;
}
namespace MayaFlux::Kinesis::Vision {

using StructuredOutput = std::variant<
    std::monostate,
    GradientResult,
    ComponentResult,
    std::vector<Contour>,
    std::vector<Keypoint>,
    std::vector<TrackResult>>;

/**
 * @brief Whether a run carried the sequence to its end.
 *
 * SUSPENDED means a deferred step has work outstanding. The result carries
 * nothing and must not be consumed. Call run again with the same arguments
 * to poll; the executor resumes where it left off and ignores the image
 * argument until the sequence completes.
 */
enum class VisionStatus : uint8_t {
    COMPLETE,
    SUSPENDED,
};

/**
 * @brief Result of executing a VisionSequence on one frame.
 *
 * pixel_image holds the final normalised float pixel buffer as a DataVariant
 * (active alternative: vector<float>). Empty when the terminal step produces
 * only structured output.
 *
 * Callers access pixel data via:
 *   EigenAccess(result.pixel_image).view<Eigen::VectorXf>(): zero-copy Eigen map
 *   std::get<std::vector<float>>(result.pixel_image): direct vector access
 *
 * w and h are the dimensions of pixel_image. Both are 0 when pixel_image is empty.
 */
struct VisionResult {
    Kakshya::DataVariant pixel_image { std::vector<float> {} };
    StructuredOutput structured { std::monostate {} };
    std::vector<SnapshotEntry> snapshots;
    std::shared_ptr<Core::VKImage> debug_labels;
    std::shared_ptr<Core::VKImage> debug_contours;

    /**
     * @brief Dense optical flow field from OpticalFlowDense, or null.
     *
     * RGBA32F at the resolution of the tracked frame: r and g are the flow in
     * pixels, b the confidence, a the residual.
     */
    std::shared_ptr<Core::VKImage> flow;

    /**
     * @brief Output of a RgbaToGray step in this sequence, or null.
     *
     * A storage image, so passing it as the image argument to a later
     * VisionGpuExecutor::run() call skips re-ingesting and re-converting:
     * op_ingest passes any frame already carrying storage usage through
     * unchanged. Intended for a caller resolving one VisionRequest into
     * several VisionSequences that all start from the same gray frame, so
     * only the first pays for RgbaToGray.
     */
    std::shared_ptr<Core::VKImage> gray;

    /**
     * @brief Device resident tracks from TrackKeypoints with export_tracks,
     *        or null.
     *
     * One vec4 header followed by two vec4 per track. The header holds the
     * track count as uint bits in x. Track record i is
     * (position.xy, previous.xy) then (error, tracked, id bits, age bits),
     * the fields of TrackResult, in the same order as the host result. The
     * executor owns the memory: the view stays valid for one more run before
     * its buffer is rewritten, and never outlives the executor.
     */
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> tracks_buffer;
    uint32_t w { 0 };
    uint32_t h { 0 };
    VisionStatus status { VisionStatus::COMPLETE };
    size_t suspended_at { 0 };

    /**
     * @brief True when the sequence reached its end and this result may be
     *        consumed, cached, or broadcast.
     */
    [[nodiscard]] bool is_ready() const noexcept
    {
        return status == VisionStatus::COMPLETE;
    }

    /**
     * @brief Zero-copy float span into pixel_image storage.
     * @return Empty span if pixel_image is not vector<float> or is empty.
     */
    [[nodiscard]] std::span<const float> as_span() const noexcept
    {
        const auto* v = std::get_if<std::vector<float>>(&pixel_image);
        if (!v || v->empty())
            return {};
        return { v->data(), v->size() };
    }
};

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
 * @brief Composed analysis output: one populated field per active intent
 *        that produced a result this call.
 *
 * find_elements/detect_features are the existing GPU-produced result type
 * directly: VisionResult::structured's content, routed by intent.
 * detect_edges/estimate_motion are exactly VisionResult::debug_labels/flow.
 *
 * find_elements arrives already sorted by area, largest first, when the
 * caller set FindContoursParams::max_contours > 0 on the request: that cap
 * is driven by contour_topk_select.comp, a real GPU top-K-by-area
 * reduction already in the FindContours pipeline, so "the largest
 * contour" is find_elements->front() for a caller who asked for one, with
 * no separate derived field needed.
 *
 * track_objects is TrackObjectsAnalysis, not a bare vector: its
 * centroid/velocity/bounds are GPU-computed (see track_reduce.comp), the
 * one intent with a real reduction shader today. detect_features has no
 * equivalent GPU-computed reduction (strongest-by-response) yet.
 *
 * detect_edges/estimate_motion have no derived field: nothing in the GPU
 * dispatch path reduces or downloads their content. MeasureAppearance has
 * no field at all: its VisionOp (Sobel) writes its output only to the
 * working image via the generic dispatch path, which VisionGpuExecutor
 * does not surface through any VisionResult field.
 */
struct VisionAnalysis {
    std::optional<std::vector<Contour>> find_elements;
    std::optional<TrackObjectsAnalysis> track_objects;
    std::optional<std::vector<Keypoint>> detect_features;
    std::shared_ptr<Core::VKImage> detect_edges;
    std::shared_ptr<Core::VKImage> estimate_motion;
};

/**
 * @brief State threaded through one execution of a VisionSequence.
 *
 * Shared by the CPU and GPU executors so op functions have one signature on
 * both paths. Three bands: the walk, working storage, and redundancy caches.
 *
 * Cross-run retained state is not here. It is owned vectors on the CPU path
 * and owned images on the GPU path, neither of which is a working storage
 * handle, so each executor holds its own in its own types.
 *
 * @tparam Handle Working storage handle. Slot index on the CPU path,
 *                shared_ptr<VKImage> on the GPU path.
 *
 * sequence is non-owning and valid only for the run that constructed the
 * pass. Storage and caches outlive a single run once the pass is held by
 * the executor; the walk band is reset per run by begin().
 */
template <typename Handle>
struct VisionPass {
    // -------------------------------------------------------------------------
    // Walk
    // -------------------------------------------------------------------------

    const VisionSequence* sequence { nullptr };
    size_t index { 0 };
    uint32_t w { 0 };
    uint32_t h { 0 };
    uint32_t channels { 4 };
    VisionResult result;

    // -------------------------------------------------------------------------
    // Working storage
    // -------------------------------------------------------------------------

    Handle current {};
    uint32_t storage_w { 0 };
    uint32_t storage_h { 0 };

    // -------------------------------------------------------------------------
    // Retained across runs
    // -------------------------------------------------------------------------

    Handle prev {};
    Handle prev_cache {};
    std::vector<Keypoint> prev_keypoints;

    // -------------------------------------------------------------------------
    // Redundancy caches
    // -------------------------------------------------------------------------

    struct Completed {
        Handle output {};
        Handle input {};
    };

    std::unordered_map<size_t, Completed> completed;

    /**
     * @brief Reset the walk band for a fresh run.
     *
     * Clears the memo unconditionally: its keys compare image handles, which
     * proxy for content only within one walk. Across runs the caller's input
     * and the contexts' output images are reused in place, so an equal handle
     * does not mean equal pixels.
     *
     * Retained cross-run state is dropped only when the geometry changes,
     * since surviving between runs is what it is for.
     */
    void begin(const VisionSequence& seq, uint32_t width, uint32_t height)
    {
        completed.clear();

        if (width != w || height != h)
            forget();

        sequence = &seq;
        index = 0;
        channels = 4;
        result = VisionResult {};
        set_geometry(width, height);
    }

    /**
     * @brief Discard retained cross-run state. Storage and caches are kept.
     */
    void forget()
    {
        prev = Handle {};
        prev_cache = Handle {};
        prev_keypoints.clear();
    }

    [[nodiscard]] const VisionStep& step() const noexcept
    {
        return sequence->steps[index];
    }

    [[nodiscard]] size_t plane_size() const noexcept
    {
        return static_cast<size_t>(w) * h;
    }

    /**
     * @brief Op at @p offset steps ahead, or nullptr past the end.
     *
     * Replaces the adjacency booleans derived by VisionSequence::Builder.
     * Correct under any mutation of steps because it is evaluated at the
     * point of use.
     */
    [[nodiscard]] const VisionStep* ahead(size_t offset = 1) const noexcept
    {
        const auto& steps = sequence->steps;
        const size_t at = index + offset;
        return at < steps.size() ? &steps[at] : nullptr;
    }

    /**
     * @brief Op at @p offset steps back, or nullptr before the start.
     *
     * Counterpart to ahead(), for ops validating what produced their input.
     */
    [[nodiscard]] const VisionStep* behind(size_t offset = 1) const noexcept
    {
        if (offset > index)
            return nullptr;
        return &sequence->steps[index - offset];
    }

    void set_geometry(uint32_t width, uint32_t height) noexcept
    {
        w = width;
        h = height;
        result.w = width;
        result.h = height;
    }

    /**
     * @brief Memoised output for @p key when it was produced from @p input.
     */
    [[nodiscard]] const Handle* memo(size_t key, const Handle& input) const
    {
        auto it = completed.find(key);
        if (it == completed.end() || !(it->second.input == input))
            return nullptr;
        return &it->second.output;
    }
};

using CpuVisionPass = VisionPass<size_t>;
using GpuVisionPass = VisionPass<std::shared_ptr<Core::VKImage>>;

} // namespace MayaFlux::Kinesis::Vision
