#include "Expanse.hpp"

#include "glm/vec3.hpp"

namespace MayaFlux::Nexus {

void Expanse::evaluate(uint32_t fabric_id,
    std::span<const std::pair<uint32_t, glm::vec3>> snapshot)
{
    begin_evaluate();

    auto& prev = m_occupants_by_fabric[fabric_id];

    std::unordered_set<uint32_t> inside;
    for (const auto& [eid, pos] : snapshot) {
        if (contains(pos))
            inside.insert(eid);
    }

    if (!m_perceptions.empty()) {
        const auto [center, radius] = reach();

        std::vector<Kinesis::QueryResult> occupants;
        occupants.reserve(inside.size());
        for (const auto& [eid, pos] : snapshot) {
            if (inside.contains(eid)) {
                const glm::vec3 offset = pos - center;
                occupants.push_back({ .id = eid, .distance_sq = glm::dot(offset, offset) });
            }
        }

        const PerceptionContext pctx { .position = center, .radius = radius, .spatial_results = occupants };
        for (const auto& [handle, perception] : m_perceptions)
            perception(pctx);
    }

    for (const auto& [eid, pos] : snapshot) {
        if (!inside.contains(eid))
            continue;

        const CrossingContext ctx { .fabric_id = fabric_id, .entity = eid, .position = pos };

        if (!prev.contains(eid)) {
            if (m_on_enter)
                m_on_enter(eid);
            for (const auto& [handle, action] : m_entries)
                action(ctx);
        }

        for (const auto& [handle, action] : m_insides)
            action(ctx);
    }

    std::unordered_map<uint32_t, glm::vec3> where;
    if (!m_exits.empty() && !prev.empty()) {
        where.reserve(snapshot.size());
        for (const auto& [eid, pos] : snapshot)
            where.emplace(eid, pos);
    }

    for (uint32_t eid : prev) {
        if (inside.contains(eid))
            continue;

        if (m_on_exit)
            m_on_exit(eid);

        if (m_exits.empty())
            continue;

        CrossingContext ctx { .fabric_id = fabric_id, .entity = eid };
        if (const auto it = where.find(eid); it != where.end())
            ctx.position = it->second;
        for (const auto& [handle, action] : m_exits)
            action(ctx);
    }

    if (inside.empty()) {
        m_occupants_by_fabric.erase(fabric_id);
    } else {
        prev = std::move(inside);
    }
}

} // namespace MayaFlux::Nexus
