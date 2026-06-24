#include "Filter.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    void convolve_horizontal(
        std::span<const float> src, std::span<float> dst,
        uint32_t w, uint32_t h,
        std::span<const float> kernel)
    {
        const auto half = static_cast<int32_t>(kernel.size() / 2);
        const size_t n = static_cast<size_t>(w) * h;

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t idx) {
                const auto py = static_cast<int32_t>(idx / w);
                const auto px = static_cast<int32_t>(idx % w);
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t nx = std::clamp(px + k, 0, static_cast<int32_t>(w) - 1);
                    acc += src[static_cast<size_t>(py) * w + nx]
                        * kernel[static_cast<size_t>(k + half)];
                }
                dst[idx] = acc;
            });
    }

    void convolve_vertical(
        std::span<const float> src, std::span<float> dst,
        uint32_t w, uint32_t h,
        std::span<const float> kernel)
    {
        const auto half = static_cast<int32_t>(kernel.size() / 2);
        const size_t n = static_cast<size_t>(w) * h;

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t idx) {
                const auto py = static_cast<int32_t>(idx / w);
                const auto px = static_cast<int32_t>(idx % w);
                float acc = 0.0F;
                for (int32_t k = -half; k <= half; ++k) {
                    const int32_t ny = std::clamp(py + k, 0, static_cast<int32_t>(h) - 1);
                    acc += src[static_cast<size_t>(ny) * w + px]
                        * kernel[static_cast<size_t>(k + half)];
                }
                dst[idx] = acc;
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

    convolve_horizontal(src, tmp, w, h, kernel_x);
    convolve_vertical(tmp, out, w, h, kernel_y);

    return out;
}

std::vector<float> gaussian_blur(
    std::span<const float> src, uint32_t w, uint32_t h, float sigma)
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
    for (auto& v : kernel)
        v /= sum;

    return filter_separable(src, w, h, kernel, kernel);
}

} // namespace MayaFlux::Kinesis::Vision
