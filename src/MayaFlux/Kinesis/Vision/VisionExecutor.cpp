#include "VisionExecutor.hpp"

#include "Contours.hpp"
#include "Filter.hpp"
#include "Gradient.hpp"
#include "Harris.hpp"
#include "Morphology.hpp"
#include "OpticalFlow.hpp"
#include "PixelOps.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kinesis::Vision {

namespace {

    // =========================================================================
    // Parameter accessors — crash clearly on wrong variant rather than silently
    // =========================================================================

    template <typename T>
    const T& get_params(const VisionParams& p, VisionOp op)
    {
        const T* ptr = std::get_if<T>(&p);
        if (!ptr) {
            error<std::logic_error>(
                Journal::Component::Kinesis, Journal::Context::Runtime, std::source_location::current(),
                "VisionExecutor: parameter type mismatch for op {}",
                Reflect::enum_to_string(op));
        }
        return *ptr;
    }

} // namespace

VisionResult VisionExecutor::run(
    const VisionSequence& sequence,
    std::span<const float> frame,
    uint32_t w, uint32_t h)
{
    VisionResult result;
    result.w = w;
    result.h = h;

    std::vector<float> current(frame.begin(), frame.end());

    for (const auto& step : sequence.steps) {
        switch (step.op) {

        case VisionOp::Downsample2x: {
            uint32_t new_w = 0, new_h = 0;
            current = downsample_2x(current, w, h, new_w, new_h);
            w = new_w;
            h = new_h;
            result.w = w;
            result.h = h;
            result.structured = std::monostate {};
            break;
        }

            // =====================================================================
            // Colour space
            // =====================================================================

        case VisionOp::RgbaToGray:
            current = rgba_to_gray(current, w, h);
            result.structured = std::monostate {};
            break;

        case VisionOp::RgbaToHsv:
            current = rgba_to_hsv(current, w, h);
            result.structured = std::monostate {};
            break;

        case VisionOp::GrayToRgba:
            current = gray_to_rgba(current, w, h);
            result.structured = std::monostate {};
            break;

            // =====================================================================
            // Thresholding
            // =====================================================================

        case VisionOp::Threshold: {
            const auto& p = get_params<ThresholdParams>(step.params, step.op);
            current = threshold(current, p.value);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ThresholdAdaptive: {
            const auto& p = get_params<ThresholdAdaptiveParams>(step.params, step.op);
            current = threshold_adaptive(current, w, h, p.block_size, p.offset);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ThresholdOtsu:
            current = threshold_otsu(current);
            result.structured = std::monostate {};
            break;

            // =====================================================================
            // Normalisation
            // =====================================================================

        case VisionOp::NormalizeInplace:
            normalize_inplace(current);
            result.structured = std::monostate {};
            break;

        case VisionOp::NormalizeRange: {
            const auto& p = get_params<NormalizeRangeParams>(step.params, step.op);
            normalize_range_inplace(current, p.lo, p.hi);
            result.structured = std::monostate {};
            break;
        }

            // =====================================================================
            // Filter
            // =====================================================================

        case VisionOp::GaussianBlur: {
            const auto& p = get_params<GaussianBlurParams>(step.params, step.op);
            current = gaussian_blur(current, w, h, p.sigma);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::FilterSeparable: {
            const auto& p = get_params<FilterSeparableParams>(step.params, step.op);
            current = filter_separable(current, w, h, p.kernel_x, p.kernel_y);
            result.structured = std::monostate {};
            break;
        }

            // =====================================================================
            // Gradient
            // =====================================================================

        case VisionOp::Sobel: {
            auto grad = sobel(current, w, h);
            result.structured = grad;
            current = std::move(grad.magnitude);
            break;
        }

        case VisionOp::Scharr: {
            auto grad = scharr(current, w, h);
            result.structured = grad;
            current = std::move(grad.magnitude);
            break;
        }

        case VisionOp::Canny: {
            const auto& p = get_params<CannyParams>(step.params, step.op);
            current = canny(current, w, h, p.sigma, p.low_threshold, p.high_threshold);
            result.structured = std::monostate {};
            break;
        }

            // =====================================================================
            // Morphology
            // =====================================================================

        case VisionOp::Erode: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            current = erode(current, w, h, p.radius);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Dilate: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            current = dilate(current, w, h, p.radius);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Open: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            current = open(current, w, h, p.radius);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Close: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            current = close(current, w, h, p.radius);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::MorphGradient: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            current = morph_gradient(current, w, h, p.radius);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ConnectedComponents: {
            auto cc = connected_components(current, w, h);
            result.structured = std::move(cc);
            current.clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        case VisionOp::FindContours: {
            auto contours = find_contours(current, w, h);
            result.structured = std::move(contours);
            current.clear();
            result.w = 0;
            result.h = 0;
            break;
        }

            // =====================================================================
            // Harris
            // =====================================================================

        case VisionOp::HarrisResponse: {
            const auto& p = get_params<HarrisParams>(step.params, step.op);
            current = harris_response(current, w, h, p.k, p.sigma);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ExtractPeaks: {
            const auto& p = get_params<ExtractPeaksParams>(step.params, step.op);
            auto kpts = extract_peaks(current, w, h, p.threshold, p.nms_radius);
            m_prev_keypoints = kpts;
            result.structured = std::move(kpts);
            current.clear();
            result.w = 0;
            result.h = 0;
            break;
        }

            // =====================================================================
            // Optical flow
            // =====================================================================

        case VisionOp::TrackKeypoints: {
            const auto& p = get_params<TrackKeypointsParams>(step.params, step.op);

            if (m_prev_gray.empty() || m_prev_keypoints.empty()) {
                m_prev_gray = current;
                result.structured = std::vector<TrackResult> {};
                current.clear();
                result.w = 0;
                result.h = 0;
                break;
            }

            std::vector<glm::vec2> prev_positions;
            prev_positions.reserve(m_prev_keypoints.size());
            for (const auto& kp : m_prev_keypoints)
                prev_positions.push_back(kp.position);

            auto tracked = track_keypoints(
                m_prev_gray, current,
                w, h,
                prev_positions,
                p.window_radius,
                p.max_iterations,
                p.eigen_threshold,
                p.error_threshold);

            m_prev_gray = current;
            result.structured = std::move(tracked);
            current.clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        } // switch
    }

    result.pixel_image = std::move(current);
    return result;
}

void VisionExecutor::reset()
{
    m_prev_gray.clear();
    m_prev_keypoints.clear();
}

} // namespace MayaFlux::Kinesis::Vision
