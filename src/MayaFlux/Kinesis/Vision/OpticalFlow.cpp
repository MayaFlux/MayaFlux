#include "OpticalFlow.hpp"

#include "Gradient.hpp"
#include "ImageFilter.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    // =========================================================================
    // Bilinear sample with clamp-to-edge
    // =========================================================================

    float sample(std::span<const float> img, uint32_t w, uint32_t h,
        float x, float y)
    {
        x = std::clamp(x, 0.0F, static_cast<float>(w) - 1.001F);
        y = std::clamp(y, 0.0F, static_cast<float>(h) - 1.001F);

        const auto ix = static_cast<int32_t>(x);
        const auto iy = static_cast<int32_t>(y);
        const float fx = x - static_cast<float>(ix);
        const float fy = y - static_cast<float>(iy);

        const int32_t ix1 = std::min(ix + 1, static_cast<int32_t>(w) - 1);
        const int32_t iy1 = std::min(iy + 1, static_cast<int32_t>(h) - 1);

        const float v00 = img[static_cast<size_t>(iy) * w + ix];
        const float v10 = img[static_cast<size_t>(iy) * w + ix1];
        const float v01 = img[static_cast<size_t>(iy1) * w + ix];
        const float v11 = img[static_cast<size_t>(iy1) * w + ix1];

        return v00 * (1.0F - fx) * (1.0F - fy)
            + v10 * fx * (1.0F - fy)
            + v01 * (1.0F - fx) * fy
            + v11 * fx * fy;
    }

    // =========================================================================
    // Lucas-Kanade for a single point
    // =========================================================================

    TrackResult lk_point(
        std::span<const float> prev,
        std::span<const float> curr,
        std::span<const float> dx,
        std::span<const float> dy,
        uint32_t w, uint32_t h,
        glm::vec2 pt,
        uint32_t window_radius,
        uint32_t max_iterations,
        float eigen_threshold,
        float error_threshold)
    {
        const auto pw = static_cast<float>(w);
        const auto ph = static_cast<float>(h);

        float px = pt.x * pw - 0.5F;
        float py = pt.y * ph - 0.5F;

        const auto r = static_cast<int32_t>(window_radius);

        float sxx = 0.0F, syy = 0.0F, sxy = 0.0F;

        for (int32_t wy = -r; wy <= r; ++wy) {
            for (int32_t wx = -r; wx <= r; ++wx) {
                const float sx = std::clamp(px + wx, 0.0F, pw - 1.0F);
                const float sy = std::clamp(py + wy, 0.0F, ph - 1.0F);
                const auto ix = static_cast<int32_t>(sx);
                const auto iy = static_cast<int32_t>(sy);
                const size_t ni = static_cast<size_t>(iy) * w + ix;

                const float gx = dx[ni];
                const float gy = dy[ni];
                sxx += gx * gx;
                syy += gy * gy;
                sxy += gx * gy;
            }
        }

        const float trace = sxx + syy;
        const float det = sxx * syy - sxy * sxy;
        const float disc = std::sqrt(std::max(0.0F,
            trace * trace * 0.25F - det));
        const float min_eig = trace * 0.5F - disc;

        if (min_eig < eigen_threshold)
            return { .position = pt, .error = 1.0F, .tracked = false };

        const float inv_det = 1.0F / (det + 1e-10F);

        float vx = 0.0F, vy = 0.0F;

        for (uint32_t iter = 0; iter < max_iterations; ++iter) {
            float bx = 0.0F, by = 0.0F;

            for (int32_t wy = -r; wy <= r; ++wy) {
                for (int32_t wx = -r; wx <= r; ++wx) {
                    const float sx = std::clamp(px + wx, 0.0F, pw - 1.0F);
                    const float sy = std::clamp(py + wy, 0.0F, ph - 1.0F);
                    const auto ix = static_cast<int32_t>(sx);
                    const auto iy = static_cast<int32_t>(sy);
                    const size_t ni = static_cast<size_t>(iy) * w + ix;

                    const float gx = dx[ni];
                    const float gy = dy[ni];

                    const float p_val = prev[ni];
                    const float c_val = sample(curr, w, h,
                        px + wx + vx, py + wy + vy);
                    const float it = p_val - c_val;

                    bx += it * gx;
                    by += it * gy;
                }
            }

            const float dvx = (syy * bx - sxy * by) * inv_det;
            const float dvy = (sxx * by - sxy * bx) * inv_det;

            vx += dvx;
            vy += dvy;

            if (dvx * dvx + dvy * dvy < 0.03F * 0.03F)
                break;
        }

        const float nx = px + vx;
        const float ny = py + vy;

        if (nx < 0.0F || ny < 0.0F
            || nx >= pw || ny >= ph)
            return { .position = pt, .error = 1.0F, .tracked = false };

        float error = 0.0F;
        int32_t count = 0;
        for (int32_t wy = -r; wy <= r; ++wy) {
            for (int32_t wx = -r; wx <= r; ++wx) {
                const float sx = std::clamp(px + wx, 0.0F, pw - 1.0F);
                const float sy = std::clamp(py + wy, 0.0F, ph - 1.0F);
                const auto ix = static_cast<int32_t>(sx);
                const auto iy = static_cast<int32_t>(sy);
                const float p_val = prev[static_cast<size_t>(iy) * w + ix];
                const float c_val = sample(curr, w, h,
                    nx + wx, ny + wy);
                const float diff = p_val - c_val;
                error += diff * diff;
                ++count;
            }
        }
        error = std::sqrt(error / static_cast<float>(count));

        if (error > error_threshold)
            return { .position = pt, .error = error, .tracked = false };

        return {
            .position = {
                (nx + 0.5F) / pw,
                (ny + 0.5F) / ph },
            .error = error,
            .tracked = true,
        };
    }

} // namespace

std::vector<TrackResult> track_keypoints(
    std::span<const float> prev_gray,
    std::span<const float> curr_gray,
    uint32_t w, uint32_t h,
    std::span<const glm::vec2> prev_points,
    uint32_t window_radius,
    uint32_t max_iterations,
    float eigen_threshold,
    float error_threshold)
{
    auto grad = sobel(prev_gray, w, h);

    const size_t np = prev_points.size();
    std::vector<TrackResult> results(np);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, np).begin(),
        std::views::iota(size_t { 0 }, np).end(),
        [&](size_t i) {
            results[i] = lk_point(
                prev_gray, curr_gray,
                grad.dx, grad.dy,
                w, h,
                prev_points[i],
                window_radius,
                max_iterations,
                eigen_threshold,
                error_threshold);
        });

    return results;
}

} // namespace MayaFlux::Kinesis::Vision
