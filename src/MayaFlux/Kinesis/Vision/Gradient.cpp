#include "Gradient.hpp"

#include "Filter.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include <Eigen/Core>

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    GradientResult gradient_from_kernels(
        std::span<const float> gray, uint32_t w, uint32_t h,
        std::span<const float> kx, std::span<const float> ky)
    {
        const size_t n = static_cast<size_t>(w) * h;

        static constexpr float smooth[] = { 1.0F, 2.0F, 1.0F };
        auto dx = filter_separable(gray, w, h, kx, smooth);
        auto dy = filter_separable(gray, w, h, smooth, ky);

        std::vector<float> mag(n);
        std::vector<float> ang(n);

        float max_mag = 0.0F;

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t i) {
                mag[i] = std::sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
                ang[i] = std::atan2(dy[i], dx[i]);
            });

        const float peak = *std::ranges::max_element(mag);
        if (peak > 0.0F) {
            const float inv = 1.0F / peak;
            P::transform(P::par_unseq, mag.begin(), mag.end(), mag.begin(),
                [inv](float v) { return v * inv; });
        }

        return { .dx = std::move(dx), .dy = std::move(dy), .magnitude = std::move(mag), .angle = std::move(ang) };
    }

    void gradient_into_buffers(
        std::span<const float> gray,
        std::span<float> dx,
        std::span<float> dy,
        std::span<float> tmp,
        uint32_t w, uint32_t h,
        std::span<const float> kx,
        std::span<const float> ky)
    {
        static constexpr float smooth[] = { 1.0F, 2.0F, 1.0F };
        filter_separable(gray, tmp, dx, w, h, kx, smooth);
        filter_separable(gray, tmp, dy, w, h, smooth, ky);
    }

} // namespace

void sobel(
    std::span<const float> gray,
    std::span<float> dx, std::span<float> dy, std::span<float> tmp,
    uint32_t w, uint32_t h)
{
    static constexpr float kx[] = { -1.0F, 0.0F, 1.0F };
    static constexpr float ky[] = { -1.0F, 0.0F, 1.0F };
    gradient_into_buffers(gray, dx, dy, tmp, w, h, kx, ky);
}

GradientResult sobel(std::span<const float> gray, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    GradientResult r;
    r.dx.resize(n);
    r.dy.resize(n);
    std::vector<float> tmp(n);
    sobel(gray, r.dx, r.dy, tmp, w, h);

    const auto en = static_cast<Eigen::Index>(n);
    auto dx = Eigen::Map<const Eigen::ArrayXf>(r.dx.data(), en);
    auto dy = Eigen::Map<const Eigen::ArrayXf>(r.dy.data(), en);
    r.magnitude.resize(n);
    r.angle.resize(n);

    Eigen::Map<Eigen::ArrayXf>(r.magnitude.data(), en) = (dx.square() + dy.square()).sqrt();
    const float peak = Eigen::Map<const Eigen::ArrayXf>(r.magnitude.data(), en).maxCoeff();
    if (peak > 0.0F)
        Eigen::Map<Eigen::ArrayXf>(r.magnitude.data(), en) /= peak;

    P::transform(P::par_unseq, r.dx.begin(), r.dx.end(), r.dy.begin(), r.angle.begin(),
        [](float gx, float gy) { return std::atan2(gy, gx); });
    return r;
}

void scharr(
    std::span<const float> gray,
    std::span<float> dx, std::span<float> dy, std::span<float> tmp,
    uint32_t w, uint32_t h)
{
    static constexpr float kx[] = { -3.0F, 0.0F, 3.0F };
    static constexpr float ky[] = { -3.0F, 0.0F, 3.0F };
    gradient_into_buffers(gray, dx, dy, tmp, w, h, kx, ky);
}

