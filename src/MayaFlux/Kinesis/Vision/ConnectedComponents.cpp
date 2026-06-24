#include "ConnectedComponents.hpp"

namespace MayaFlux::Kinesis::Vision {

namespace {

    inline bool is_fg(float v) noexcept { return v >= 0.5F; }

    // =========================================================================
    // Union-find with path compression and union by rank
    // =========================================================================

    struct UnionFind {
        std::vector<uint32_t> parent;
        std::vector<uint32_t> rank;

        explicit UnionFind(size_t n)
            : parent(n)
            , rank(n, 0)
        {
            std::iota(parent.begin(), parent.end(), 0U);
        }

        uint32_t find(uint32_t x)
        {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        }

        void unite(uint32_t a, uint32_t b)
        {
            a = find(a);
            b = find(b);
            if (a == b)
                return;
            if (rank[a] < rank[b])
                std::swap(a, b);
            parent[b] = a;
            if (rank[a] == rank[b])
                ++rank[a];
        }
    };

} // namespace

ComponentResult connected_components(
    std::span<const float> mask, uint32_t w, uint32_t h)
{
    const size_t n = static_cast<size_t>(w) * h;

    std::vector<uint32_t> labels(n, 0);
    UnionFind uf(n + 1);
    uint32_t next_label = 1;

    for (uint32_t py = 0; py < h; ++py) {
        for (uint32_t px = 0; px < w; ++px) {
            const size_t idx = static_cast<size_t>(py) * w + px;

            if (!is_fg(mask[idx]))
                continue;

            uint32_t left = 0;
            uint32_t above = 0;

            if (px > 0 && is_fg(mask[idx - 1]))
                left = labels[idx - 1];
            if (py > 0 && is_fg(mask[idx - w]))
                above = labels[idx - w];

            if (left == 0 && above == 0) {
                labels[idx] = next_label++;
            } else if (left != 0 && above == 0) {
                labels[idx] = left;
            } else if (left == 0 && above != 0) {
                labels[idx] = above;
            } else {
                labels[idx] = left;
                uf.unite(left, above);
            }
        }
    }

    std::vector<uint32_t> remap(next_label, 0);
    uint32_t compact = 0;
    for (uint32_t i = 1; i < next_label; ++i) {
        const uint32_t root = uf.find(i);
        if (remap[root] == 0)
            remap[root] = ++compact;
        remap[i] = remap[root];
    }

    struct Extent {
        uint32_t x_min, y_min, x_max, y_max;
        uint32_t pixel_count;
    };

    std::vector<Extent> extents(compact + 1,
        Extent { .x_min = w, .y_min = h, .x_max = 0, .y_max = 0, .pixel_count = 0 });

    for (uint32_t py = 0; py < h; ++py) {
        for (uint32_t px = 0; px < w; ++px) {
            const size_t idx = static_cast<size_t>(py) * w + px;
            if (labels[idx] == 0)
                continue;

            const uint32_t lbl = remap[labels[idx]];
            labels[idx] = lbl;

            auto& e = extents[lbl];
            e.x_min = std::min(e.x_min, px);
            e.y_min = std::min(e.y_min, py);
            e.x_max = std::max(e.x_max, px);
            e.y_max = std::max(e.y_max, py);
            ++e.pixel_count;
        }
    }

    const float inv_w = 1.0F / static_cast<float>(w);
    const float inv_h = 1.0F / static_cast<float>(h);

    std::vector<BoundingBox> boxes;
    boxes.reserve(compact);

    for (uint32_t i = 1; i <= compact; ++i) {
        const auto& e = extents[i];
        const float x = static_cast<float>(e.x_min) * inv_w;
        const float y = static_cast<float>(e.y_min) * inv_h;
        const float bw = static_cast<float>(e.x_max - e.x_min + 1) * inv_w;
        const float bh = static_cast<float>(e.y_max - e.y_min + 1) * inv_h;
        boxes.push_back({ .x = x, .y = y, .w = bw, .h = bh, .confidence = 1.0F, .label_id = i });
    }

    return { .label_map = std::move(labels), .boxes = std::move(boxes), .count = compact };
}

} // namespace MayaFlux::Kinesis::Vision
