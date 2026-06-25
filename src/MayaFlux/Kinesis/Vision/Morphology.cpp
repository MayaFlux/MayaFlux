#include "Morphology.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"
#include <Eigen/Core>

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    inline bool is_fg(float v) noexcept { return v >= 0.5F; }

    std::vector<float> morph_op(
        std::span<const float> mask, uint32_t w, uint32_t h,
        uint32_t radius, bool dilating)
    {
        const size_t n = static_cast<size_t>(w) * h;
        const auto r = static_cast<int32_t>(radius);
        std::vector<float> out(n);

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t idx) {
                const auto px = static_cast<int32_t>(idx % w);
                const auto py = static_cast<int32_t>(idx / w);

                bool result = !dilating;

                for (int32_t dy = -r; dy <= r && result == !dilating; ++dy) {
                    const int32_t ny = std::clamp(py + dy, 0, static_cast<int32_t>(h) - 1);
                    for (int32_t dx = -r; dx <= r && result == !dilating; ++dx) {
                        const int32_t nx = std::clamp(px + dx, 0, static_cast<int32_t>(w) - 1);
                        const bool fg = is_fg(mask[static_cast<size_t>(ny) * w + nx]);
                        if (dilating && fg) {
                            result = true;
                        } else if (!dilating && !fg) {
                            result = false;
                        }
                    }
                }

                out[idx] = result ? 1.0F : 0.0F;
            });

        return out;
    }

    void morph_op_into(
        std::span<const float> mask, std::span<float> out,
        uint32_t w, uint32_t h, uint32_t radius, bool dilating)
    {
        const size_t n = static_cast<size_t>(w) * h;
        const auto r = static_cast<int32_t>(radius);

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t idx) {
                const auto px = static_cast<int32_t>(idx % w);
                const auto py = static_cast<int32_t>(idx / w);
                bool result = !dilating;

                for (int32_t dy = -r; dy <= r && result == !dilating; ++dy) {
                    const int32_t ny = std::clamp(py + dy, 0, static_cast<int32_t>(h) - 1);

                    for (int32_t dx = -r; dx <= r && result == !dilating; ++dx) {
                        const int32_t nx = std::clamp(px + dx, 0, static_cast<int32_t>(w) - 1);
                        const bool fg = is_fg(mask[static_cast<size_t>(ny) * w + nx]);

                        if (dilating && fg) {
                            result = true;
                        } else if (!dilating && !fg) {
                            result = false;
                        }
                    }
                }
                out[idx] = result ? 1.0F : 0.0F;
            });
    }

} // namespace

void erode(std::span<const float> mask, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius)
{
    morph_op_into(mask, dst, w, h, radius, false);
}

std::vector<float> erode(std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    std::vector<float> out(static_cast<size_t>(w) * h);
    erode(mask, out, w, h, radius);
    return out;
}

void dilate(std::span<const float> mask, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius)
{
    morph_op_into(mask, dst, w, h, radius, true);
}

std::vector<float> dilate(std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    std::vector<float> out(static_cast<size_t>(w) * h);
    dilate(mask, out, w, h, radius);
    return out;
}

void open(std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius)
{
    erode(mask, tmp, w, h, radius);
    dilate(tmp, dst, w, h, radius);
}

std::vector<float> open(std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    std::vector<float> tmp(static_cast<size_t>(w) * h);
    std::vector<float> out(tmp.size());
    open(mask, tmp, out, w, h, radius);
    return out;
}

void close(std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius)
{
    dilate(mask, tmp, w, h, radius);
    erode(tmp, dst, w, h, radius);
}

std::vector<float> close(std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    std::vector<float> tmp(static_cast<size_t>(w) * h);
    std::vector<float> out(tmp.size());
    close(mask, tmp, out, w, h, radius);
    return out;
}

void morph_gradient(std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius)
{
    const auto en = static_cast<Eigen::Index>(static_cast<size_t>(w) * h);
    dilate(mask, tmp, w, h, radius);
    erode(mask, dst, w, h, radius);

    Eigen::Map<Eigen::ArrayXf>(dst.data(), en) = Eigen::Map<const Eigen::ArrayXf>(tmp.data(), en)
        - Eigen::Map<const Eigen::ArrayXf>(dst.data(), en);
}

std::vector<float> morph_gradient(std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    std::vector<float> tmp(static_cast<size_t>(w) * h);
    std::vector<float> out(tmp.size());
    morph_gradient(mask, tmp, out, w, h, radius);
    return out;
}

} // namespace MayaFlux::Kinesis::Vision
