#include "Filter.hpp"

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

namespace {

    void convolve_horizontal(
        std::span<const float> src, std::span<float> dst,
        uint32_t w, uint32_t h,
        std::span<const float> kernel)
    {
        const auto half = static_cast<int32_t>(kernel.size() / 2);
        const auto iw = static_cast<int32_t>(w);

        P::for_each(P::par_unseq,
            std::views::iota(uint32_t { 0 }, h).begin(),
            std::views::iota(uint32_t { 0 }, h).end(),
            [&](uint32_t row) {
                const float* src_row = src.data() + static_cast<size_t>(row) * w;
                float* dst_row = dst.data() + static_cast<size_t>(row) * w;

                // left border: pixels [0, half) require clamped reads
                for (int32_t px = 0; px < std::min(half, iw); ++px) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        acc += src_row[std::clamp(px + k, 0, iw - 1)]
                            * kernel[static_cast<size_t>(k + half)];
                    }
                    dst_row[px] = acc;
                }

                const int32_t interior_end = iw - half;

#ifdef MAYAFLUX_ARCH_X64
                // AVX2: 8 output pixels per iteration
                // interior: all kernel taps land in [0, w) without clamping
                int32_t px = half;
                for (; px + 8 <= interior_end; px += 8) {
                    __m256 acc = _mm256_setzero_ps();
                    for (int32_t k = -half; k <= half; ++k) {
                        const __m256 sv = _mm256_loadu_ps(src_row + px + k);
                        const __m256 kv = _mm256_set1_ps(kernel[static_cast<size_t>(k + half)]);
                        acc = _mm256_fmadd_ps(sv, kv, acc);
                    }
                    _mm256_storeu_ps(dst_row + px, acc);
                }
                // scalar tail (fewer than 8 pixels remain in interior + right border)
                for (; px < iw; ++px) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        acc += src_row[std::clamp(px + k, 0, iw - 1)]
                            * kernel[static_cast<size_t>(k + half)];
                    }
                    dst_row[px] = acc;
                }

#elif defined(MAYAFLUX_ARCH_ARM64)
            // NEON: 4 output pixels per iteration
            int32_t px = half;
            for (; px + 4 <= interior_end; px += 4) {
                float32x4_t acc = vdupq_n_f32(0.0F);
                for (int32_t k = -half; k <= half; ++k) {
                    const float32x4_t sv = vld1q_f32(src_row + px + k);
                    const float32x4_t kv = vdupq_n_f32(kernel[static_cast<size_t>(k + half)]);
                    acc = vmlaq_f32(acc, sv, kv);
                }
                vst1q_f32(dst_row + px, acc);
            }
            for (; px < iw; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += src_row[std::clamp(px + k, 0, iw - 1)]
                        * kernel[static_cast<size_t>(k + half)];
                dst_row[px] = acc;
            }

#else
            // scalar interior (no clamping needed)
            for (int32_t px = half; px < interior_end; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += src_row[px + k] * kernel[static_cast<size_t>(k + half)];
                dst_row[px] = acc;
            }
            // right border
            for (int32_t px = interior_end; px < iw; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += src_row[std::clamp(px + k, 0, iw - 1)]
                        * kernel[static_cast<size_t>(k + half)];
                dst_row[px] = acc;
            }
