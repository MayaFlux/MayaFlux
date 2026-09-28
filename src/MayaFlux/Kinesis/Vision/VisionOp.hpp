#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

/**
 * @file VisionOp.hpp
 * @brief Declarative description of a Kinesis::Vision processing sequence.
 *
 * VisionStep names one operation and carries the parameters needed to invoke
 * it. VisionSequence is an ordered list of steps. Both are pure value types
 * with no MayaFlux dependencies.
 *
 * Processors (VisionDataProcessor, VisionBufferProcessor) accept a
 * VisionSequence at construction and execute it each cycle without knowing
 * which specific algorithms are involved. No per-algorithm processor subclass
 * is needed.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @enum VisionOp
 * @brief Named operations available in a VisionSequence.
 *
 * Each enumerator maps 1:1 to a function in Kinesis::Vision.
 * The processor dispatches on this enum; parameters are carried
 * by the corresponding VisionStep variant field.
 */
enum class VisionOp : uint8_t {
    RgbaToGray,
    RgbaToHsv,
    GrayToRgba,

    Downsample2x,

    Threshold,
    ThresholdAdaptive,
    ThresholdOtsu,

    NormalizeInplace,
    NormalizeRange,

    GaussianBlur,
    FilterSeparable,

    Sobel,
    Scharr,
    Canny,

    Erode,
    Dilate,
    Open,
    Close,
    MorphGradient,
    ConnectedComponents,
    FindContours,

    HarrisResponse,
    ExtractPeaks,

    TrackKeypoints,
    Snapshot,

    OpticalFlowDense,
};

/**
 * @brief Selects where a VisionSequence executes.
 *
 * GPU uses MayaFlux::Yantra::VisionGpuExecutor, the Vulkan implementation in
 * Yantra. It accepts a GPU image, runs the vision steps through GPU dispatches,
 * and can keep image results on the device. This is the default because it
 * suits GPU image pipelines and avoids transferring intermediate images to
 * the host. Host input may still require an upload, and requesting host data
 * from a GPU result may require a readback and synchronization. GPU resource
 * setup and dispatch also have costs, particularly for small workloads.
 *
 * CPU uses MayaFlux::Kinesis::Vision::VisionExecutor, the host implementation
 * in Kinesis. It accepts normalized floating-point pixels in host memory and
 * produces host-side results. Choose it when the input and desired output are
 * already on the host, when a GPU is unavailable, or when a sequence needs an
 * operation implemented only by the CPU executor. Supplying a GPU image to a
 * CPU pipeline requires a transfer to host memory.
 *
 * The selected backend applies to the whole sequence. VisionProcessor and
 * ImageCVProcessor do not switch backend for individual steps or fall back to
 * the other executor when an operation is unsupported. The default expresses
 * the preferred path for GPU image processing, not a guarantee that it will
 * be faster for every input size or transfer pattern.
 */
enum class VisionBackend : uint8_t {
    GPU, ///< Execute the sequence with the Yantra Vulkan executor.
    CPU, ///< Execute the sequence with the Kinesis host executor.
};

// ============================================================================
// Per-op parameter structs
// ============================================================================

struct ThresholdParams {
    float value;
};
struct ThresholdAdaptiveParams {
    uint32_t block_size;
    float offset;
};
struct NormalizeRangeParams {
    float lo;
    float hi;
};
struct GaussianBlurParams {
    float sigma;
};

struct FilterSeparableParams {
    std::vector<float> kernel_x;
    std::vector<float> kernel_y;
};

struct CannyParams {
    float sigma;
    float low_threshold;
    float high_threshold;
};

struct MorphParams {
    uint32_t radius;
};

struct HarrisParams {
    float k = 0.04F;
    float sigma = 1.0F;
};

struct ExtractPeaksParams {
    float threshold;
    uint32_t nms_radius;
};

