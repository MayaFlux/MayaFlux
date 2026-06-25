#include "Harris.hpp"

#include "Gradient.hpp"
#include "ImageFilter.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

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
