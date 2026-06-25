#include "Harris.hpp"

#include "Gradient.hpp"
#include "ImageFilter.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

std::vector<float> harris_response(
    std::span<const float> gray, uint32_t w, uint32_t h,
    float k, float sigma)
{
    const size_t n = static_cast<size_t>(w) * h;

    auto grad = sobel(gray, w, h);

    std::vector<float> ixx(n), iyy(n), ixy(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const float dx = grad.dx[i];
            const float dy = grad.dy[i];
            ixx[i] = dx * dx;
            iyy[i] = dy * dy;
            ixy[i] = dx * dy;
        });

    auto sxx = gaussian_blur(ixx, w, h, sigma);
    auto syy = gaussian_blur(iyy, w, h, sigma);
    auto sxy = gaussian_blur(ixy, w, h, sigma);

    std::vector<float> response(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const float det = sxx[i] * syy[i] - sxy[i] * sxy[i];
            const float trace = sxx[i] + syy[i];
            response[i] = std::max(0.0F, det - k * trace * trace);
        });

    const float peak = *std::ranges::max_element(response);
    if (peak > 0.0F) {
        const float inv = 1.0F / peak;
        P::transform(P::par_unseq,
            response.begin(), response.end(), response.begin(),
            [inv](float v) { return v * inv; });
    }

    return response;
}

void harris_response(
    std::span<const float> gray,
    std::span<float> dx, std::span<float> dy, std::span<float> tmp,
    std::span<float> ixx, std::span<float> iyy, std::span<float> ixy,
    std::span<float> sxx, std::span<float> syy, std::span<float> sxy,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    float k,
    std::span<const float> kern)
{
    const size_t n = static_cast<size_t>(w) * h;

    sobel(gray, dx, dy, tmp, w, h);

    {
        const float* dx_ptr = dx.data();
        const float* dy_ptr = dy.data();
        float* ixx_ptr = ixx.data();
        float* iyy_ptr = iyy.data();
        float* ixy_ptr = ixy.data();

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t i) {
                const float gx = dx_ptr[i];
                const float gy = dy_ptr[i];
                ixx_ptr[i] = gx * gx;
                iyy_ptr[i] = gy * gy;
                ixy_ptr[i] = gx * gy;
            });
    }

    const float* h_src[3] = { ixx.data(), iyy.data(), ixy.data() };
    float* h_dst[3] = { dx.data(), dy.data(), tmp.data() };
    filter_horizontal_planes({ h_src, 3 }, { h_dst, 3 }, w, h, kern);

    const float* v_src[3] = { dx.data(), dy.data(), tmp.data() };
    float* v_dst[3] = { sxx.data(), syy.data(), sxy.data() };
    filter_vertical_planes({ v_src, 3 }, { v_dst, 3 }, w, h, kern);

    {
        const float* sxx_ptr = sxx.data();
        const float* syy_ptr = syy.data();
        const float* sxy_ptr = sxy.data();
        float* dst_ptr = dst.data();

#ifdef MAYAFLUX_ARCH_X64
        const __m256 k_vec = _mm256_set1_ps(k);
        const __m256 zero = _mm256_setzero_ps();

        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            const __m256 s0 = _mm256_loadu_ps(sxx_ptr + i);
            const __m256 s1 = _mm256_loadu_ps(syy_ptr + i);
            const __m256 s01 = _mm256_loadu_ps(sxy_ptr + i);

            const __m256 det = _mm256_sub_ps(
                _mm256_mul_ps(s0, s1),
                _mm256_mul_ps(s01, s01));
            const __m256 trace = _mm256_add_ps(s0, s1);
            const __m256 resp = _mm256_max_ps(zero,
                _mm256_fnmadd_ps(k_vec,
                    _mm256_mul_ps(trace, trace),
                    det));

            _mm256_storeu_ps(dst_ptr + i, resp);
        }
        for (; i < n; ++i) {
            const float s0 = sxx_ptr[i];
            const float s1 = syy_ptr[i];
            const float s01 = sxy_ptr[i];
            const float det = s0 * s1 - s01 * s01;
            const float trace = s0 + s1;
            dst_ptr[i] = std::max(0.0F, det - k * trace * trace);
        }

#elif defined(MAYAFLUX_ARCH_ARM64)
        const float32x4_t k_vec = vdupq_n_f32(k);
        const float32x4_t zero = vdupq_n_f32(0.0F);

        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            const float32x4_t s0 = vld1q_f32(sxx_ptr + i);
            const float32x4_t s1 = vld1q_f32(syy_ptr + i);
            const float32x4_t s01 = vld1q_f32(sxy_ptr + i);

            const float32x4_t det = vsubq_f32(
                vmulq_f32(s0, s1),
                vmulq_f32(s01, s01));
            const float32x4_t trace = vaddq_f32(s0, s1);
            const float32x4_t resp = vmaxq_f32(zero,
                vmlsq_f32(det, k_vec, vmulq_f32(trace, trace)));

            vst1q_f32(dst_ptr + i, resp);
        }
        for (; i < n; ++i) {
            const float s0 = sxx_ptr[i];
            const float s1 = syy_ptr[i];
            const float s01 = sxy_ptr[i];
            const float det = s0 * s1 - s01 * s01;
            const float trace = s0 + s1;
            dst_ptr[i] = std::max(0.0F, det - k * trace * trace);
        }

#else
        for (size_t i = 0; i < n; ++i) {
            const float s0 = sxx_ptr[i];
            const float s1 = syy_ptr[i];
            const float s01 = sxy_ptr[i];
            const float det = s0 * s1 - s01 * s01;
            const float trace = s0 + s1;
            dst_ptr[i] = std::max(0.0F, det - k * trace * trace);
        }
#endif
    }
}

std::vector<Keypoint> extract_peaks(
    std::span<const float> response, uint32_t w, uint32_t h,
    float threshold, uint32_t nms_radius)
{
    const auto n = static_cast<size_t>(w) * h;
    const auto r = static_cast<int32_t>(nms_radius);

    std::vector<Keypoint> keypoints;

    for (uint32_t py = 0; py < h; ++py) {
        for (uint32_t px = 0; px < w; ++px) {
            const size_t idx = static_cast<size_t>(py) * w + px;
            const float val = response[idx];

            if (val < threshold)
                continue;

            bool is_max = true;
            for (int32_t dy = -r; dy <= r && is_max; ++dy) {
                const int32_t ny = static_cast<int32_t>(py) + dy;
                if (ny < 0 || ny >= static_cast<int32_t>(h))
                    continue;
                for (int32_t dx = -r; dx <= r && is_max; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    const int32_t nx = static_cast<int32_t>(px) + dx;
                    if (nx < 0 || nx >= static_cast<int32_t>(w))
                        continue;
                    const size_t ni = static_cast<size_t>(ny) * w + nx;
                    if (response[ni] >= val)
                        is_max = false;
                }
            }

            if (!is_max)
                continue;

            keypoints.push_back({
                .position = {
                    (static_cast<float>(px) + 0.5F) / static_cast<float>(w),
                    (static_cast<float>(py) + 0.5F) / static_cast<float>(h) },
                .response = val,
                .scale = 1.0F,
                .angle = 0.0F,
            });
        }
    }

    std::ranges::sort(keypoints, [](const Keypoint& a, const Keypoint& b) {
        return a.response > b.response;
    });

    return keypoints;
}

} // namespace MayaFlux::Kinesis::Vision
