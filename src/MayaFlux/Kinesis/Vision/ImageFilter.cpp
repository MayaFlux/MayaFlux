#include "ImageFilter.hpp"

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include <Eigen/Core>

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

void filter_horizontal_planes(
    std::span<const float* const> src,
    std::span<float* const> dst,
    uint32_t w, uint32_t h,
    std::span<const float> kernel)
{
    constexpr size_t Unroll = 4;

    const auto half = static_cast<int32_t>(kernel.size() / 2);
    const auto iw = static_cast<int32_t>(w);
    const float* kdata = kernel.data();
    const size_t n = src.size();

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, h).begin(),
        std::views::iota(uint32_t { 0 }, h).end(),
        [&](uint32_t row) {
            const size_t row_off = static_cast<size_t>(row) * w;

            const float* s_ptr[32];
            float* d_ptr[32];
            for (size_t i = 0; i < n; ++i) {
                s_ptr[i] = src[i] + row_off;
                d_ptr[i] = dst[i] + row_off;
            }

            // left border
            for (size_t i = 0; i < n; ++i) {
                for (int32_t px = 0; px < std::min(half, iw); ++px) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        acc += s_ptr[i][std::clamp(px + k, 0, iw - 1)]
                            * kdata[static_cast<size_t>(k + half)];
                    }
                    d_ptr[i][px] = acc;
                }
            }

            const int32_t interior_end = iw - half;

#ifdef MAYAFLUX_ARCH_X64
            int32_t px = half;
            for (; px + 8 <= interior_end; px += 8) {
                size_t i = 0;
                for (; i + Unroll - 1 < n; i += Unroll) {
                    __m256 acc0 = _mm256_setzero_ps();
                    __m256 acc1 = _mm256_setzero_ps();
                    __m256 acc2 = _mm256_setzero_ps();
                    __m256 acc3 = _mm256_setzero_ps();
                    for (int32_t k = -half; k <= half; ++k) {
                        const __m256 kv = _mm256_set1_ps(kdata[static_cast<size_t>(k + half)]);
                        acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 0] + px + k), kv, acc0);
                        acc1 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 1] + px + k), kv, acc1);
                        acc2 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 2] + px + k), kv, acc2);
                        acc3 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 3] + px + k), kv, acc3);
                    }
                    _mm256_storeu_ps(d_ptr[i + 0] + px, acc0);
                    _mm256_storeu_ps(d_ptr[i + 1] + px, acc1);
                    _mm256_storeu_ps(d_ptr[i + 2] + px, acc2);
                    _mm256_storeu_ps(d_ptr[i + 3] + px, acc3);
                }
                for (; i < n; ++i) {
                    __m256 acc = _mm256_setzero_ps();
                    for (int32_t k = -half; k <= half; ++k) {
                        acc = _mm256_fmadd_ps(
                            _mm256_loadu_ps(s_ptr[i] + px + k),
                            _mm256_set1_ps(kdata[static_cast<size_t>(k + half)]),
                            acc);
                    }
                    _mm256_storeu_ps(d_ptr[i] + px, acc);
                }
            }
            // right border + scalar tail
            for (size_t i = 0; i < n; ++i) {
                for (int32_t px2 = px; px2 < iw; ++px2) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        acc += s_ptr[i][std::clamp(px2 + k, 0, iw - 1)]
                            * kdata[static_cast<size_t>(k + half)];
                    }
                    d_ptr[i][px2] = acc;
                }
            }

