#include "VisionExecutor.hpp"

#include "Contours.hpp"
#include "Gradient.hpp"
#include "Harris.hpp"
#include "ImageFilter.hpp"
#include "Morphology.hpp"
#include "OpticalFlow.hpp"
#include "PixelOps.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    template <typename T>
    const T& get_params(const VisionParams& p, VisionOp op)
    {
        const T* ptr = std::get_if<T>(&p);
        if (!ptr) {
            error<std::logic_error>(
                Journal::Component::Kinesis, Journal::Context::Runtime,
                std::source_location::current(),
                "VisionExecutor: parameter type mismatch for op {}",
                Reflect::enum_to_string(op));
        }
        return *ptr;
    }

} // namespace

// =============================================================================
// Slot management
// =============================================================================

void VisionExecutor::ensure_slots(uint32_t w, uint32_t h)
{
    if (m_slot_w == w && m_slot_h == h)
        return;

    const size_t n = static_cast<size_t>(w) * h;
    const size_t n_rgba = n * 4;

    m_slots[k_slot_cur] = std::vector<float>(n_rgba, 0.0F);
    for (size_t i = 1; i < k_slot_count; ++i)
        m_slots[i] = std::vector<float>(n, 0.0F);

    m_slot_w = w;
    m_slot_h = h;
}

// =============================================================================
// Kernel cache
// =============================================================================

const std::vector<float>& VisionExecutor::gaussian_kernel(float sigma)
{
    const auto key = std::bit_cast<uint32_t>(sigma);
    auto it = m_kernel_cache.find(key);
    if (it != m_kernel_cache.end())
        return it->second;

    const auto radius = static_cast<int32_t>(std::ceil(3.0F * sigma));
    const int32_t size = 2 * radius + 1;
    const float inv_2s2 = 1.0F / (2.0F * sigma * sigma);

    std::vector<float> k(static_cast<size_t>(size));
    float sum = 0.0F;
    for (int32_t i = -radius; i <= radius; ++i) {
        const float v = std::exp(-static_cast<float>(i * i) * inv_2s2);
        k[static_cast<size_t>(i + radius)] = v;
        sum += v;
    }

    auto kmap = Eigen::Map<Eigen::ArrayXf>(k.data(), static_cast<Eigen::Index>(k.size()));
    kmap /= sum;

    return m_kernel_cache.emplace(key, std::move(k)).first->second;
}

// =============================================================================
// Step implementations
// =============================================================================

void VisionExecutor::step_filter_separable(
    size_t src_slot, size_t dst_slot,
    uint32_t w, uint32_t h,
    std::span<const float> kernel_x,
    std::span<const float> kernel_y)
{
    filter_separable(
        slot_vec(src_slot), slot_vec(k_slot_tmp), slot_vec(dst_slot),
        w, h, kernel_x, kernel_y);
}

void VisionExecutor::step_gaussian_blur(
    size_t src_slot, size_t dst_slot,
    uint32_t w, uint32_t h,
    float sigma)
{
    const auto& kern = gaussian_kernel(sigma);
    step_filter_separable(src_slot, dst_slot, w, h, kern, kern);
}

void VisionExecutor::step_harris(
    size_t src_slot, size_t dst_slot,
    uint32_t w, uint32_t h,
    float k, float sigma,
    Eigen::Index en)
{
    sobel(slot_vec(src_slot), slot_vec(k_slot_dx), slot_vec(k_slot_dy),
        slot_vec(k_slot_tmp), w, h);

    {
        const float* dx_ptr = slot_vec(k_slot_dx).data();
        const float* dy_ptr = slot_vec(k_slot_dy).data();
        float* ixx_ptr = slot_vec(k_slot_ixx).data();
        float* iyy_ptr = slot_vec(k_slot_iyy).data();
        float* ixy_ptr = slot_vec(k_slot_ixy).data();
        const auto n = static_cast<size_t>(en);

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t i) {
                const float dx = dx_ptr[i];
                const float dy = dy_ptr[i];
                ixx_ptr[i] = dx * dx;
                iyy_ptr[i] = dy * dy;
                ixy_ptr[i] = dx * dy;
            });
    }

    const auto& kern = gaussian_kernel(sigma);

    const float* h_src[3] = { slot_vec(k_slot_ixx).data(), slot_vec(k_slot_iyy).data(), slot_vec(k_slot_ixy).data() };
    float* h_dst[3] = { slot_vec(k_slot_dx).data(), slot_vec(k_slot_dy).data(), slot_vec(k_slot_tmp).data() };
    filter_horizontal_planes({ h_src, 3 }, { h_dst, 3 }, w, h, kern);

    const float* v_src[3] = { slot_vec(k_slot_dx).data(), slot_vec(k_slot_dy).data(), slot_vec(k_slot_tmp).data() };
    float* v_dst[3] = { slot_vec(k_slot_sxx).data(), slot_vec(k_slot_syy).data(), slot_vec(k_slot_sxy).data() };
    filter_vertical_planes({ v_src, 3 }, { v_dst, 3 }, w, h, kern);

    {
        const float* sxx_ptr = slot_vec(k_slot_sxx).data();
        const float* syy_ptr = slot_vec(k_slot_syy).data();
        const float* sxy_ptr = slot_vec(k_slot_sxy).data();
        float* dst_ptr = slot_vec(dst_slot).data();
        const auto n = static_cast<size_t>(en);

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t i) {
                const float sxx = sxx_ptr[i];
                const float syy = syy_ptr[i];
                const float sxy = sxy_ptr[i];
                const float det = sxx * syy - sxy * sxy;
                const float trace = sxx + syy;
                dst_ptr[i] = std::max(0.0F, det - k * trace * trace);
            });

        float peak = 0.0F;
        for (size_t i = 0; i < n; ++i)
            peak = std::max(peak, dst_ptr[i]);

        if (peak > 0.0F) {
            const float inv = 1.0F / peak;
            P::transform(P::par_unseq,
                dst_ptr, dst_ptr + n, dst_ptr,
                [inv](float v) { return v * inv; });
        }
    }
}

