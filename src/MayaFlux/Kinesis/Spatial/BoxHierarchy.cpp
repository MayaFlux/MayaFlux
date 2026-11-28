#include "BoxHierarchy.hpp"

namespace MayaFlux::Kinesis {

namespace {

    constexpr uint32_t k_leaf_items = 4;

    AABB3D around(std::span<const AABB3D> items, std::span<const uint32_t> chosen)
    {
        AABB3D box = items[chosen.front()];
        for (const uint32_t item : chosen.subspan(1)) {
            box.min = glm::min(box.min, items[item].min);
            box.max = glm::max(box.max, items[item].max);
        }
        return box;
    }

}

BoxHierarchy::BoxHierarchy(std::span<const AABB3D> items)
    : m_order(items.size())
{
    std::iota(m_order.begin(), m_order.end(), 0U);
    if (items.empty()) {
        return;
    }

    struct Pending {
        uint32_t node;
        uint32_t first;
        uint32_t count;
    };

    m_nodes.push_back({});
    std::vector<Pending> pending { { .node = 0, .first = 0, .count = static_cast<uint32_t>(items.size()) } };

    while (!pending.empty()) {
        const Pending work = pending.back();
        pending.pop_back();

        const std::span<uint32_t> chosen(m_order.data() + work.first, work.count);
        const AABB3D box = around(items, chosen);

        if (work.count <= k_leaf_items) {
            m_nodes[work.node] = { .box = box, .first = work.first, .count = work.count };
            continue;
        }

        const glm::vec3 size = box.max - box.min;
        const int axis = size.x >= size.y && size.x >= size.z ? 0 : (size.y >= size.z ? 1 : 2);
        const uint32_t half = work.count / 2;
        std::ranges::nth_element(chosen, chosen.begin() + half, {}, [&items, axis](uint32_t item) {
            return items[item].min[axis] * 0.5F + items[item].max[axis] * 0.5F;
        });

        const auto children = static_cast<uint32_t>(m_nodes.size());
        m_nodes.push_back({});
        m_nodes.push_back({});
        m_nodes[work.node] = { .box = box, .first = children, .count = 0 };

        pending.push_back({ .node = children, .first = work.first, .count = half });
        pending.push_back({ .node = children + 1, .first = work.first + half, .count = work.count - half });
    }
}

} // namespace MayaFlux::Kinesis