/**
 * @brief Parameters for TrackKeypoints.
 *
 * eigen_threshold and error_threshold are normalised by the window area, so
 * they do not depend on window_radius. levels, max_points and min_distance
 * apply to the persistent GPU tracker: levels is the pyramid depth including
 * full resolution, max_points caps live tracks, and min_distance is the grid
 * cell size in pixels that admits at most one new track. A positive
 * forward_backward_threshold rejects a track when its backward estimate
 * misses the source point by more than that many pixels; zero disables it.
 * With export_tracks set, the tracks are also delivered in
 * VisionResult::tracks_buffer for consumers that stay on the GPU. With
 * host_tracks cleared, the tracks are not read back to the host and the
 * structured result stays empty, so a sequence that only feeds GPU consumers
 * pays no transfer. It has no effect on the CPU executor, which always
 * produces the host result.
 */
struct TrackKeypointsParams {
    uint32_t window_radius = 7;
    uint32_t max_iterations = 20;
    float eigen_threshold = 1e-4F;
    float error_threshold = 0.3F;
    uint32_t levels = 4;
    uint32_t max_points = 512;
    float min_distance = 8.0F;
    float forward_backward_threshold = 0.0F;
    bool export_tracks = false;
    bool host_tracks = true;
};

/**
 * @brief Parameters for OpticalFlowDense.
 *
 * Dense coarse-to-fine Lucas-Kanade. window_radius is the half size of the
 * Gaussian weighted window in pixels of the level being solved, iterations the
 * refinement steps per level, and levels the pyramid depth including full
 * resolution (lowered for small images). eigen_threshold damps the solve so
 * untextured pixels take small steps, and max_step caps one increment in
 * pixels. With visualize set, a hue and brightness rendering of the flow is
 * delivered in VisionResult::debug_labels, at full brightness from
 * visual_range pixels. visual_min_motion keeps smaller displacements dark in
 * the visualization, in pixels of the flow image.
 */
struct OpticalFlowDenseParams {
    uint32_t window_radius = 5;
    uint32_t iterations = 3;
    uint32_t levels = 4;
    float eigen_threshold = 1e-3F;
    float max_step = 4.0F;
    bool visualize = false;
    float visual_range = 2.0F;
    float visual_min_motion = 0.0F;
};

/**
 * @brief Connected-component label export and color output.
 *
 * CPU always returns component counts and bounding boxes. export_labels also
 * retains the per-pixel host label_map; otherwise labels remain internal when
 * the next step extracts contours. with_colors produces a pixel-precision,
 * opaque RGBA host image in pixel_image, with black background and one color
 * per foreground label. Subsequent steps may replace that image.
 *
 * GPU export_labels requests a device label map and with_colors selects the
 * device color image in debug_labels.
 */
struct ConnectedComponentsParams {
    bool export_labels { false };
    bool with_colors { false };
};

/**
 * @brief Contour filtering, trace limits, and output selection.
 *
 * min_area is pixel coverage normalised by image area. A positive
 * max_contours selects contours by descending area, counting both outer
 * boundaries and holes. Zero imposes no caller count limit.
 * max_points_per_contour bounds each trace; zero imposes no caller point
 * limit. The GPU additionally applies its storage capacities.
 *
 * as_image replaces structured contours with white closed boundaries on an
 * opaque black background. CPU returns host RGBA pixels in pixel_image with
 * their dimensions; GPU returns a device image in debug_contours.
 */
struct FindContoursParams {
    float min_area { 0.0F };
    uint32_t max_contours { 0 };
    uint32_t max_points_per_contour { 0 };
    bool as_image { false };
};

/**
 * @brief Parameter variant covering all ops that carry parameters.
 *
 * Ops with no parameters (RgbaToGray, Sobel, Scharr, ThresholdOtsu,
 * NormalizeInplace, GrayToRgba, RgbaToHsv, MorphGradient, Erode, Dilate,
 * Open, Close) use std::monostate.
 */
using VisionParams = std::variant<
    std::monostate,
    ThresholdParams,
    ThresholdAdaptiveParams,
    NormalizeRangeParams,
    GaussianBlurParams,
    FilterSeparableParams,
    CannyParams,
    MorphParams,
    HarrisParams,
    ExtractPeaksParams,
    TrackKeypointsParams,
    ConnectedComponentsParams,
    FindContoursParams,
    OpticalFlowDenseParams>;

