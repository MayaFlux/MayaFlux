// NOLINTBEGIN
#include "PixelOps.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"
#include <Eigen/Core>

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

void rgba_to_gray(std::span<const float> rgba, std::span<float> dst, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, h).begin(),
        std::views::iota(uint32_t { 0 }, h).end(),
        [&](uint32_t row) {
            const float* src_row = rgba.data() + static_cast<size_t>(row) * w * 4;
            float* dst_row = dst.data() + static_cast<size_t>(row) * w;

#ifdef MAYAFLUX_ARCH_X64
            uint32_t px = 0;
            for (; px + 8 <= w; px += 8) {
                __m128 a0 = _mm_loadu_ps(src_row + px * 4);
                __m128 a1 = _mm_loadu_ps(src_row + px * 4 + 4);
                __m128 a2 = _mm_loadu_ps(src_row + px * 4 + 8);
                __m128 a3 = _mm_loadu_ps(src_row + px * 4 + 12);
                _MM_TRANSPOSE4_PS(a0, a1, a2, a3);
                __m128 lo = _mm_fmadd_ps(a0, _mm_set1_ps(0.299F),
                    _mm_fmadd_ps(a1, _mm_set1_ps(0.587F),
                        _mm_mul_ps(a2, _mm_set1_ps(0.114F))));

                __m128 b0 = _mm_loadu_ps(src_row + px * 4 + 16);
                __m128 b1 = _mm_loadu_ps(src_row + px * 4 + 20);
                __m128 b2 = _mm_loadu_ps(src_row + px * 4 + 24);
                __m128 b3 = _mm_loadu_ps(src_row + px * 4 + 28);
                _MM_TRANSPOSE4_PS(b0, b1, b2, b3);
                __m128 hi = _mm_fmadd_ps(b0, _mm_set1_ps(0.299F),
                    _mm_fmadd_ps(b1, _mm_set1_ps(0.587F),
                        _mm_mul_ps(b2, _mm_set1_ps(0.114F))));

                _mm256_storeu_ps(dst_row + px, _mm256_set_m128(hi, lo));
            }
            for (; px < w; ++px) {
                const size_t p = px * 4;
                dst_row[px] = src_row[p] * 0.299F
                    + src_row[p + 1] * 0.587F
                    + src_row[p + 2] * 0.114F;
            }

#elif defined(MAYAFLUX_ARCH_ARM64)
        const float32x4_t k_r = vdupq_n_f32(0.299F);
        const float32x4_t k_g = vdupq_n_f32(0.587F);
        const float32x4_t k_b = vdupq_n_f32(0.114F);
        uint32_t px = 0;
        for (; px + 4 <= w; px += 4) {
            float32x4x4_t p = vld4q_f32(src_row + px * 4);
            vst1q_f32(dst_row + px,
                vmlaq_f32(vmlaq_f32(vmulq_f32(p.val[0], k_r), p.val[1], k_g), p.val[2], k_b));
        }
        for (; px < w; ++px) {
            const size_t p = px * 4;
            dst_row[px] = src_row[p] * 0.299F
                + src_row[p + 1] * 0.587F
                + src_row[p + 2] * 0.114F;
        }

#else
        for (uint32_t px = 0; px < w; ++px) {
            const size_t p = px * 4;
            dst_row[px] = src_row[p] * 0.299F
                + src_row[p + 1] * 0.587F
                + src_row[p + 2] * 0.114F;
        }
#endif
        });
}

std::vector<float> rgba_to_gray(std::span<const float> rgba, uint32_t w, uint32_t h)
{
    std::vector<float> out(static_cast<size_t>(w) * h);
    rgba_to_gray(rgba, out, w, h);
    return out;
}