// =============================================================================
// reset
// =============================================================================

void VisionExecutor::reset()
{
    std::get<std::vector<float>>(m_prev_gray).clear();
    m_prev_keypoints.clear();
}

// =============================================================================
// run
// =============================================================================

VisionResult VisionExecutor::run(
    const VisionSequence& sequence,
    std::span<const float> frame,
    uint32_t w, uint32_t h)
{
    ensure_slots(w, h);

    auto& cur_vec = slot_vec(k_slot_cur);
    cur_vec.assign(frame.begin(), frame.end());

    size_t cur = k_slot_cur;
    size_t nxt = k_slot_nxt;
    auto en = static_cast<Eigen::Index>(static_cast<size_t>(w) * h);

    VisionResult result;
    result.w = w;
    result.h = h;

    for (const auto& step : sequence.steps) {
        switch (step.op) {

        case VisionOp::Downsample2x: {
            uint32_t new_w = 0, new_h = 0;
            downsample_2x(slot_vec(cur), slot_vec(nxt), w, h, new_w, new_h);
            w = new_w;
            h = new_h;
            result.w = w;
            result.h = h;
            en = static_cast<Eigen::Index>(static_cast<size_t>(w) * h);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::RgbaToGray: {
            rgba_to_gray(slot_vec(cur), slot_vec(nxt), w, h);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::RgbaToHsv: {
            rgba_to_hsv(slot_vec(cur), slot_vec(nxt), w, h);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::GrayToRgba: {
            gray_to_rgba(slot_vec(cur), slot_vec(nxt), w, h);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Threshold: {
            const auto& p = get_params<ThresholdParams>(step.params, step.op);
            slot_map_mut(cur, en) = (slot_map(cur, en) >= p.value).cast<float>();
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ThresholdAdaptive: {
            const auto& p = get_params<ThresholdAdaptiveParams>(step.params, step.op);
            threshold_adaptive(slot_vec(cur), slot_vec(nxt), w, h, p.block_size, p.offset);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ThresholdOtsu: {
            threshold_otsu(slot_vec(cur), slot_vec(nxt));
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::NormalizeInplace: {
            auto m = slot_map_mut(cur, en);
            const float mn = m.minCoeff();
            const float mx = m.maxCoeff();
            if (mx > mn)
                m = (m - mn) / (mx - mn);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::NormalizeRange: {
            const auto& p = get_params<NormalizeRangeParams>(step.params, step.op);
            if (p.hi > p.lo) {
                auto m = slot_map_mut(cur, en);
                m = ((m - p.lo) / (p.hi - p.lo)).max(0.0F).min(1.0F);
            }
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::GaussianBlur: {
            const auto& p = get_params<GaussianBlurParams>(step.params, step.op);
            step_gaussian_blur(cur, nxt, w, h, p.sigma);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::FilterSeparable: {
            const auto& p = get_params<FilterSeparableParams>(step.params, step.op);
            step_filter_separable(cur, nxt, w, h, p.kernel_x, p.kernel_y);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Sobel: {
            GradientResult grad;
            sobel(slot_vec(cur), slot_vec(k_slot_dx), slot_vec(k_slot_dy),
                slot_vec(k_slot_tmp), w, h);

            auto dx = slot_map(k_slot_dx, en);
            auto dy = slot_map(k_slot_dy, en);
            slot_map_mut(nxt, en) = (dx.square() + dy.square()).sqrt();
            const float peak = slot_map(nxt, en).maxCoeff();

            if (peak > 0.0F)
                slot_map_mut(nxt, en) /= peak;

            grad.dx.assign(slot_vec(k_slot_dx).begin(), slot_vec(k_slot_dx).begin() + en);
            grad.dy.assign(slot_vec(k_slot_dy).begin(), slot_vec(k_slot_dy).begin() + en);
            grad.magnitude.assign(slot_vec(nxt).begin(), slot_vec(nxt).begin() + en);
            grad.angle.resize(static_cast<size_t>(en));

            P::transform(P::par_unseq,
                slot_vec(k_slot_dx).begin(), slot_vec(k_slot_dx).begin() + en,
                slot_vec(k_slot_dy).begin(), grad.angle.begin(),
                [](float gx, float gy) { return std::atan2(gy, gx); });

            result.structured = std::move(grad);
            std::swap(cur, nxt);
            break;
        }

        case VisionOp::Scharr: {
            GradientResult grad;
            scharr(slot_vec(cur), slot_vec(k_slot_dx), slot_vec(k_slot_dy),
                slot_vec(k_slot_tmp), w, h);

            auto dx = slot_map(k_slot_dx, en);
            auto dy = slot_map(k_slot_dy, en);
            slot_map_mut(nxt, en) = (dx.square() + dy.square()).sqrt();

            const float peak = slot_map(nxt, en).maxCoeff();
            if (peak > 0.0F)
                slot_map_mut(nxt, en) /= peak;

            grad.dx.assign(slot_vec(k_slot_dx).begin(), slot_vec(k_slot_dx).begin() + en);
            grad.dy.assign(slot_vec(k_slot_dy).begin(), slot_vec(k_slot_dy).begin() + en);
            grad.magnitude.assign(slot_vec(nxt).begin(), slot_vec(nxt).begin() + en);
            grad.angle.resize(static_cast<size_t>(en));

            P::transform(P::par_unseq,
                slot_vec(k_slot_dx).begin(), slot_vec(k_slot_dx).begin() + en,
                slot_vec(k_slot_dy).begin(), grad.angle.begin(),
                [](float gx, float gy) { return std::atan2(gy, gx); });

            result.structured = std::move(grad);
            std::swap(cur, nxt);
            break;
        }

        case VisionOp::Canny: {
            const auto& p = get_params<CannyParams>(step.params, step.op);
            canny(slot_vec(cur), slot_vec(nxt), w, h, p.sigma, p.low_threshold, p.high_threshold);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Erode: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            erode(slot_vec(cur), slot_vec(nxt), w, h, p.radius);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Dilate: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            dilate(slot_vec(cur), slot_vec(nxt), w, h, p.radius);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Open: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            open(slot_vec(cur), slot_vec(k_slot_tmp), slot_vec(nxt), w, h, p.radius);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::Close: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            close(slot_vec(cur), slot_vec(k_slot_tmp), slot_vec(nxt), w, h, p.radius);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::MorphGradient: {
            const auto& p = get_params<MorphParams>(step.params, step.op);
            morph_gradient(slot_vec(cur), slot_vec(k_slot_tmp), slot_vec(nxt), w, h, p.radius);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ConnectedComponents: {
            result.structured = connected_components(slot_vec(cur), w, h);
            slot_vec(cur).clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        case VisionOp::FindContours: {
            result.structured = find_contours(slot_vec(cur), w, h);
            slot_vec(cur).clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        case VisionOp::HarrisResponse: {
            const auto& p = get_params<HarrisParams>(step.params, step.op);
            step_harris(cur, nxt, w, h, p.k, p.sigma, en);
            std::swap(cur, nxt);
            result.structured = std::monostate {};
            break;
        }

        case VisionOp::ExtractPeaks: {
            const auto& p = get_params<ExtractPeaksParams>(step.params, step.op);
            auto kpts = extract_peaks(slot_vec(cur), w, h, p.threshold, p.nms_radius);
            m_prev_keypoints = kpts;
            result.structured = std::move(kpts);
            slot_vec(cur).clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        case VisionOp::TrackKeypoints: {
            const auto& p = get_params<TrackKeypointsParams>(step.params, step.op);

            auto& prev_vec = std::get<std::vector<float>>(m_prev_gray);
            if (prev_vec.empty() || m_prev_keypoints.empty()) {
                prev_vec = slot_vec(cur);
                result.structured = std::vector<TrackResult> {};
                slot_vec(cur).clear();
                result.w = 0;
                result.h = 0;
                break;
            }

            std::vector<glm::vec2> prev_pos;
            prev_pos.reserve(m_prev_keypoints.size());
            for (const auto& kp : m_prev_keypoints)
                prev_pos.push_back(kp.position);

            auto tracked = track_keypoints(
                prev_vec, slot_vec(cur),
                w, h, prev_pos,
                p.window_radius, p.max_iterations,
                p.eigen_threshold, p.error_threshold);

            prev_vec = slot_vec(cur);
            result.structured = std::move(tracked);
            slot_vec(cur).clear();
            result.w = 0;
            result.h = 0;
            break;
        }

        } // switch
    }

    result.pixel_image = std::move(m_slots[cur]);

    m_slots[cur] = std::vector<float> {};

    return result;
}

} // namespace MayaFlux::Kinesis::Vision
