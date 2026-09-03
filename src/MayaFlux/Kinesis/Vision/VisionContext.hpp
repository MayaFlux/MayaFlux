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
 *   EigenAccess(result.pixel_image).view<Eigen::VectorXf>()  -- zero-copy Eigen map
 *   std::get<std::vector<float>>(result.pixel_image)          -- direct vector access
 *
 * w and h are the dimensions of pixel_image. Both are 0 when pixel_image is empty.
 */
struct VisionResult {
    Kakshya::DataVariant pixel_image { std::vector<float> {} };
    StructuredOutput structured { std::monostate {} };
    std::vector<SnapshotEntry> snapshots;
    std::shared_ptr<Core::VKImage> debug_labels;
    std::shared_ptr<Core::VKImage> debug_contours;
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