void rgba_to_hsv(
    std::span<const float> rgba, std::span<float> dst,
    uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const float r = rgba[i * 4];
            const float g = rgba[i * 4 + 1];
            const float b = rgba[i * 4 + 2];

            const float cmax = std::max({ r, g, b });
            const float cmin = std::min({ r, g, b });
            const float delta = cmax - cmin;

            float h_val = 0.0F;
            if (delta > 0.0F) {
                if (cmax == r) {
                    h_val = std::fmod((g - b) / delta, 6.0F);
                } else if (cmax == g) {
                    h_val = (b - r) / delta + 2.0F;
                } else {
                    h_val = (r - g) / delta + 4.0F;
                }
                h_val /= 6.0F;
                if (h_val < 0.0F)
                    h_val += 1.0F;
            }

            dst[i * 3] = h_val;
            dst[i * 3 + 1] = cmax > 0.0F ? delta / cmax : 0.0F;
            dst[i * 3 + 2] = cmax;
        });
}

std::vector<float> rgba_to_hsv(std::span<const float> rgba, uint32_t w, uint32_t h)
{
    std::vector<float> out(static_cast<size_t>(w) * h * 3);
    rgba_to_hsv(rgba, out, w, h);
    return out;
}

void gray_to_rgba(std::span<const float> gray, std::span<float> dst, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, h).begin(),
        std::views::iota(uint32_t { 0 }, h).end(),
        [&](uint32_t row) {
            const float* src_row = gray.data() + static_cast<size_t>(row) * w;
            float* dst_row = dst.data() + static_cast<size_t>(row) * w * 4;

#ifdef MAYAFLUX_ARCH_X64
            // broadcasts each gray scalar into [g,g,g,1] using shuffle
            // process 2 pixels per iteration to get 8 floats = 1 AVX store
            const __m256 alpha_mask = _mm256_set_ps(1.0F, 0.0F, 0.0F, 0.0F,
                1.0F, 0.0F, 0.0F, 0.0F);
            uint32_t px = 0;
            for (; px + 2 <= w; px += 2) {
                const float g0 = src_row[px];
                const float g1 = src_row[px + 1];
                // [g0 g0 g0 1 g1 g1 g1 1]
                __m256 v = _mm256_set_ps(1.0F, g1, g1, g1, 1.0F, g0, g0, g0);
                _mm256_storeu_ps(dst_row + px * 4, v);
            }
            for (; px < w; ++px) {
                const float v = src_row[px];
                dst_row[px * 4] = dst_row[px * 4 + 1] = dst_row[px * 4 + 2] = v;
                dst_row[px * 4 + 3] = 1.0F;
            }

#elif defined(MAYAFLUX_ARCH_ARM64)
        const float32x4_t alpha = vdupq_n_f32(1.0F);
        uint32_t px = 0;
        for (; px + 4 <= w; px += 4) {
            float32x4_t g = vld1q_f32(src_row + px);
            float32x4_t lo = vzip1q_f32(g, g); // g0 g0 g1 g1
            float32x4_t hi = vzip2q_f32(g, g); // g2 g2 g3 g3
            float32x4x2_t p01 = vzipq_f32(lo, vdupq_lane_f32(vget_low_f32(alpha), 0));

            float32x4x4_t rgba;
            rgba.val[0] = g;
            rgba.val[1] = g;
            rgba.val[2] = g;
            rgba.val[3] = alpha;
            vst4q_f32(dst_row + px * 4, rgba);
            (void)lo;
            (void)hi;
            (void)p01;
        }
        for (; px < w; ++px) {
            const float v = src_row[px];
            dst_row[px * 4] = v;
            dst_row[px * 4 + 1] = v;
            dst_row[px * 4 + 2] = v;
            dst_row[px * 4 + 3] = 1.0F;
        }

#else
        for (uint32_t px = 0; px < w; ++px) {
            const float v = src_row[px];
            dst_row[px * 4] = v;
            dst_row[px * 4 + 1] = v;
            dst_row[px * 4 + 2] = v;
            dst_row[px * 4 + 3] = 1.0F;
        }
#endif
        });
}

std::vector<float> gray_to_rgba(std::span<const float> gray, uint32_t w, uint32_t h)
{
    std::vector<float> out(static_cast<size_t>(w) * h * 4);
    gray_to_rgba(gray, out, w, h);
    return out;
}

