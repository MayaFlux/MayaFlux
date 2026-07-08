#pragma once

#include "ConnectedComponents.hpp"
#include "Features.hpp"
#include "Gradient.hpp"
#include "OpticalFlow.hpp"
#include "VisionOp.hpp"

#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class VKImage;
}

/**
 * @file VisionExecutor.hpp
 * @brief Dispatch engine for VisionSequence execution.
 *
 * VisionExecutor::run() is the single entry point for executing a
 * VisionSequence. It maintains per-frame state (previous gray frame for
 * optical flow) and returns a VisionResult carrying both the final pixel
 * image and any structured outputs produced by the terminal step.
 *
 * Scratch storage is owned by the executor as DataVariant slots holding
 * vector<float>. Steps ping-pong between slots via Eigen::Map<ArrayXf>
 * views, accumulating zero heap allocation in steady state at a fixed
 * resolution. Gaussian kernels are cached by sigma across frames.
 *
 * VisionResult::pixel_image is a DataVariant (vector<float>) moved out of
 * a scratch slot. Callers read it via EigenAccess::view<Eigen::VectorXf>()
 * for zero-copy Eigen access, or as_span() for raw float access.
 */

namespace MayaFlux::Kinesis::Vision {

using StructuredOutput = std::variant<
    std::monostate,
    GradientResult,
    ComponentResult,
    std::vector<Contour>,
    std::vector<Keypoint>,
    std::vector<TrackResult>>;

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
    uint32_t w { 0 };
    uint32_t h { 0 };

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
 * @class VisionExecutor
 * @brief Stateful executor for a VisionSequence.
 *
 * Owns a pool of DataVariant scratch slots (each holding vector<float>) sized
 * to the working resolution. Steps read via Eigen::Map<const ArrayXf> and
 * write via Eigen::Map<ArrayXf> into the next slot, then swap indices.
 * No heap allocation occurs in steady state after the first frame at a given
 * resolution.
 *
 * Gaussian kernels are cached by sigma (keyed on bit-exact float) and reused
 * across frames and across the three structure tensor smoothing passes in Harris.
 *
 * One executor instance per pipeline. Not thread-safe.
 */
class MAYAFLUX_API VisionExecutor {
public:
    VisionExecutor() = default;

    /**
     * @brief Execute a VisionSequence on one frame.
     *
     * @param sequence Ordered steps to execute.
     * @param frame    Normalised float input. RGBA (4 floats/pixel) for
     *                 RgbaToGray/RgbaToHsv; single-channel otherwise.
     * @param w        Frame width in pixels.
     * @param h        Frame height in pixels.
     * @return         VisionResult with pixel_image (DataVariant) and/or structured output.
     */
    [[nodiscard]] VisionResult run(
        const VisionSequence& sequence,
        std::span<const float> frame,
        uint32_t w, uint32_t h);

    /**
     * @brief Clear stored inter-frame state.
     *
     * Call when the pixel source changes (camera switch, video seek) so the
     * next track_keypoints step starts clean. Does not release scratch storage.
     */
    void reset();

private:
    // =========================================================================
    // Scratch pool
    //
    // Each slot is a DataVariant holding vector<float>. Slots are accessed via
    // slot_vec(i) for direct vector reference and slot_map(i) / slot_map_mut(i)
    // for zero-copy Eigen views. The working resolution is tracked to detect
    // geometry changes and resize all slots in one pass.
    //
    // Slot assignment (fixed):
    //   0  current working buffer (ping)
    //   1  next working buffer (pong)
    //   2  filter horizontal pass tmp
    //   3  harris: dx
    //   4  harris: dy
    //   5  harris: ixx
    //   6  harris: iyy
    //   7  harris: ixy
    //   8  harris: sxx (smoothed)
    //   9  harris: syy
    //  10  harris: sxy
    // =========================================================================

    static constexpr size_t k_slot_count = 11;
    static constexpr size_t k_slot_cur = 0;
    static constexpr size_t k_slot_nxt = 1;
    static constexpr size_t k_slot_tmp = 2;
    static constexpr size_t k_slot_dx = 3;
    static constexpr size_t k_slot_dy = 4;
    static constexpr size_t k_slot_ixx = 5;
    static constexpr size_t k_slot_iyy = 6;
    static constexpr size_t k_slot_ixy = 7;
    static constexpr size_t k_slot_sxx = 8;
    static constexpr size_t k_slot_syy = 9;
    static constexpr size_t k_slot_sxy = 10;

    std::array<Kakshya::DataVariant, k_slot_count> m_slots;
    uint32_t m_slot_w { 0 };
    uint32_t m_slot_h { 0 };

    /**
     * @brief Ensure all slots are sized to n_pixels floats.
     *
     * No-op when geometry matches. Resizes and zero-fills all slots otherwise.
     */
    void ensure_slots(uint32_t w, uint32_t h);

    /**
     * @brief Mutable reference to the vector<float> inside slot i.
     */
    [[nodiscard]] std::vector<float>& slot_vec(size_t i) noexcept
    {
        return std::get<std::vector<float>>(m_slots[i]);
    }

    /**
     * @brief Const reference to the vector<float> inside slot i.
     */
    [[nodiscard]] const std::vector<float>& slot_vec(size_t i) const noexcept
    {
        return std::get<std::vector<float>>(m_slots[i]);
    }

    /**
     * @brief Zero-copy read-only Eigen::Map<const ArrayXf> over slot i.
     */
    [[nodiscard]] Eigen::Map<const Eigen::ArrayXf> slot_map(size_t i, Eigen::Index n) const noexcept
    {
        return { slot_vec(i).data(), n };
    }

    /**
     * @brief Zero-copy mutable Eigen::Map<ArrayXf> over slot i.
     */
    [[nodiscard]] Eigen::Map<Eigen::ArrayXf> slot_map_mut(size_t i, Eigen::Index n) noexcept
    {
        return { slot_vec(i).data(), n };
    }

    // =========================================================================
    // Gaussian kernel cache
    //
    // Keyed on the bit pattern of the float sigma value. A given sigma produces
    // an identical kernel every time; recomputing it three times per Harris call
    // per frame is pure waste. The 1D separable kernel is stored once and reused
    // for all three structure tensor smoothing passes and for standalone
    // GaussianBlur steps.
    // =========================================================================

    std::unordered_map<uint32_t, std::vector<float>> m_kernel_cache;

    /**
     * @brief Return a reference to the precomputed 1D Gaussian kernel for sigma.
     *
     * Computes and caches on first call for a given sigma. The kernel is
     * normalised to unit sum and has length 2*ceil(3*sigma)+1.
     */
    [[nodiscard]] const std::vector<float>& gaussian_kernel(float sigma);

    // =========================================================================
    // Inter-frame state
    // =========================================================================

    Kakshya::DataVariant m_prev_gray { std::vector<float> {} };
    std::vector<float> m_curr_gray_cache;
    std::vector<Keypoint> m_prev_keypoints;
};

} // namespace MayaFlux::Kinesis::Vision
