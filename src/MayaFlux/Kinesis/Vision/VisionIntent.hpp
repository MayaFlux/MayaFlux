#pragma once

/**
 * @file VisionIntent.hpp
 * @brief Declarative "what to do" vocabulary, one layer above VisionOp.
 *
 * VisionOp/VisionStep/VisionSequence (VisionOp.hpp) are the mechanism layer:
 * a named algorithm and its parameters. VisionIntent is the layer above it:
 * what the caller wants, independent of which VisionOps achieve it. A
 * caller may want more than one thing from the same frame at once (track
 * an object while also finding other elements), so this is a composable
 * flag enum, not a single-value one.
 *
 * This file is a pure value type with no MayaFlux dependency beyond the
 * project-wide MF_BITMASK_OPERATORS macro (cmake/pch.h, force-included),
 * matching VisionOp.hpp's own design so any consumer of a VisionSequence
 * can eventually consume a resolved VisionIntent without knowing which
 * specific VisionOps were chosen.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @enum VisionIntent
 * @brief Composable flags naming what a vision analysis should produce.
 *
 * Backed by today's VisionOp catalog: FindElements, TrackObjects,
 * DetectFeatures, DetectEdges, EstimateMotion, MeasureAppearance.
 *
 * Named but not yet backed by any VisionOp; resolving one of these is
 * expected to fail clearly until a supporting op exists, the same way
 * VisionGpuExecutor::config() returns INVALID_SHADER for an op with no
 * GPU implementation rather than omitting the op: Classify, EstimateDepth,
 * DetectAnomaly, Register.
 */
enum class VisionIntent : uint32_t {
    NONE = 0U,

    /// Localize discrete things in the frame (thresholding/morphology into
    /// ConnectedComponents/FindContours).
    FindElements = 1U << 0U,

    /// Maintain identity of specific points across frames (ExtractPeaks
    /// into TrackKeypoints).
    TrackObjects = 1U << 1U,

    /// Distinctive points with no persistence claim (HarrisResponse into
    /// ExtractPeaks alone).
    DetectFeatures = 1U << 2U,

    /// Boundary/gradient detection (Sobel, Scharr, Canny).
    DetectEdges = 1U << 3U,

    /// Dense, whole-field motion with no identity claim (OpticalFlowDense).
    /// Kept separate from TrackObjects: a global flow field and a
    /// persistently tracked point are different claims.
    EstimateMotion = 1U << 4U,

    /// Photometric/statistical characterization of the frame itself,
    /// independent of anything found in it.
    MeasureAppearance = 1U << 5U,

    /// Assign a label to a detected region or the whole frame. No backing
    /// VisionOp exists yet; needs a learned-model op.
    Classify = 1U << 6U,

    /// Per-pixel depth or disparity. No backing VisionOp exists yet; a
    /// different output shape entirely from every flag above it.
    EstimateDepth = 1U << 7U,

    /// Flag departure from a learned or authored baseline. No backing
    /// VisionOp exists yet; needs a reference/baseline concept VisionOp
    /// has no notion of today.
    DetectAnomaly = 1U << 8U,

    /// Align two or more frames into a common reference. No backing
    /// VisionOp exists yet; the only flag here whose input is plural.
    Register = 1U << 9U,
};

MF_BITMASK_OPERATORS(VisionIntent)

} // namespace MayaFlux::Kinesis::Vision