void threshold(std::span<const float> gray, std::span<float> dst, float value)
{
    const size_t n = gray.size();
    const float* src = gray.data();
    float* out = dst.data();

#ifdef MAYAFLUX_ARCH_X64
    const __m256 thresh = _mm256_set1_ps(value);
    const __m256 one = _mm256_set1_ps(1.0F);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m256 mask = _mm256_cmp_ps(v, thresh, _CMP_GE_OS);
        _mm256_storeu_ps(out + i, _mm256_and_ps(mask, one));
    }
    for (; i < n; ++i)
        out[i] = src[i] >= value ? 1.0F : 0.0F;

#elif defined(MAYAFLUX_ARCH_ARM64)
    const float32x4_t thresh = vdupq_n_f32(value);
    const float32x4_t one = vdupq_n_f32(1.0F);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(src + i);
        uint32x4_t mask = vcgeq_f32(v, thresh);
        vst1q_f32(out + i,
            vreinterpretq_f32_u32(vandq_u32(mask, vreinterpretq_u32_f32(one))));
    }
    for (; i < n; ++i)
        out[i] = src[i] >= value ? 1.0F : 0.0F;

#else
    P::transform(P::par_unseq, gray.begin(), gray.end(), dst.begin(),
        [value](float v) { return v >= value ? 1.0F : 0.0F; });
#endif
}

std::vector<float> threshold(std::span<const float> gray, float value)
{
    std::vector<float> out(gray.size());
    threshold(gray, out, value);
    return out;
}

void threshold_adaptive(
    std::span<const float> gray, std::span<float> dst,
    uint32_t w, uint32_t h,
    uint32_t block_size, float offset)
{
    const size_t n = static_cast<size_t>(w) * h;
    const auto half = static_cast<int32_t>(block_size / 2);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t idx) {
            const auto px = static_cast<int32_t>(idx % w);
            const auto py = static_cast<int32_t>(idx / w);

            float sum = 0.0F;
            int32_t count = 0;

            for (int32_t dy = -half; dy <= half; ++dy) {
                const int32_t ny = std::clamp(py + dy, 0, static_cast<int32_t>(h) - 1);
                for (int32_t dx = -half; dx <= half; ++dx) {
                    const int32_t nx = std::clamp(px + dx, 0, static_cast<int32_t>(w) - 1);
                    sum += gray[static_cast<size_t>(ny) * w + nx];
                    ++count;
                }
            }

            const float mean = sum / static_cast<float>(count);
            dst[idx] = gray[idx] >= (mean - offset) ? 1.0F : 0.0F;
        });
}

std::vector<float> threshold_adaptive(
    std::span<const float> gray, uint32_t w, uint32_t h,
    uint32_t block_size, float offset)
{
    std::vector<float> out(static_cast<size_t>(w) * h);
    threshold_adaptive(gray, out, w, h, block_size, offset);
    return out;
}

void threshold_otsu(std::span<const float> gray, std::span<float> dst)
{
    constexpr int32_t bins = 256;
    std::array<float, bins> hist {};

    for (float v : gray)
        hist[static_cast<int32_t>(std::clamp(v, 0.0F, 1.0F) * (bins - 1))]++;

    const auto total = static_cast<float>(gray.size());
    for (auto& h : hist)
        h /= total;

    float sum_all = 0.0F;
    for (int32_t i = 0; i < bins; ++i)
        sum_all += static_cast<float>(i) * hist[i];

    float sum_bg = 0.0F;
    float w_bg = 0.0F;
    float best_var = 0.0F;
    int32_t best_t = 0;

    for (int32_t t = 0; t < bins; ++t) {
        w_bg += hist[t];
        if (w_bg == 0.0F)
            continue;

        const float w_fg = 1.0F - w_bg;
        if (w_fg == 0.0F)
            break;

        sum_bg += static_cast<float>(t) * hist[t];

        const float mean_bg = sum_bg / w_bg;
        const float mean_fg = (sum_all - sum_bg) / w_fg;
        const float diff = mean_fg - mean_bg;
        const float var = w_bg * w_fg * diff * diff;

        if (var > best_var) {
            best_var = var;
            best_t = t;
        }
    }

    const float t_norm = static_cast<float>(best_t) / static_cast<float>(bins - 1);
    threshold(gray, dst, t_norm);
}

