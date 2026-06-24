#include "Contours.hpp"

namespace MayaFlux::Kinesis::Vision {

namespace {

    inline bool is_fg(float v) noexcept { return v >= 0.5F; }

    // 8-connected clockwise direction offsets starting from right (+x)
    // Order: E, SE, S, SW, W, NW, N, NE
    constexpr int32_t dx8[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    constexpr int32_t dy8[] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    // =========================================================================
    // Border following (Moore neighbourhood tracing)
    // =========================================================================

    std::vector<glm::vec2> trace_contour(
        const std::vector<uint8_t>& visited,
        std::span<const float> mask,
        uint32_t w, uint32_t h,
        uint32_t start_x, uint32_t start_y)
    {
        std::vector<glm::vec2> points;

        auto cx = static_cast<int32_t>(start_x);
        auto cy = static_cast<int32_t>(start_y);

        int32_t start_dir = 0;
        for (int32_t d = 0; d < 8; ++d) {
            const int32_t nx = cx + dx8[d];
            const int32_t ny = cy + dy8[d];
            if (nx < 0 || ny < 0
                || nx >= static_cast<int32_t>(w)
                || ny >= static_cast<int32_t>(h)
                || !is_fg(mask[static_cast<size_t>(ny) * w + nx])) {
                start_dir = d;
                break;
            }
        }

        const int32_t orig_x = cx;
        const int32_t orig_y = cy;
        int32_t dir = start_dir;
        bool first = true;

        while (true) {
            bool found = false;
            for (int32_t i = 0; i < 8; ++i) {
                const int32_t d = (dir + 6 + i) % 8;
                const int32_t nx = cx + dx8[d];
                const int32_t ny = cy + dy8[d];

                if (nx < 0 || ny < 0
                    || nx >= static_cast<int32_t>(w)
                    || ny >= static_cast<int32_t>(h))
                    continue;

                if (is_fg(mask[static_cast<size_t>(ny) * w + nx])) {
                    points.emplace_back(static_cast<float>(cx) / static_cast<float>(w),
                        static_cast<float>(cy) / static_cast<float>(h));
                    cx = nx;
                    cy = ny;
                    dir = d;
                    found = true;
                    break;
                }
            }

            if (!found)
                break;

            if (!first && cx == orig_x && cy == orig_y)
                break;

            first = false;

            if (points.size() > static_cast<size_t>(w) * h)
                break;
        }

        return points;
    }

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

} // namespace

std::vector<Contour> find_contours(
    std::span<const float> mask, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<uint8_t> visited(n, 0);
    std::vector<Contour> result;

    for (uint32_t py = 0; py < h; ++py) {
        for (uint32_t px = 0; px < w; ++px) {
            const size_t idx = static_cast<size_t>(py) * w + px;

            if (!is_fg(mask[idx]) || visited[idx])
                continue;

            bool is_border = false;
            if (px == 0 || py == 0
                || px == w - 1 || py == h - 1) {
                is_border = true;
            } else {
                if (!is_fg(mask[idx - 1])
                    || !is_fg(mask[idx + 1])
                    || !is_fg(mask[idx - w])
                    || !is_fg(mask[idx + w]))
                    is_border = true;
            }

            if (!is_border) {
                visited[idx] = 1;
                continue;
            }

            auto points = trace_contour(visited, mask, w, h, px, py);

            for (const auto& p : points) {
                const auto vx = static_cast<uint32_t>(p.x * static_cast<float>(w));
                const auto vy = static_cast<uint32_t>(p.y * static_cast<float>(h));
                visited[static_cast<size_t>(vy) * w + vx] = 1;
            }

            if (points.size() < 3)
                continue;

            const float area = polygon_area(points);
            const float perim = polygon_perimeter(points);

            result.push_back({ .points = std::move(points), .area = area, .perimeter = perim });
        }
    }

    return result;
}

} // namespace MayaFlux::Kinesis::Vision
