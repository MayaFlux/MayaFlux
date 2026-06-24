#pragma once

#include "ConnectedComponents.hpp"
#include "Features.hpp"
#include "MayaFlux/Kinesis/Vision/Gradient.hpp"
#include "OpticalFlow.hpp"
#include "VisionOp.hpp"

/**
 * @file VisionExecutor.hpp
 * @brief Dispatch engine for VisionSequence execution.
 *
 * VisionExecutor::run() is the single entry point for executing a
 * VisionSequence. It maintains per-frame state (previous gray frame for
 * optical flow) and returns a VisionResult carrying both the final pixel
 * image and any structured outputs produced by the terminal step.
 *
 * No MayaFlux type dependencies beyond Kinesis::Vision itself.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Union of possible structured outputs from a VisionSequence.
 *
 * The active alternative matches the terminal step of the sequence:
 *   - std::monostate   : terminal step produced only a pixel image
 *   - GradientResult   : terminal step was Sobel or Scharr
 *   - ComponentResult  : terminal step was connected_components
 *   - vector<Contour>  : terminal step was find_contours
 *   - vector<Keypoint> : terminal step was extract_peaks
 *   - vector<TrackResult>: terminal step was track_keypoints
 */
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
 * pixel_image holds the final float pixel buffer produced by the sequence.
 * It is empty when the terminal step produces only structured output with
 * no pixel image (e.g. extract_peaks, track_keypoints).
 *
 * structured holds the terminal step's structured output when the step
 * produces one. It is monostate when the terminal step produces only a
 * pixel image.
 *
 * w and h are the dimensions of pixel_image. Both are 0 when pixel_image
 * is empty.
 */
struct VisionResult {
    std::vector<float> pixel_image;
    StructuredOutput structured { std::monostate {} };
    uint32_t w { 0 };
    uint32_t h { 0 };
};

/**
 * @class VisionExecutor
 * @brief Stateful executor for a VisionSequence.
 *
 * Holds the previous gray frame for optical flow tracking across calls.
 * All other steps are stateless; the executor carries no other mutable state.
 *
 * One executor instance per pipeline: do not share across containers or
 * threads without external synchronisation.
 */
class MAYAFLUX_API VisionExecutor {
public:
    VisionExecutor() = default;

    /**
     * @brief Execute a VisionSequence on one frame.
     *
     * @param sequence Ordered steps to execute.
     * @param frame    Normalised float input frame. Format must match the
     *                 first step: RGBA (4 floats/pixel) for RgbaToGray /
     *                 RgbaToHsv, single-channel (1 float/pixel) for all
     *                 other entry points.
     * @param w        Frame width in pixels.
     * @param h        Frame height in pixels.
     * @return         VisionResult with pixel_image and/or structured output.
     */
    [[nodiscard]] VisionResult run(
        const VisionSequence& sequence,
        std::span<const float> frame,
        uint32_t w, uint32_t h);

    /**
     * @brief Clear the stored previous frame.
     *
     * Call when the source changes (new camera device, seek in video file)
     * so the next track_keypoints step starts from a clean state.
     */
    void reset();

private:
    std::vector<float> m_prev_gray;
    std::vector<Keypoint> m_prev_keypoints;
};

} // namespace MayaFlux::Kinesis::Vision