GradientResult scharr(std::span<const float> gray, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    GradientResult r;
    r.dx.resize(n);
    r.dy.resize(n);
    std::vector<float> tmp(n);
    scharr(gray, r.dx, r.dy, tmp, w, h);

    const auto en = static_cast<Eigen::Index>(n);
    auto dx = Eigen::Map<const Eigen::ArrayXf>(r.dx.data(), en);
    auto dy = Eigen::Map<const Eigen::ArrayXf>(r.dy.data(), en);
    r.magnitude.resize(n);
    r.angle.resize(n);

    Eigen::Map<Eigen::ArrayXf>(r.magnitude.data(), en) = (dx.square() + dy.square()).sqrt();
    const float peak = Eigen::Map<const Eigen::ArrayXf>(r.magnitude.data(), en).maxCoeff();
    if (peak > 0.0F)
        Eigen::Map<Eigen::ArrayXf>(r.magnitude.data(), en) /= peak;

    P::transform(P::par_unseq, r.dx.begin(), r.dx.end(), r.dy.begin(), r.angle.begin(),
        [](float gx, float gy) { return std::atan2(gy, gx); });
    return r;
}

std::vector<float> canny(
    std::span<const float> gray, uint32_t w, uint32_t h,
    float sigma, float low_threshold, float high_threshold)
{
    const size_t n = static_cast<size_t>(w) * h;

    std::vector<float> blurred;
    std::span<const float> src = gray;
    if (sigma > 0.0F) {
        blurred = gaussian_blur(gray, w, h, sigma);
        src = blurred;
    }

    auto grad = sobel(src, w, h);

    std::vector<float> nms(n, 0.0F);
    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t idx) {
            const auto px = static_cast<int32_t>(idx % w);
            const auto py = static_cast<int32_t>(idx / w);

            if (px == 0 || py == 0
                || px == static_cast<int32_t>(w) - 1
                || py == static_cast<int32_t>(h) - 1)
                return;

            const float angle = grad.angle[idx];
            const float pi = std::numbers::pi_v<float>;

            float norm_angle = angle < 0.0F ? angle + pi : angle;
            norm_angle = norm_angle / pi * 4.0F;

            int32_t ox = 0, oy = 0;
            if (norm_angle < 0.5F || norm_angle >= 3.5F) {
                ox = 1;
            } else if (norm_angle < 1.5F) {
                ox = 1;
                oy = 1;
            } else if (norm_angle < 2.5F) {
                oy = 1;
            } else {
                ox = -1;
                oy = 1;
            }

            const float m = grad.magnitude[idx];
            const float m_pos = grad.magnitude[static_cast<size_t>(py + oy) * w + static_cast<size_t>(px + ox)];
            const float m_neg = grad.magnitude[static_cast<size_t>(py - oy) * w + static_cast<size_t>(px - ox)];

            if (m >= m_pos && m >= m_neg)
                nms[idx] = m;
        });

    std::vector<float> out(n, 0.0F);
    std::vector<uint8_t> strong(n, 0);
    std::vector<uint8_t> weak(n, 0);

    for (size_t i = 0; i < n; ++i) {
        if (nms[i] >= high_threshold) {
            strong[i] = 1;
        } else if (nms[i] >= low_threshold) {
            weak[i] = 1;
        }
    }

    std::vector<size_t> stack;
    for (size_t i = 0; i < n; ++i) {
        if (strong[i])
            stack.push_back(i);
    }

    while (!stack.empty()) {
        const size_t idx = stack.back();
        stack.pop_back();
        out[idx] = 1.0F;

        const auto px = static_cast<int32_t>(idx % w);
        const auto py = static_cast<int32_t>(idx / w);

        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0)
                    continue;
                const int32_t nx = px + dx;
                const int32_t ny = py + dy;
                if (nx < 0 || ny < 0
                    || nx >= static_cast<int32_t>(w)
                    || ny >= static_cast<int32_t>(h))
                    continue;
                const size_t ni = static_cast<size_t>(ny) * w + nx;
                if (weak[ni] && out[ni] == 0.0F)
                    stack.push_back(ni);
            }
        }
    }

    return out;
}

void canny(
    std::span<const float> gray,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    float sigma, float low_threshold, float high_threshold)
{
    auto result = canny(gray, w, h, sigma, low_threshold, high_threshold);
    std::ranges::copy(result, dst.begin());
}

} // namespace MayaFlux::Kinesis::Vision
