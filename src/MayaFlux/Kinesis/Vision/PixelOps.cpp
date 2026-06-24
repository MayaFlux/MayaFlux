#include "PixelOps.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

std::vector<float> rgba_to_gray(
    std::span<const float> rgba, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> out(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const size_t p = i * 4;
            out[i] = rgba[p] * 0.299F
                + rgba[p + 1] * 0.587F
                + rgba[p + 2] * 0.114F;
        });

    return out;
}

std::vector<float> rgba_to_hsv(
    std::span<const float> rgba, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> out(n * 3);

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

            out[i * 3] = h_val;
            out[i * 3 + 1] = cmax > 0.0F ? delta / cmax : 0.0F;
            out[i * 3 + 2] = cmax;
        });

    return out;
}

std::vector<float> gray_to_rgba(
    std::span<const float> gray, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> out(n * 4);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const float v = gray[i];
            out[i * 4] = v;
            out[i * 4 + 1] = v;
            out[i * 4 + 2] = v;
            out[i * 4 + 3] = 1.0F;
        });

    return out;
}

std::vector<float> threshold(std::span<const float> gray, float value)
{
    std::vector<float> out(gray.size());
    P::transform(P::par_unseq, gray.begin(), gray.end(), out.begin(),
        [value](float v) { return v >= value ? 1.0F : 0.0F; });
    return out;
}

std::vector<float> threshold_adaptive(
    std::span<const float> gray, uint32_t w, uint32_t h,
    uint32_t block_size, float offset)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<float> out(n);
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
            out[idx] = gray[idx] >= (mean - offset) ? 1.0F : 0.0F;
        });

    return out;
}

std::vector<float> threshold_otsu(std::span<const float> gray)
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
    return threshold(gray, t_norm);
}

void normalize_inplace(std::span<float> data)
{
    if (data.empty())
        return;

    const auto [mn, mx] = std::ranges::minmax(data);
    const float range = mx - mn;
    if (range == 0.0F)
        return;

    const float inv = 1.0F / range;
    P::transform(P::par_unseq, data.begin(), data.end(), data.begin(),
        [mn, inv](float v) { return (v - mn) * inv; });
}

void normalize_range_inplace(std::span<float> data, float lo, float hi)
{
    if (lo == hi)
        return;

    const float inv = 1.0F / (hi - lo);
    P::transform(P::par_unseq, data.begin(), data.end(), data.begin(),
        [lo, inv](float v) {
            return std::clamp((v - lo) * inv, 0.0F, 1.0F);
        });
}

std::vector<float> downsample_2x(
    std::span<const float> src, uint32_t w, uint32_t h,
    uint32_t& out_w, uint32_t& out_h)
{
    out_w = w / 2;
    out_h = h / 2;
    const size_t n = static_cast<size_t>(out_w) * out_h;
    std::vector<float> out(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t idx) {
            const uint32_t ox = static_cast<uint32_t>(idx % out_w);
            const uint32_t oy = static_cast<uint32_t>(idx / out_w);
            const uint32_t sx = ox * 2;
            const uint32_t sy = oy * 2;
            out[idx] = (src[static_cast<size_t>(sy) * w + sx]
                           + src[static_cast<size_t>(sy) * w + sx + 1]
                           + src[static_cast<size_t>(sy + 1) * w + sx]
                           + src[static_cast<size_t>(sy + 1) * w + sx + 1])
                * 0.25F;
        });

    return out;
}

} // namespace MayaFlux::Kinesis::Vision
