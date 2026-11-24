#include "Identification.hpp"

namespace MayaFlux::Kinesis {

namespace {

    struct Cell {
        int64_t x;
        int64_t y;
        int64_t z;
    };

    struct Entry {
        Cell cell;
        uint32_t index;
    };

    bool cell_less(const Cell& a, const Cell& b) noexcept
    {
        return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    }

    bool cell_equal(const Cell& a, const Cell& b) noexcept
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    bool is_finite(const glm::vec3& p) noexcept
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }

    Cell cell_of(const glm::vec3& p, double inverse) noexcept
    {
        const auto axis = [inverse](float value) {
            return static_cast<int64_t>(std::floor(std::clamp(static_cast<double>(value) * inverse, -4.0e18, 4.0e18)));
        };
        return { .x = axis(p.x), .y = axis(p.y), .z = axis(p.z) };
    }

    std::array<Cell, 13> forward_offsets() noexcept
    {
        std::array<Cell, 13> offsets {};
        size_t next = 0;
        for (int64_t dx = -1; dx <= 1; ++dx) {
            for (int64_t dy = -1; dy <= 1; ++dy) {
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    const bool ahead = dx > 0 || (dx == 0 && (dy > 0 || (dy == 0 && dz > 0)));
                    if (ahead) {
                        offsets[next++] = { .x = dx, .y = dy, .z = dz };
                    }
                }
            }
        }
        return offsets;
    }

    struct Roots {
        std::vector<uint32_t> parent;

        explicit Roots(size_t count)
            : parent(count)
        {
            std::ranges::iota(parent, 0U);
        }

        uint32_t find(uint32_t node) noexcept
        {
            while (parent[node] != node) {
                parent[node] = parent[parent[node]];
                node = parent[node];
            }
            return node;
        }

        void unite(uint32_t a, uint32_t b) noexcept
        {
            a = find(a);
            b = find(b);
            if (a < b) {
                parent[b] = a;
            } else if (b < a) {
                parent[a] = b;
            }
        }
    };

    void merge_equal(std::span<const glm::vec3> positions, Roots& roots)
    {
        std::vector<uint32_t> order;
        order.reserve(positions.size());
        for (uint32_t i = 0; i < positions.size(); ++i) {
            if (is_finite(positions[i])) {
                order.push_back(i);
            }
        }

        std::sort(std::execution::par, order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            const glm::vec3& p = positions[a];
            const glm::vec3& q = positions[b];
            return std::tie(p.x, p.y, p.z) < std::tie(q.x, q.y, q.z);
        });

        for (size_t k = 1; k < order.size(); ++k) {
            if (positions[order[k]] == positions[order[k - 1]]) {
                roots.unite(order[k], order[k - 1]);
            }
        }
    }

    void merge_near(std::span<const glm::vec3> positions, double tolerance, Roots& roots)
    {
        const double inverse = 1.0 / tolerance;
        const double reach = tolerance * tolerance;

        std::vector<Entry> entries;
        entries.reserve(positions.size());
        for (uint32_t i = 0; i < positions.size(); ++i) {
            if (is_finite(positions[i])) {
                entries.push_back({ .cell = cell_of(positions[i], inverse), .index = i });
            }
        }

        std::sort(std::execution::par, entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (cell_less(a.cell, b.cell)) {
                return true;
            }
            return cell_equal(a.cell, b.cell) && a.index < b.index;
        });

        const auto ahead = forward_offsets();
        const auto first_in = [&entries](const Cell& target) {
            return std::lower_bound(entries.begin(), entries.end(), target,
                [](const Entry& e, const Cell& c) { return cell_less(e.cell, c); });
        };

        for (size_t s = 0; s < entries.size(); ++s) {
            const Entry& here = entries[s];
            const glm::dvec3 p(positions[here.index]);
            const auto test = [&](const Entry& other) {
                const glm::dvec3 d = glm::dvec3(positions[other.index]) - p;
                if (glm::dot(d, d) <= reach) {
                    roots.unite(here.index, other.index);
                }
            };

            for (size_t t = s + 1; t < entries.size() && cell_equal(entries[t].cell, here.cell); ++t) {
                test(entries[t]);
            }

            for (const Cell& offset : ahead) {
                const Cell target {
                    .x = here.cell.x + offset.x,
                    .y = here.cell.y + offset.y,
                    .z = here.cell.z + offset.z
                };
                for (auto it = first_in(target); it != entries.end() && cell_equal(it->cell, target); ++it) {
                    test(*it);
                }
            }
        }
    }

} // namespace

std::optional<Welding> weld_positions(std::span<const glm::vec3> positions, float tolerance)
{
    if (!(tolerance >= 0.0F) || !std::isfinite(tolerance)
        || positions.size() > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }

    Roots roots(positions.size());
    if (tolerance == 0.0F) {
        merge_equal(positions, roots);
    } else {
        merge_near(positions, static_cast<double>(tolerance), roots);
    }

    Welding result;
    result.vertex_map.resize(positions.size());
    for (uint32_t i = 0; i < positions.size(); ++i) {
        const uint32_t root = roots.find(i);
        if (root == i) {
            result.vertex_map[i] = static_cast<uint32_t>(result.source_vertex.size());
            result.source_vertex.push_back(i);
        } else {
            result.vertex_map[i] = result.vertex_map[root];
        }
    }
    return result;
}

} // namespace MayaFlux::Kinesis
