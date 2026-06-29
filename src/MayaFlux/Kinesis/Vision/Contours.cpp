#include "Contours.hpp"

#include "ConnectedComponents.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Vision {

namespace {

    // 8-connected clockwise direction offsets starting from right (+x)
    // Order: E, SE, S, SW, W, NW, N, NE
    constexpr int32_t dx8[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    constexpr int32_t dy8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    // =========================================================================
    // Shoelace area and perimeter
    // =========================================================================

    float polygon_area(const std::vector<glm::vec2>& pts)
    {
        float area = 0.0F;
        const size_t n = pts.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            area += pts[j].x * pts[i].y;
            area -= pts[i].x * pts[j].y;
        }
        return std::abs(area) * 0.5F;
    }

    float polygon_perimeter(const std::vector<glm::vec2>& pts)
    {
        float perim = 0.0F;
        const size_t n = pts.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++)
            perim += glm::length(pts[i] - pts[j]);
        return perim;
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
    float min_area, uint32_t max_contours)
{
    const auto cc = connected_components(mask, w, h);
    if (cc.count == 0)
        return {};

    std::vector<Contour> result(cc.count);
    std::vector<uint8_t> valid(cc.count, 0);

    P::for_each(P::par_unseq,
        std::views::iota(uint32_t { 0 }, cc.count).begin(),
        std::views::iota(uint32_t { 0 }, cc.count).end(),
        [&](uint32_t ci) {
            const uint32_t lbl = ci + 1;
            const auto& box = cc.boxes[ci];

            const auto x0 = static_cast<uint32_t>(box.x * static_cast<float>(w));
            const auto y0 = static_cast<uint32_t>(box.y * static_cast<float>(h));
            const auto x1 = std::min(x0 + static_cast<uint32_t>(box.w * static_cast<float>(w)) - 1U, w - 1);
            const auto y1 = std::min(y0 + static_cast<uint32_t>(box.h * static_cast<float>(h)) - 1U, h - 1);

            uint32_t start_x = 0, start_y = 0;
            bool found = false;
            for (uint32_t py = y0; py <= y1 && !found; ++py) {
                for (uint32_t px = x0; px <= x1 && !found; ++px) {
                    if (cc.label_map[static_cast<size_t>(py) * w + px] == lbl) {
                        start_x = px;
                        start_y = py;
                        found = true;
                    }
                }
            }

            if (!found)
                return;

            const uint32_t bw = x1 - x0 + 1;
            const uint32_t bh = y1 - y0 + 1;

            if (bw < 2 || bh < 2)
                return;

            std::vector<uint8_t> visited(static_cast<size_t>(bw) * bh, 0);

            auto is_component_fg = [&](int32_t px, int32_t py) -> bool {
                if (px < 0 || py < 0
                    || static_cast<uint32_t>(px) >= w
                    || static_cast<uint32_t>(py) >= h)
                    return false;
                return cc.label_map[static_cast<size_t>(py) * w + px] == lbl;
            };

            auto mark_visited = [&](int32_t px, int32_t py) {
                if (static_cast<uint32_t>(px) < x0 || static_cast<uint32_t>(py) < y0
                    || static_cast<uint32_t>(px) > x1 || static_cast<uint32_t>(py) > y1)
                    return;
                const uint32_t lx = static_cast<uint32_t>(px) - x0;
                const uint32_t ly = static_cast<uint32_t>(py) - y0;
                visited[static_cast<size_t>(ly) * bw + lx] = 1;
            };

            auto cx = static_cast<int32_t>(start_x);
            auto cy = static_cast<int32_t>(start_y);

            int32_t start_dir = 0;
            for (int32_t d = 0; d < 8; ++d) {
                const int32_t nx = cx + dx8[d];
                const int32_t ny = cy + dy8[d];
                if (!is_component_fg(nx, ny)) {
                    start_dir = d;
                    break;
                }
            }

            const int32_t orig_x = cx;
            const int32_t orig_y = cy;
            int32_t dir = start_dir;
            bool first = true;
            std::vector<glm::vec2> points;

            while (true) {
                bool step_found = false;
                for (int32_t i = 0; i < 8; ++i) {
                    const int32_t d = (dir + 6 + i) % 8;
                    const int32_t nx = cx + dx8[d];
                    const int32_t ny = cy + dy8[d];

                    if (!is_component_fg(nx, ny))
                        continue;

                    points.emplace_back(
                        static_cast<float>(cx) / static_cast<float>(w),
                        static_cast<float>(cy) / static_cast<float>(h));
                    mark_visited(cx, cy);
                    cx = nx;
                    cy = ny;
                    dir = d;
                    step_found = true;
                    break;
                }

                if (!step_found)
                    break;
                if (!first && cx == orig_x && cy == orig_y)
                    break;
                first = false;
                if (points.size() > static_cast<size_t>(w) * h)
                    break;
            }

            if (points.size() < 3)
                return;

            const float area = polygon_area(points);
            if (area < min_area)
                return;

            result[ci] = { .points = std::move(points), .area = area, .perimeter = polygon_perimeter(points) };
            valid[ci] = 1;
        });

    std::vector<Contour> out;
    out.reserve(cc.count);
    for (uint32_t i = 0; i < cc.count; ++i) {
        if (valid[i])
            out.push_back(std::move(result[i]));
    }

    if (max_contours > 0 && out.size() > static_cast<size_t>(max_contours)) {
        std::partial_sort(out.begin(),
            out.begin() + static_cast<ptrdiff_t>(max_contours),
            out.end(),
            [](const Contour& a, const Contour& b) { return a.area > b.area; });
        out.resize(max_contours);
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