std::vector<float> threshold_otsu(std::span<const float> gray)
{
    std::vector<float> out(gray.size());
    threshold_otsu(gray, out);
    return out;
}

void normalize_inplace(std::span<float> data)
{
    if (data.empty())
        return;

    const Eigen::Index en = static_cast<Eigen::Index>(data.size());
    auto m = Eigen::Map<Eigen::ArrayXf>(data.data(), en);
    const float mn = m.minCoeff();
    const float mx = m.maxCoeff();
    const float range = mx - mn;
    if (range == 0.0F)
        return;

    const float inv = 1.0F / range;
    float* out = data.data();
    const size_t n = data.size();

#ifdef MAYAFLUX_ARCH_X64
    const __m256 v_mn = _mm256_set1_ps(mn);
    const __m256 v_inv = _mm256_set1_ps(inv);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(out + i);
        _mm256_storeu_ps(out + i, _mm256_mul_ps(_mm256_sub_ps(v, v_mn), v_inv));
    }
    for (; i < n; ++i)
        out[i] = (out[i] - mn) * inv;

#elif defined(MAYAFLUX_ARCH_ARM64)
    const float32x4_t v_mn = vdupq_n_f32(mn);
    const float32x4_t v_inv = vdupq_n_f32(inv);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(out + i);
        vst1q_f32(out + i, vmulq_f32(vsubq_f32(v, v_mn), v_inv));
    }
    for (; i < n; ++i)
        out[i] = (out[i] - mn) * inv;

#else
    P::transform(P::par_unseq, data.begin(), data.end(), data.begin(),
        [mn, inv](float v) { return (v - mn) * inv; });
#endif
}

void normalize_range_inplace(std::span<float> data, float lo, float hi)
{
    if (lo == hi)
        return;

    const float inv = 1.0F / (hi - lo);
    float* out = data.data();
    const size_t n = data.size();

#ifdef MAYAFLUX_ARCH_X64
    const __m256 v_lo = _mm256_set1_ps(lo);
    const __m256 v_inv = _mm256_set1_ps(inv);
    const __m256 v_zero = _mm256_setzero_ps();
    const __m256 v_one = _mm256_set1_ps(1.0F);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(out + i);
        v = _mm256_mul_ps(_mm256_sub_ps(v, v_lo), v_inv);
        v = _mm256_min_ps(_mm256_max_ps(v, v_zero), v_one);
        _mm256_storeu_ps(out + i, v);
    }
    for (; i < n; ++i)
        out[i] = std::clamp((out[i] - lo) * inv, 0.0F, 1.0F);

#elif defined(MAYAFLUX_ARCH_ARM64)
    const float32x4_t v_lo = vdupq_n_f32(lo);
    const float32x4_t v_inv = vdupq_n_f32(inv);
    const float32x4_t v_zero = vdupq_n_f32(0.0F);
    const float32x4_t v_one = vdupq_n_f32(1.0F);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(out + i);
        v = vmulq_f32(vsubq_f32(v, v_lo), v_inv);
        v = vminq_f32(vmaxq_f32(v, v_zero), v_one);
        vst1q_f32(out + i, v);
    }
    for (; i < n; ++i)
        out[i] = std::clamp((out[i] - lo) * inv, 0.0F, 1.0F);

#else
    P::transform(P::par_unseq, data.begin(), data.end(), data.begin(),
        [lo, inv](float v) { return std::clamp((v - lo) * inv, 0.0F, 1.0F); });
#endif
}