#elif defined(MAYAFLUX_ARCH_ARM64)
        int32_t px = half;
        for (; px + 4 <= interior_end; px += 4) {
            size_t i = 0;
            for (; i + Unroll - 1 < n; i += Unroll) {
                float32x4_t acc0 = vdupq_n_f32(0.0F);
                float32x4_t acc1 = vdupq_n_f32(0.0F);
                float32x4_t acc2 = vdupq_n_f32(0.0F);
                float32x4_t acc3 = vdupq_n_f32(0.0F);
                for (int32_t k = -half; k <= half; ++k) {
                    const float32x4_t kv = vdupq_n_f32(kdata[static_cast<size_t>(k + half)]);
                    acc0 = vmlaq_f32(acc0, vld1q_f32(s_ptr[i + 0] + px + k), kv);
                    acc1 = vmlaq_f32(acc1, vld1q_f32(s_ptr[i + 1] + px + k), kv);
                    acc2 = vmlaq_f32(acc2, vld1q_f32(s_ptr[i + 2] + px + k), kv);
                    acc3 = vmlaq_f32(acc3, vld1q_f32(s_ptr[i + 3] + px + k), kv);
                }
                vst1q_f32(d_ptr[i + 0] + px, acc0);
                vst1q_f32(d_ptr[i + 1] + px, acc1);
                vst1q_f32(d_ptr[i + 2] + px, acc2);
                vst1q_f32(d_ptr[i + 3] + px, acc3);
            }
            for (; i < n; ++i) {
                float32x4_t acc = vdupq_n_f32(0.0F);
                for (int32_t k = -half; k <= half; ++k)
                    acc = vmlaq_f32(acc,
                        vld1q_f32(s_ptr[i] + px + k),
                        vdupq_n_f32(kdata[static_cast<size_t>(k + half)]));
                vst1q_f32(d_ptr[i] + px, acc);
            }
        }
        for (size_t i = 0; i < n; ++i) {
            for (int32_t px2 = px; px2 < iw; ++px2) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += s_ptr[i][std::clamp(px2 + k, 0, iw - 1)]
                        * kdata[static_cast<size_t>(k + half)];
                d_ptr[i][px2] = acc;
            }
        }

#else
        for (size_t i = 0; i < n; ++i) {
            for (int32_t px = half; px < interior_end; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += s_ptr[i][px + k] * kdata[static_cast<size_t>(k + half)];
                d_ptr[i][px] = acc;
            }
            for (int32_t px = interior_end; px < iw; ++px) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k)
                    acc += s_ptr[i][std::clamp(px + k, 0, iw - 1)]
                        * kdata[static_cast<size_t>(k + half)];
                d_ptr[i][px] = acc;
            }
        }
#endif
        });
}

void filter_vertical_planes(
    std::span<const float* const> src,
    std::span<float* const> dst,
    uint32_t w, uint32_t h,
    std::span<const float> kernel)
{
    constexpr size_t Unroll = 4;

    const auto half = static_cast<int32_t>(kernel.size() / 2);
    const auto ih = static_cast<int32_t>(h);
    const auto iw = static_cast<int32_t>(w);
    const float* kdata = kernel.data();
    const size_t n = src.size();

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, h).begin(),
        std::views::iota(uint32_t { 0 }, h).end(),
        [&](uint32_t row) {
            const auto py = static_cast<int32_t>(row);
            const bool border = (py < half || py >= ih - half);
            const size_t row_off = static_cast<size_t>(row) * w;

            const float* s_ptr[32];
            float* d_ptr[32];
            for (size_t i = 0; i < n; ++i) {
                s_ptr[i] = src[i];
                d_ptr[i] = dst[i] + row_off;
            }

#ifdef MAYAFLUX_ARCH_X64
            int32_t px = 0;
            if (!border) {
                for (; px + 8 <= iw; px += 8) {
                    size_t i = 0;
                    for (; i + Unroll - 1 < n; i += Unroll) {
                        __m256 acc0 = _mm256_setzero_ps();
                        __m256 acc1 = _mm256_setzero_ps();
                        __m256 acc2 = _mm256_setzero_ps();
                        __m256 acc3 = _mm256_setzero_ps();
                        for (int32_t k = -half; k <= half; ++k) {
                            const size_t off = static_cast<size_t>(py + k) * w
                                + static_cast<size_t>(px);
                            const __m256 kv = _mm256_set1_ps(kdata[static_cast<size_t>(k + half)]);
                            acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 0] + off), kv, acc0);
                            acc1 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 1] + off), kv, acc1);
                            acc2 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 2] + off), kv, acc2);
                            acc3 = _mm256_fmadd_ps(_mm256_loadu_ps(s_ptr[i + 3] + off), kv, acc3);
                        }
                        _mm256_storeu_ps(d_ptr[i + 0] + px, acc0);
                        _mm256_storeu_ps(d_ptr[i + 1] + px, acc1);
                        _mm256_storeu_ps(d_ptr[i + 2] + px, acc2);
                        _mm256_storeu_ps(d_ptr[i + 3] + px, acc3);
                    }
                    for (; i < n; ++i) {
                        __m256 acc = _mm256_setzero_ps();
                        for (int32_t k = -half; k <= half; ++k) {
                            const size_t off = static_cast<size_t>(py + k) * w
                                + static_cast<size_t>(px);
                            acc = _mm256_fmadd_ps(
                                _mm256_loadu_ps(s_ptr[i] + off),
                                _mm256_set1_ps(kdata[static_cast<size_t>(k + half)]),
                                acc);
                        }
                        _mm256_storeu_ps(d_ptr[i] + px, acc);
                    }
                }
            }
            for (; px < iw; ++px) {
                for (size_t i = 0; i < n; ++i) {
                    float acc = 0.0F;
                    for (int32_t k = -half; k <= half; ++k) {
                        const int32_t ny = border ? std::clamp(py + k, 0, ih - 1) : py + k;
                        const size_t off = static_cast<size_t>(ny) * w + static_cast<size_t>(px);
                        acc += s_ptr[i][off] * kdata[static_cast<size_t>(k + half)];
                    }
                    d_ptr[i][px] = acc;
                }
            }

