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
 * @brief Live device view of FindContours' traced contours, when
 *        FindContoursParams::export_contours_buffer is set.
 *
 * meta is one uvec4 per contour (point_offset, point_count, parent_label,
 * label_id), points is the flat point array, area_perim is one vec2
 * (area, perimeter) per contour. Decode via
 * VisionGpuExecutor::read_exported_contours(). Default constructed (all
 * null, count 0) when not requested. Valid for one more run before the
 * executor reuses it.
 */
struct ContoursBufferView {
    uint32_t count { 0 };
    uint32_t points_written { 0 };
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> meta;
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> points;
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> area_perim;
};

/**
 * @brief Live device views of the GPU-resident export buffers, one named
 *        slot per exporting op, populated only when its own export flag was
 *        set. Null/default otherwise. The executor owns the memory: each
 *        view is valid for one more run before its buffer is reused.
 *
 * tracks/keypoints decode via VisionGpuExecutor::read_exported_tracks()/
 * read_exported_keypoints(). labels is ConnectedComponents' per-pixel
 * compact label map (one uint32 per pixel, 0 background, 1..count
 * foreground, matching component_boxes/Contour::label_id), consumed via
 * VisionGpuExecutor::select_label() rather than decoded to a host list.
 * contours decodes via read_exported_contours(); see ContoursBufferView.
 */
struct BufferOutputs {
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> tracks;
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> keypoints;
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> labels;
    ContoursBufferView contours;
};

/**
 * @brief One image slot per VisionOp that produces an image, populated
 *        only for whichever of these ops ran in the sequence. Null means
 *        the op did not run.
 *
 * RgbaToGray uses the separate top-level VisionResult::gray instead.
 * component_colors, contour_image, and flow_visualization come from
 * ConnectedComponents::with_colors, FindContours::as_image, and
 * OpticalFlowDense::visualize respectively.
 */
struct ImageOutputs {
    std::shared_ptr<Core::VKImage> rgba_to_hsv;
    std::shared_ptr<Core::VKImage> gray_to_rgba;
    std::shared_ptr<Core::VKImage> downsample_2x;
    std::shared_ptr<Core::VKImage> threshold;
    std::shared_ptr<Core::VKImage> threshold_adaptive;
    std::shared_ptr<Core::VKImage> threshold_otsu;
    std::shared_ptr<Core::VKImage> normalize_inplace;
    std::shared_ptr<Core::VKImage> normalize_range;
    std::shared_ptr<Core::VKImage> gaussian_blur;
    std::shared_ptr<Core::VKImage> filter_separable;
    std::shared_ptr<Core::VKImage> sobel;
    std::shared_ptr<Core::VKImage> scharr;
    std::shared_ptr<Core::VKImage> canny;
    std::shared_ptr<Core::VKImage> erode;
    std::shared_ptr<Core::VKImage> dilate;
    std::shared_ptr<Core::VKImage> open;
    std::shared_ptr<Core::VKImage> close;
    std::shared_ptr<Core::VKImage> morph_gradient;
    std::shared_ptr<Core::VKImage> harris_response;

    std::shared_ptr<Core::VKImage> component_colors; ///< ConnectedComponents::with_colors
    std::shared_ptr<Core::VKImage> contour_image; ///< FindContours::as_image
    std::shared_ptr<Core::VKImage> flow_visualization; ///< OpticalFlowDense::visualize
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
     * @brief One BoundingBox per component from a ConnectedComponents step
     *        in this sequence, label 1..count, or empty.
     *
     * Populated whenever ConnectedComponents ran, whether or not FindContours
     * immediately follows: the box computation is a small readback of
     * already GPU-reduced min/max buffers, not per-pixel work, so it costs
     * nothing extra to keep regardless of what the sequence does next.
     * label_id on each box matches Contour::label_id from a following
     * FindContours step exactly, letting a caller join a contour's polygon
     * to its own bounding box without re-deriving one from the polygon.
     */
    std::vector<BoundingBox> component_boxes;

    /**
     * @brief GPU-resident export buffers, one named slot per exporting op.
     *        See BufferOutputs.
     */
    BufferOutputs buffers;

    /**
     * @brief Image output of whichever image-producing ops ran in this
     *        sequence, one named slot per op. See ImageOutputs.
     */
    ImageOutputs images;

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