void downsample_2x(
    std::span<const float> src,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    uint32_t& new_w, uint32_t& new_h)
{
    new_w = w / 2;
    new_h = h / 2;

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, new_h).begin(),
        std::views::iota(uint32_t { 0 }, new_h).end(),
        [&](uint32_t oy) {
            const float* row0 = src.data() + static_cast<size_t>(oy * 2) * w;
            const float* row1 = src.data() + static_cast<size_t>(oy * 2 + 1) * w;
            float* drow = dst.data() + static_cast<size_t>(oy) * new_w;

#ifdef MAYAFLUX_ARCH_X64
            const __m256 quarter = _mm256_set1_ps(0.25F);
            uint32_t ox = 0;
            for (; ox + 8 <= new_w; ox += 8) {

                __m256 r0a = _mm256_loadu_ps(row0 + ox * 2); // px 0..7  of row0
                __m256 r0b = _mm256_loadu_ps(row0 + ox * 2 + 8); // px 8..15 of row0
                __m256 r1a = _mm256_loadu_ps(row1 + ox * 2);
                __m256 r1b = _mm256_loadu_ps(row1 + ox * 2 + 8);

                // horizontal add pairs: hadd gains (a0+a1, a2+a3, b0+b1, b2+b3 | ...)
                __m256 sum0 = _mm256_hadd_ps(r0a, r0b); // (r00+r01, r02+r03, r08+r09, r010+r011 | ...)
                __m256 sum1 = _mm256_hadd_ps(r1a, r1b);
                __m256 sum = _mm256_add_ps(sum0, sum1);

                __m256d perm_d = _mm256_permute4x64_pd(_mm256_castps_pd(sum), _MM_SHUFFLE(3, 1, 2, 0));
                __m256 result = _mm256_mul_ps(_mm256_castpd_ps(perm_d), quarter);
                _mm256_storeu_ps(drow + ox, result);
            }
            for (; ox < new_w; ++ox) {
                drow[ox] = (row0[ox * 2] + row0[ox * 2 + 1]
                               + row1[ox * 2] + row1[ox * 2 + 1])
                    * 0.25F;
            }

#elif defined(MAYAFLUX_ARCH_ARM64)
        const float32x4_t quarter = vdupq_n_f32(0.25F);
        uint32_t ox = 0;
        for (; ox + 4 <= new_w; ox += 4) {
            // vld2q_f32 loads 8 floats deinterleaved into 2 registers:
            // val[0] = even elements, val[1] = odd elements
            float32x4x2_t r0 = vld2q_f32(row0 + ox * 2);
            float32x4x2_t r1 = vld2q_f32(row1 + ox * 2);
            // r0.val[0] = [row0[0], row0[2], row0[4], row0[6]]
            // r0.val[1] = [row0[1], row0[3], row0[5], row0[7]]
            float32x4_t sum = vaddq_f32(
                vaddq_f32(r0.val[0], r0.val[1]),
                vaddq_f32(r1.val[0], r1.val[1]));
            vst1q_f32(drow + ox, vmulq_f32(sum, quarter));
        }
        for (; ox < new_w; ++ox) {
            drow[ox] = (row0[ox * 2] + row0[ox * 2 + 1]
                           + row1[ox * 2] + row1[ox * 2 + 1])
                * 0.25F;
        }

#else
        for (uint32_t ox = 0; ox < new_w; ++ox) {
            drow[ox] = (row0[ox * 2] + row0[ox * 2 + 1]
                           + row1[ox * 2] + row1[ox * 2 + 1])
                * 0.25F;
        }
#endif
        });
}

std::vector<float> downsample_2x(
    std::span<const float> src,
    uint32_t w, uint32_t h,
    uint32_t& new_w, uint32_t& new_h)
{
    new_w = w / 2;
    new_h = h / 2;
    std::vector<float> out(static_cast<size_t>(new_w) * new_h);
    downsample_2x(src, out, w, h, new_w, new_h);
    return out;
}

} // namespace MayaFlux::Kinesis::Vision
// NOLINTEND