#elif defined(MAYAFLUX_ARCH_ARM64)
        int32_t px = 0;
        if (!border) {
            for (; px + 4 <= iw; px += 4) {
                size_t i = 0;
                for (; i + Unroll - 1 < n; i += Unroll) {
                    float32x4_t acc0 = vdupq_n_f32(0.0F);
                    float32x4_t acc1 = vdupq_n_f32(0.0F);
                    float32x4_t acc2 = vdupq_n_f32(0.0F);
                    float32x4_t acc3 = vdupq_n_f32(0.0F);
                    for (int32_t k = -half; k <= half; ++k) {
                        const size_t off = static_cast<size_t>(py + k) * w
                            + static_cast<size_t>(px);
                        const float32x4_t kv = vdupq_n_f32(kdata[static_cast<size_t>(k + half)]);
                        acc0 = vmlaq_f32(acc0, vld1q_f32(s_ptr[i + 0] + off), kv);
                        acc1 = vmlaq_f32(acc1, vld1q_f32(s_ptr[i + 1] + off), kv);
                        acc2 = vmlaq_f32(acc2, vld1q_f32(s_ptr[i + 2] + off), kv);
                        acc3 = vmlaq_f32(acc3, vld1q_f32(s_ptr[i + 3] + off), kv);
                    }
                    vst1q_f32(d_ptr[i + 0] + px, acc0);
                    vst1q_f32(d_ptr[i + 1] + px, acc1);
                    vst1q_f32(d_ptr[i + 2] + px, acc2);
                    vst1q_f32(d_ptr[i + 3] + px, acc3);
                }
                for (; i < n; ++i) {
                    float32x4_t acc = vdupq_n_f32(0.0F);
                    for (int32_t k = -half; k <= half; ++k) {
                        const size_t off = static_cast<size_t>(py + k) * w
                            + static_cast<size_t>(px);
                        acc = vmlaq_f32(acc,
                            vld1q_f32(s_ptr[i] + off),
                            vdupq_n_f32(kdata[static_cast<size_t>(k + half)]));
                    }
                    vst1q_f32(d_ptr[i] + px, acc);
                }
            }
        }
        for (; px < iw; ++px) {
            for (size_t i = 0; i < n; ++i) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t ny = border ? std::clamp(py + k, 0, ih - 1) : py + k;
                    const size_t off = static_cast<size_t>(ny) * w + static_cast<size_t>(px);
                    acc += s_ptr[i][off] * kdata[static_cast<size_t>(k + half)];
                }
                d_ptr[i][px] = acc;
            }
        }

#else
        for (int32_t px = 0; px < iw; ++px) {
            for (size_t i = 0; i < n; ++i) {
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t ny = std::clamp(py + k, 0, ih - 1);
                    const size_t off = static_cast<size_t>(ny) * w + static_cast<size_t>(px);
                    acc += s_ptr[i][off] * kdata[static_cast<size_t>(k + half)];
                }
                d_ptr[i][px] = acc;
            }
        }
#endif
        });
}

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
