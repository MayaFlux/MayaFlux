#include "Expanse.hpp"

#include "glm/vec3.hpp"

namespace MayaFlux::Nexus {

void Expanse::evaluate(uint32_t fabric_id,
    std::span<const std::pair<uint32_t, glm::vec3>> snapshot)
{
    auto& prev = m_occupants_by_fabric[fabric_id];

    std::unordered_set<uint32_t> inside;
    for (const auto& [eid, pos] : snapshot) {
        if (m_contains && m_contains(pos))
            inside.insert(eid);
    }

    if (m_on_enter) {
        for (uint32_t eid : inside) {
            if (!prev.contains(eid))
                m_on_enter(eid);
        }
    }

    if (m_on_exit) {
        for (uint32_t eid : prev) {
            if (!inside.contains(eid))
                m_on_exit(eid);
        }
    }

    if (inside.empty()) {
        m_occupants_by_fabric.erase(fabric_id);
    } else {
        prev = std::move(inside);
    }
}

} // namespace MayaFlux::Nexus
