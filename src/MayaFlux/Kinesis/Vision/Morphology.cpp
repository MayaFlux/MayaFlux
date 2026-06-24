#include "Morphology.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

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

} // namespace

std::vector<float> erode(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    return morph_op(mask, w, h, radius, false);
}

std::vector<float> dilate(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    return morph_op(mask, w, h, radius, true);
}

std::vector<float> open(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    return dilate(erode(mask, w, h, radius), w, h, radius);
}

std::vector<float> close(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    return erode(dilate(mask, w, h, radius), w, h, radius);
}

std::vector<float> morph_gradient(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius)
{
    const auto d = dilate(mask, w, h, radius);
    const auto e = erode(mask, w, h, radius);

    std::vector<float> out(d.size());
    P::transform(P::par_unseq, d.begin(), d.end(), e.begin(), out.begin(),
        [](float dv, float ev) { return dv - ev; });

    return out;
}

} // namespace MayaFlux::Kinesis::Vision