/**
 * @brief One step in a VisionSequence: an op and its parameters.
 *
 * A deferred step submits its GPU work without waiting on the fence. The
 * run that submits it returns SUSPENDED at that step index; a subsequent
 * run polls the fence, and once signalled resumes the sequence from the
 * following step. How many calls that takes is not the executor's concern.
 */
struct VisionStep {
    VisionOp op;
    VisionParams params { std::monostate {} };
    bool deferred { false };
};

/**
 * @brief Ordered VisionSteps and the backend chosen to execute them.
 *
 * Construct with the fluent VisionSequence::Builder. The backend belongs to
 * the sequence, so a processor uses the same executor for every step. Directly
 * constructed sequences default to VisionBackend::GPU.
 */
struct VisionSequence {
    std::vector<VisionStep> steps;
    VisionBackend backend { VisionBackend::GPU }; ///< Backend used for every step.

    /**
     * @brief Fluent builder for VisionSequence.
     *
     * Each method appends one step and returns *this for chaining.
     * Call build() to produce the final VisionSequence.
     * Parameterized operations accept their parameter struct,
     * allowing designated initializers in member declaration order. Omitted
     * settings use the struct's defaults. Operations whose settings have no
     * defaults require an explicit parameter struct.
     *
     * @code
     * auto seq = VisionSequence::Builder{}
     *     .rgba_to_gray()
     *     .gaussian_blur({ .sigma = 1.5F })
     *     .threshold({ .value = 0.4F })
     *     .build();
     * auto features = VisionSequence::Builder{}
     *     .rgba_to_gray()
     *     .harris_response({ .sigma = 1.5F })
     *     .extract_peaks({ .threshold = 0.05F, .nms_radius = 6 })
     *     .track_keypoints({ .error_threshold = 0.08F })
     *     .build();
     * @endcode
     */
    class Builder {
    public:
        Builder& rgba_to_gray()
        {
            return push(VisionOp::RgbaToGray);
        }

        Builder& rgba_to_hsv()
        {
            return push(VisionOp::RgbaToHsv);
        }

        Builder& gray_to_rgba()
        {
            return push(VisionOp::GrayToRgba);
        }

        Builder& downsample_2x()
        {
            return push(VisionOp::Downsample2x);
        }

        Builder& threshold(ThresholdParams params)
        {
            return push(VisionOp::Threshold, params);
        }

        Builder& threshold_adaptive(ThresholdAdaptiveParams params)
        {
            return push(VisionOp::ThresholdAdaptive, params);
        }

        Builder& threshold_otsu()
        {
            return push(VisionOp::ThresholdOtsu);
        }

        Builder& normalize()
        {
            return push(VisionOp::NormalizeInplace);
        }

        Builder& normalize_range(NormalizeRangeParams params)
        {
            return push(VisionOp::NormalizeRange, params);
        }

        Builder& gaussian_blur(GaussianBlurParams params)
        {
            return push(VisionOp::GaussianBlur, params);
        }

        Builder& filter_separable(FilterSeparableParams params)
        {
            return push(VisionOp::FilterSeparable, std::move(params));
        }

        Builder& sobel()
        {
            return push(VisionOp::Sobel);
        }

        Builder& scharr()
        {
            return push(VisionOp::Scharr);
        }

        Builder& canny(CannyParams params)
        {
            return push(VisionOp::Canny, params);
        }

        Builder& erode(MorphParams params)
        {
            return push(VisionOp::Erode, params);
        }

        Builder& dilate(MorphParams params)
        {
            return push(VisionOp::Dilate, params);
        }

        Builder& open(MorphParams params)
        {
            return push(VisionOp::Open, params);
        }

        Builder& close(MorphParams params)
        {
            return push(VisionOp::Close, params);
        }

        Builder& morph_gradient(MorphParams params)
        {
            return push(VisionOp::MorphGradient, params);
        }

        Builder& harris_response(HarrisParams params = {})
        {
            return push(VisionOp::HarrisResponse, params);
        }

