#pragma once

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

struct TrackKeypointsParams {
    uint32_t window_radius = 7;
    uint32_t max_iterations = 20;
    float eigen_threshold = 1e-4F;
    float error_threshold = 0.3F;
};

struct FindContoursParams {
    float min_area { 0.0F };
    uint32_t max_contours { 0 };
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
    FindContoursParams>;

/**
 * @brief One step in a VisionSequence: an op and its parameters.
 */
struct VisionStep {
    VisionOp op;
    VisionParams params { std::monostate {} };
};

/**
 * @brief Ordered sequence of VisionSteps describing a complete vision pipeline.
 *
 * Constructed via the fluent VisionSequence::Builder.
 */
struct VisionSequence {
    std::vector<VisionStep> steps;
    bool tracks_keypoints { false };
    bool track_follows_peaks { false };

    /**
     * @brief Fluent builder for VisionSequence.
     *
     * Each method appends one step and returns *this for chaining.
     * Call build() to produce the final VisionSequence.
     *
     * @code
     * auto seq = VisionSequence::Builder{}
     *     .rgba_to_gray()
     *     .gaussian_blur(1.5f)
     *     .threshold(0.4f)
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

        Builder& threshold(float value)
        {
            return push(VisionOp::Threshold, ThresholdParams { .value = value });
        }

        Builder& threshold_adaptive(uint32_t block_size, float offset)
        {
            return push(VisionOp::ThresholdAdaptive,
                ThresholdAdaptiveParams { .block_size = block_size, .offset = offset });
        }

        Builder& threshold_otsu()
        {
            return push(VisionOp::ThresholdOtsu);
        }

        Builder& normalize()
        {
            return push(VisionOp::NormalizeInplace);
        }

        Builder& normalize_range(float lo, float hi)
        {
            return push(VisionOp::NormalizeRange,
                NormalizeRangeParams { .lo = lo, .hi = hi });
        }

        Builder& gaussian_blur(float sigma)
        {
            return push(VisionOp::GaussianBlur, GaussianBlurParams { .sigma = sigma });
        }

        Builder& filter_separable(
            std::vector<float> kx, std::vector<float> ky)
        {
            return push(VisionOp::FilterSeparable,
                FilterSeparableParams { .kernel_x = std::move(kx), .kernel_y = std::move(ky) });
        }

        Builder& sobel()
        {
            return push(VisionOp::Sobel);
        }

        Builder& scharr()
        {
            return push(VisionOp::Scharr);
        }

        Builder& canny(float sigma, float lo, float hi)
        {
            return push(VisionOp::Canny, CannyParams { .sigma = sigma, .low_threshold = lo, .high_threshold = hi });
        }

        Builder& erode(uint32_t radius)
        {
            return push(VisionOp::Erode, MorphParams { .radius = radius });
        }

        Builder& dilate(uint32_t radius)
        {
            return push(VisionOp::Dilate, MorphParams { .radius = radius });
        }

        Builder& open(uint32_t radius)
        {
            return push(VisionOp::Open, MorphParams { .radius = radius });
        }

        Builder& close(uint32_t radius)
        {
            return push(VisionOp::Close, MorphParams { .radius = radius });
        }

        Builder& morph_gradient(uint32_t radius)
        {
            return push(VisionOp::MorphGradient, MorphParams { .radius = radius });
        }

        Builder& harris_response(float k = 0.04F, float sigma = 1.0F)
        {
            return push(VisionOp::HarrisResponse, HarrisParams { .k = k, .sigma = sigma });
        }

        Builder& extract_peaks(float threshold, uint32_t nms_radius)
        {
            return push(VisionOp::ExtractPeaks,
                ExtractPeaksParams { .threshold = threshold, .nms_radius = nms_radius });
        }

        Builder& track_keypoints(
            uint32_t window_radius = 7,
            uint32_t max_iterations = 20,
            float eigen_threshold = 1e-4F,
            float error_threshold = 0.3F)
        {
            return push(VisionOp::TrackKeypoints,
                TrackKeypointsParams {
                    .window_radius = window_radius, .max_iterations = max_iterations, .eigen_threshold = eigen_threshold, .error_threshold = error_threshold });
        }

        Builder& find_contours(float min_area = 0.0F, uint32_t max_contours = 0)
        {
            return push(VisionOp::FindContours,
                FindContoursParams { .min_area = min_area, .max_contours = max_contours });
        }

        [[nodiscard]] VisionSequence build()
        {
            VisionSequence seq { .steps = std::move(m_steps) };
            for (size_t i = 0; i < seq.steps.size(); ++i) {
                if (seq.steps[i].op == VisionOp::TrackKeypoints)
                    seq.tracks_keypoints = true;
                if (i + 1 < seq.steps.size()
                    && seq.steps[i].op == VisionOp::ExtractPeaks
                    && seq.steps[i + 1].op == VisionOp::TrackKeypoints)
                    seq.track_follows_peaks = true;
            }
            return seq;
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

} // namespace MayaFlux::Kinesis::Vision
