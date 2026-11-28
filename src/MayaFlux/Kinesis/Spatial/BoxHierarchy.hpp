#pragma once

#include "Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @class BoxHierarchy
 * @brief Nested axis-aligned boxes over a set of items, for finding what a ray
 *        meets first without testing every item.
 *
 * Built once from one box per item and not updated. Build another when the
 * items move.
 */
class MAYAFLUX_API BoxHierarchy {
public:
    explicit BoxHierarchy(std::span<const AABB3D> items);

    /** @brief Number of items it was built from. */
    [[nodiscard]] size_t size() const noexcept { return m_order.size(); }

    /**
     * @brief The item a ray meets first.
     *
     * @param ray  Ray of any nonzero direction.
     * @param meet Parameter at which the ray meets an item, or std::nullopt.
     *             Asked only for items whose box the ray reaches before the
     *             best parameter found so far.
     * @return The item and its parameter. Among equal parameters the lower
     *         item number wins.
     */
    template <typename Meet>
    [[nodiscard]] std::optional<std::pair<uint32_t, float>> nearest(const Ray& ray, Meet&& meet) const
    {
        std::optional<std::pair<uint32_t, float>> best;
        if (m_nodes.empty()) {
            return best;
        }

        std::vector<uint32_t> pending { 0U };
        while (!pending.empty()) {
            const Node& node = m_nodes[pending.back()];
            pending.pop_back();

            const auto entry = node.box.ray_entry(ray);
            if (!entry || (best && *entry > best->second)) {
                continue;
            }

            if (node.count == 0) {
                pending.push_back(node.first);
                pending.push_back(node.first + 1);
                continue;
            }

            for (uint32_t i = node.first; i < node.first + node.count; ++i) {
                const uint32_t item = m_order[i];
                const std::optional<float> t = meet(item);
                if (t && (!best || *t < best->second || (*t == best->second && item < best->first))) {
                    best.emplace(item, *t);
                }
            }
        }
        return best;
    }

private:
    /** A leaf holds count items from first; a branch has count 0 and children first, first + 1. */
    struct Node {
        AABB3D box;
        uint32_t first;
        uint32_t count;
    };

    std::vector<Node> m_nodes;
    std::vector<uint32_t> m_order;
};

/**
 * @brief One box per triangle of an indexed set, for a BoxHierarchy.
 *
 * Boxes are widened by a hair so a flat triangle is not missed on its edge. A
 * triangle naming a vertex out of range gets a box nothing reaches.
 */
template <std::ranges::random_access_range R, typename Proj = std::identity>
[[nodiscard]] std::vector<AABB3D> triangle_boxes(
    const R& vertices, std::span<const uint32_t> indices, Proj position = {})
{
    const auto count = static_cast<size_t>(std::ranges::size(vertices));
    const auto first = std::ranges::begin(vertices);

    std::vector<AABB3D> boxes;
    boxes.reserve(indices.size() / 3);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t a = indices[i];
        const uint32_t b = indices[i + 1];
        const uint32_t c = indices[i + 2];
        if (a >= count || b >= count || c >= count) {
            constexpr float away = std::numeric_limits<float>::max();
            boxes.push_back({ .min = glm::vec3(away), .max = glm::vec3(away) });
            continue;
        }

        const glm::vec3 pa = std::invoke(position, first[a]);
        const glm::vec3 pb = std::invoke(position, first[b]);
        const glm::vec3 pc = std::invoke(position, first[c]);
        const glm::vec3 low = glm::min(pa, glm::min(pb, pc));
        const glm::vec3 high = glm::max(pa, glm::max(pb, pc));
        const glm::vec3 hair = glm::max(glm::abs(low), glm::abs(high)) * 1.0e-6F + 1.0e-7F;
        boxes.push_back({ .min = low - hair, .max = high + hair });
    }
    return boxes;
}

/**
 * @brief ray_triangles_parameter through a hierarchy built from triangle_boxes
 *        of the same indices.
 *
 * Gives what the linear form gives, provided the vertices have not moved since
 * the hierarchy was built.
 */
template <std::ranges::random_access_range R, typename Proj = std::identity>
[[nodiscard]] std::optional<std::pair<uint32_t, glm::vec3>> ray_triangles_parameter(
    const Ray& ray, const BoxHierarchy& hierarchy,
    const R& vertices, std::span<const uint32_t> indices, Proj position = {})
{
    const auto count = static_cast<size_t>(std::ranges::size(vertices));
    const auto first = std::ranges::begin(vertices);

    const auto meet = [&](uint32_t triangle) -> std::optional<glm::vec3> {
        const size_t at = static_cast<size_t>(triangle) * 3;
        if (at + 2 >= indices.size()) {
            return std::nullopt;
        }
        const uint32_t a = indices[at];
        const uint32_t b = indices[at + 1];
        const uint32_t c = indices[at + 2];
        if (a >= count || b >= count || c >= count) {
            return std::nullopt;
        }
        return ray_triangle_parameter(ray,
            std::invoke(position, first[a]),
            std::invoke(position, first[b]),
            std::invoke(position, first[c]));
    };

    const auto found = hierarchy.nearest(ray, [&meet](uint32_t triangle) -> std::optional<float> {
        const auto hit = meet(triangle);
        return hit ? std::optional<float>(hit->x) : std::nullopt;
    });
    if (!found) {
        return std::nullopt;
    }
    return std::pair { found->first, *meet(found->first) };
}

} // namespace MayaFlux::Kinesis