        Builder& extract_peaks(ExtractPeaksParams params)
        {
            return push(VisionOp::ExtractPeaks, params);
        }

        Builder& connected_components(ConnectedComponentsParams params = {})
        {
            return push(VisionOp::ConnectedComponents, params);
        }

        Builder& track_keypoints(TrackKeypointsParams params = {})
        {
            return push(VisionOp::TrackKeypoints, params);
        }

        Builder& find_contours(FindContoursParams params = {})
        {
            return push(VisionOp::FindContours, params);
        }

        Builder& snapshot()
        {
            return push(VisionOp::Snapshot);
        }

        Builder& optical_flow_dense(OpticalFlowDenseParams params = {})
        {
            return push(VisionOp::OpticalFlowDense, params);
        }

        /**
         * @brief Finish the sequence and select its execution backend.
         *
         * Calling build() selects VisionBackend::GPU for GPU image pipelines.
         * Pass VisionBackend::CPU to run the entire sequence through the
         * Kinesis host executor instead. The choice is stored in the returned
         * VisionSequence and used by VisionProcessor or ImageCVProcessor when
         * that sequence is processed.
         *
         * @code
         * auto host_sequence = VisionSequence::Builder{}
         *     .rgba_to_gray()
         *     .build(VisionBackend::CPU);
         * @endcode
         *
         * @param backend Backend for all steps. Defaults to the Yantra GPU
         *                executor; no per-step fallback is performed.
         * @return The completed sequence with its selected backend.
         */
        [[nodiscard]] VisionSequence build(VisionBackend backend = VisionBackend::GPU)
        {
            return VisionSequence { .steps = std::move(m_steps), .backend = backend };
        }

        /**
         * @brief Mark the most recently pushed step deferred.
         */
        Builder& defer()
        {
            if (!m_steps.empty())
                m_steps.back().deferred = true;
            return *this;
        }

    private:
        std::vector<VisionStep> m_steps;

        Builder& push(VisionOp op, VisionParams p = std::monostate {})
        {
            m_steps.push_back({ .op = op, .params = std::move(p) });
            return *this;
        }
    };
};

/**
 * @brief Combine a hash into an existing seed, FNV-style.
 */
inline void hash_combine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9U + (seed << 6) + (seed >> 2);
}

/**
 * @brief Hash a VisionStep's op and parameters together.
 *
 * Keys GPU dispatch memoization on VisionPass::completed, which spans one
 * walk including any suspensions. Two steps hashing equal are treated as
 * interchangeable, so every field that changes the output must be hashed.
 */
