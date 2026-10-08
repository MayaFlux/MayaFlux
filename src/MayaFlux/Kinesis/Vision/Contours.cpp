#include "Contours.hpp"

#include "ConnectedComponents.hpp"

#include <execution>

namespace MayaFlux::Kinesis::Vision {

namespace {

    constexpr int32_t dx8[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    constexpr int32_t dy8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    struct TraceSeed {
        size_t pixel;
        uint32_t label;
        uint32_t parent;
    };

    template <typename Inside>
    std::vector<glm::ivec2> walk_boundary(
        glm::ivec2 start, size_t max_points, Inside inside)
    {
        std::vector<glm::ivec2> points;
        glm::ivec2 current = start;
        glm::ivec2 first_next {};
        int32_t backtrack = 4;

        while (points.size() < max_points) {
            int32_t direction = -1;
            glm::ivec2 next {};
            for (int32_t i = 1; i <= 8; ++i) {
                const int32_t d = (backtrack + i) % 8;
                next = current + glm::ivec2(dx8[d], dy8[d]);
                if (inside(next)) {
                    direction = d;
                    break;
                }
            }

            if (direction < 0)
                break;
            if (!points.empty() && current == start && next == first_next)
                break;
            if (points.empty())
                first_next = next;

            points.push_back(current);
            current = next;
            backtrack = (direction + 6 - direction % 2) % 8;
        }

        return points;
    }

    Contour measure_contour(
        const std::vector<glm::ivec2>& pixels, uint32_t w, uint32_t h,
        uint32_t parent)
    {
        float area = 0.0F;
        float perimeter = 0.0F;
        for (size_t i = 0; i < pixels.size(); ++i) {
            const glm::vec2 a(pixels[i]);
            const glm::vec2 b(pixels[(i + 1) % pixels.size()]);
            area += a.x * b.y - b.x * a.y;
            perimeter += glm::length(b - a);
        }

        Contour contour {
            .points = {},
            .area = (std::abs(area) * 0.5F + perimeter * 0.5F + 1.0F)
                / (static_cast<float>(w) * static_cast<float>(h)),
            .perimeter = perimeter,
            .parent_label = parent == 0 ? Contour::no_parent : parent,
        };
        contour.points.reserve(pixels.size());
        const glm::vec2 extent(static_cast<float>(w), static_cast<float>(h));
        for (const auto& p : pixels)
            contour.points.push_back(glm::vec2(p) / extent);
        return contour;
    }

    bool point_in_contour(const std::vector<glm::vec2>& pts, float px, float py) noexcept
    {
        int winding = 0;
        const size_t n = pts.size();
        for (size_t i = 0; i < n; ++i) {
            const glm::vec2 a = pts[i];
            const glm::vec2 b = pts[(i + 1) % n];
            if (a.y <= py) {
                if (b.y > py) {
                    const float cross = (b.x - a.x) * (py - a.y)
                        - (b.y - a.y) * (px - a.x);
                    if (cross > 0.0F)
                        ++winding;
                }
            } else {
                if (b.y <= py) {
                    const float cross = (b.x - a.x) * (py - a.y)
                        - (b.y - a.y) * (px - a.x);
                    if (cross < 0.0F)
                        --winding;
                }
            }
        }
        return winding != 0;
    }

} // namespace

std::vector<Contour> find_contours(
    std::span<const float> mask, uint32_t w, uint32_t h,
    float min_area, uint32_t max_contours, uint32_t max_points_per_contour)
{
    if (w == 0 || h == 0)
        return {};

    const auto cc = connected_components(mask, w, h);
    return find_contours(cc, w, h, min_area, max_contours, max_points_per_contour);
}

std::vector<Contour> find_contours(
    const ComponentResult& cc, uint32_t w, uint32_t h,
    float min_area, uint32_t max_contours, uint32_t max_points_per_contour)
{
    if (w == 0 || h == 0 || cc.count == 0)
        return {};

    const size_t n = static_cast<size_t>(w) * h;
    std::vector<TraceSeed> seeds(cc.count, TraceSeed { .pixel = n, .label = 0, .parent = 0 });
    for (size_t i = 0; i < n; ++i) {
        const uint32_t label = cc.label_map[i];
        if (label != 0 && seeds[label - 1].pixel == n)
            seeds[label - 1] = { .pixel = i, .label = label, .parent = 0 };
    }

    std::vector<uint32_t> background(n, 0);
    std::vector<size_t> pending;
    uint32_t region = 0;
    for (size_t seed = 0; seed < n; ++seed) {
        if (cc.label_map[seed] != 0 || background[seed] != 0)
            continue;

        ++region;
        pending.clear();
        pending.push_back(seed);
        background[seed] = region;
        bool exterior = false;
        for (size_t head = 0; head < pending.size(); ++head) {
            const size_t pixel = pending[head];
            const auto x = static_cast<uint32_t>(pixel % w);
            const auto y = static_cast<uint32_t>(pixel / w);
            exterior = exterior || x == 0 || y == 0 || x + 1 == w || y + 1 == h;

            const auto visit = [&](size_t neighbor) {
                if (cc.label_map[neighbor] == 0 && background[neighbor] == 0) {
                    background[neighbor] = region;
                    pending.push_back(neighbor);
                }
            };
            if (x > 0)
                visit(pixel - 1);
            if (x + 1 < w)
                visit(pixel + 1);
            if (y > 0)
                visit(pixel - w);
            if (y + 1 < h)
                visit(pixel + w);
        }

        if (!exterior) {
            const uint32_t parent = cc.label_map[seed - w];
            seeds.push_back({ .pixel = seed, .label = region, .parent = parent });
        }
    }

    std::vector<Contour> result(seeds.size());
    std::vector<uint8_t> valid(seeds.size(), 0);
    const size_t max_points = max_points_per_contour > 0
        ? static_cast<size_t>(max_points_per_contour)
        : n * 8;

    std::for_each(std::execution::par_unseq,
        std::views::iota(size_t { 0 }, seeds.size()).begin(),
        std::views::iota(size_t { 0 }, seeds.size()).end(),
        [&](size_t ci) {
            const auto& seed = seeds[ci];
            const glm::ivec2 start(static_cast<int32_t>(seed.pixel % w), static_cast<int32_t>(seed.pixel / w));
            const auto inside = [&](glm::ivec2 p) {
                if (p.x < 0 || p.y < 0 || static_cast<uint32_t>(p.x) >= w || static_cast<uint32_t>(p.y) >= h)
                    return false;
                const size_t index = static_cast<size_t>(p.y) * w + static_cast<uint32_t>(p.x);
                return seed.parent == 0 ? cc.label_map[index] == seed.label : background[index] == seed.label;
            };
            const auto points = walk_boundary(start, max_points, inside);
            if (points.size() < 3)
                return;

            auto contour = measure_contour(points, w, h, seed.parent);
            if (contour.area < min_area)
                return;

            result[ci] = std::move(contour);
            valid[ci] = 1;
        });

    std::vector<Contour> out;
    out.reserve(seeds.size());
    for (size_t i = 0; i < seeds.size(); ++i) {
        if (valid[i])
            out.push_back(std::move(result[i]));
    }

    if (max_contours > 0) {
        const size_t count = std::min(out.size(), static_cast<size_t>(max_contours));
        std::partial_sort(out.begin(),
            out.begin() + static_cast<ptrdiff_t>(count),
            out.end(),
            [](const Contour& a, const Contour& b) { return a.area > b.area; });
        out.resize(count);
    }

    return out;
}

void apply_contour_mask(
    std::span<float> pixels,
    uint32_t w, uint32_t h,
    uint32_t channels,
    const Contour& contour,
    float origin_x, float origin_y,
    float scale_x, float scale_y)
{
    if (pixels.empty() || contour.points.empty() || channels == 0)
        return;

    for (uint32_t row = 0; row < h; ++row) {
        const float py = origin_y + (static_cast<float>(row) + 0.5F) * scale_y;
        for (uint32_t col = 0; col < w; ++col) {
            const float px = origin_x + (static_cast<float>(col) + 0.5F) * scale_x;
            if (!point_in_contour(contour.points, px, py)) {
                const size_t base = (static_cast<size_t>(row) * w + col) * channels;
                for (uint32_t c = 0; c < channels; ++c)
                    pixels[base + c] = 0.0F;
            }
        }
    }
}

} // namespace MayaFlux::Kinesis::Vision