#endif
            });
    }

    void convolve_vertical(
        std::span<const float> src, std::span<float> dst,
        uint32_t w, uint32_t h,
        std::span<const float> kernel)
    {
        const auto half = static_cast<int32_t>(kernel.size() / 2);
        const auto ih = static_cast<int32_t>(h);
        const auto iw = static_cast<int32_t>(w);

        P::for_each(P::par_unseq,
            std::views::iota(uint32_t { 0 }, h).begin(),
            std::views::iota(uint32_t { 0 }, h).end(),
            [&](uint32_t row) {
                const auto py = static_cast<int32_t>(row);
                float* dst_row = dst.data() + static_cast<size_t>(row) * w;
                const bool border = (py < half || py >= ih - half);

#ifdef MAYAFLUX_ARCH_X64
                int32_t px = 0;
                if (!border) {
                    // interior row: no clamping needed, 8 columns at a time
                    for (; px + 8 <= iw; px += 8) {
                        __m256 acc = _mm256_setzero_ps();
                        for (int32_t k = -half; k <= half; ++k) {
                            const float* src_row = src.data()
                                + static_cast<size_t>(py + k) * w + px;
                            const __m256 sv = _mm256_loadu_ps(src_row);
                            const __m256 kv = _mm256_set1_ps(
                                kernel[static_cast<size_t>(k + half)]);
                            acc = _mm256_fmadd_ps(sv, kv, acc);
                        }
                        _mm256_storeu_ps(dst_row + px, acc);
                    }
                }
                // scalar tail and all border rows
                for (; px < iw; ++px) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        const int32_t ny = border
                            ? std::clamp(py + k, 0, ih - 1)
                            : py + k;
                        acc += src[static_cast<size_t>(ny) * w + px]
                            * kernel[static_cast<size_t>(k + half)];
                    }
                    dst_row[px] = acc;
                }

#elif defined(MAYAFLUX_ARCH_ARM64)
            int32_t px = 0;
            if (!border) {
                for (; px + 4 <= iw; px += 4) {
                    float32x4_t acc = vdupq_n_f32(0.0F);
                    for (int32_t k = -half; k <= half; ++k) {
                        const float* src_row = src.data()
                            + static_cast<size_t>(py + k) * w + px;
                        const float32x4_t sv = vld1q_f32(src_row);
                        const float32x4_t kv = vdupq_n_f32(
                            kernel[static_cast<size_t>(k + half)]);
                        acc = vmlaq_f32(acc, sv, kv);
                    }
                    vst1q_f32(dst_row + px, acc);
                }
            }
            for (; px < iw; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t ny = border
                        ? std::clamp(py + k, 0, ih - 1)
                        : py + k;
                    acc += src[static_cast<size_t>(ny) * w + px]
                        * kernel[static_cast<size_t>(k + half)];
                }
                dst_row[px] = acc;
            }

#else
            for (int32_t px = 0; px < iw; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t ny = std::clamp(py + k, 0, ih - 1);
                    acc += src[static_cast<size_t>(ny) * w + px]
                        * kernel[static_cast<size_t>(k + half)];
                }
                dst_row[px] = acc;
            }
#endif
            });
    }

} // namespace

std::vector<float> filter_separable(
    std::span<const float> src, uint32_t w, uint32_t h,
    std::span<const float> kernel_x, std::span<const float> kernel_y)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> tmp(n);
    std::vector<float> out(n);
    filter_separable(src, tmp, out, w, h, kernel_x, kernel_y);
    return out;
}

void filter_separable(
    std::span<const float> src,
    std::span<float> tmp,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    std::span<const float> kernel_x,
    std::span<const float> kernel_y)
{
    convolve_horizontal(src, tmp, w, h, kernel_x);
    convolve_vertical(tmp, dst, w, h, kernel_y);
}

std::vector<float> gaussian_blur(
    std::span<const float> src, uint32_t w, uint32_t h, float sigma)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> tmp(n);
    std::vector<float> out(n);
    gaussian_blur(src, tmp, out, w, h, sigma);
    return out;
}

void gaussian_blur(
    std::span<const float> src,
    std::span<float> tmp,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    float sigma)
{
    const auto radius = static_cast<int32_t>(std::ceil(3.0F * sigma));
    const int32_t size = 2 * radius + 1;

    std::vector<float> kernel(static_cast<size_t>(size));
    float sum = 0.0F;

    const float inv_2s2 = 1.0F / (2.0F * sigma * sigma);
    for (int32_t i = -radius; i <= radius; ++i) {
        const float v = std::exp(-static_cast<float>(i * i) * inv_2s2);
        kernel[static_cast<size_t>(i + radius)] = v;
        sum += v;
    }

    auto kmap = Eigen::Map<Eigen::ArrayXf>(kernel.data(), static_cast<Eigen::Index>(kernel.size()));
    kmap /= sum;
    filter_separable(src, tmp, dst, w, h, kernel, kernel);
}

} // namespace MayaFlux::Kinesis::Vision