inline size_t hash_vision_step(VisionOp op, const VisionParams& params)
{
    size_t seed = std::hash<std::string_view> {}(Reflect::enum_to_string(op));

    std::visit([&seed](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
        } else if constexpr (std::is_same_v<T, ThresholdParams>) {
            hash_combine(seed, std::hash<float> {}(p.value));
        } else if constexpr (std::is_same_v<T, ThresholdAdaptiveParams>) {
            hash_combine(seed, std::hash<uint32_t> {}(p.block_size));
            hash_combine(seed, std::hash<float> {}(p.offset));
        } else if constexpr (std::is_same_v<T, NormalizeRangeParams>) {
            hash_combine(seed, std::hash<float> {}(p.lo));
            hash_combine(seed, std::hash<float> {}(p.hi));
        } else if constexpr (std::is_same_v<T, GaussianBlurParams>) {
            hash_combine(seed, std::hash<float> {}(p.sigma));
        } else if constexpr (std::is_same_v<T, FilterSeparableParams>) {
            for (float v : p.kernel_x)
                hash_combine(seed, std::hash<float> {}(v));
            for (float v : p.kernel_y)
                hash_combine(seed, std::hash<float> {}(v));
        } else if constexpr (std::is_same_v<T, CannyParams>) {
            hash_combine(seed, std::hash<float> {}(p.sigma));
            hash_combine(seed, std::hash<float> {}(p.low_threshold));
            hash_combine(seed, std::hash<float> {}(p.high_threshold));
        } else if constexpr (std::is_same_v<T, MorphParams>) {
            hash_combine(seed, std::hash<uint32_t> {}(p.radius));
        } else if constexpr (std::is_same_v<T, HarrisParams>) {
            hash_combine(seed, std::hash<float> {}(p.k));
            hash_combine(seed, std::hash<float> {}(p.sigma));
        } else if constexpr (std::is_same_v<T, ExtractPeaksParams>) {
            hash_combine(seed, std::hash<float> {}(p.threshold));
            hash_combine(seed, std::hash<uint32_t> {}(p.nms_radius));
        } else if constexpr (std::is_same_v<T, TrackKeypointsParams>) {
            hash_combine(seed, std::hash<uint32_t> {}(p.window_radius));
            hash_combine(seed, std::hash<uint32_t> {}(p.max_iterations));
            hash_combine(seed, std::hash<float> {}(p.eigen_threshold));
            hash_combine(seed, std::hash<float> {}(p.error_threshold));
            hash_combine(seed, std::hash<uint32_t> {}(p.levels));
            hash_combine(seed, std::hash<uint32_t> {}(p.max_points));
            hash_combine(seed, std::hash<float> {}(p.min_distance));
            hash_combine(seed, std::hash<float> {}(p.forward_backward_threshold));
            hash_combine(seed, std::hash<bool> {}(p.export_tracks));
            hash_combine(seed, std::hash<bool> {}(p.host_tracks));
        } else if constexpr (std::is_same_v<T, FindContoursParams>) {
            hash_combine(seed, std::hash<float> {}(p.min_area));
            hash_combine(seed, std::hash<uint32_t> {}(p.max_contours));
            hash_combine(seed, std::hash<uint32_t> {}(p.max_points_per_contour));
            hash_combine(seed, std::hash<bool> {}(p.as_image));
        } else if constexpr (std::is_same_v<T, ConnectedComponentsParams>) {
            hash_combine(seed, std::hash<bool> {}(p.export_labels));
            hash_combine(seed, std::hash<bool> {}(p.with_colors));
        } else if constexpr (std::is_same_v<T, OpticalFlowDenseParams>) {
            hash_combine(seed, std::hash<uint32_t> {}(p.window_radius));
            hash_combine(seed, std::hash<uint32_t> {}(p.iterations));
            hash_combine(seed, std::hash<uint32_t> {}(p.levels));
            hash_combine(seed, std::hash<float> {}(p.eigen_threshold));
            hash_combine(seed, std::hash<float> {}(p.max_step));
            hash_combine(seed, std::hash<bool> {}(p.visualize));
            hash_combine(seed, std::hash<float> {}(p.visual_range));
            hash_combine(seed, std::hash<float> {}(p.visual_min_motion));
        }
    },
        params);

    return seed;
}

/**
 * @brief True when any step tracks keypoints.
 *
 * Whole-sequence, not index-local: the gray-frame capture this gates happens
 * at RgbaToGray, arbitrarily far ahead of the TrackKeypoints step that needs
 * it. Evaluate once per run; sequences are short.
 */
[[nodiscard]] inline bool tracks_keypoints(const VisionSequence& seq)
{
    return std::ranges::any_of(seq.steps,
        [](const VisionStep& s) { return s.op == VisionOp::TrackKeypoints; });
}

/**
 * @brief True when an ExtractPeaks step is immediately followed by TrackKeypoints.
 *
 * Whole-sequence for the same reason: the capture gate at RgbaToGray needs the
 * answer before either step is reached. Ops sitting at the pair itself should
 * use VisionPass::ahead() instead.
 */
[[nodiscard]] inline bool track_follows_peaks(const VisionSequence& seq)
{
    for (size_t i = 0; i + 1 < seq.steps.size(); ++i) {
        if (seq.steps[i].op == VisionOp::ExtractPeaks
            && seq.steps[i + 1].op == VisionOp::TrackKeypoints)
            return true;
    }
    return false;
}

} // namespace MayaFlux::Kinesis::Vision
