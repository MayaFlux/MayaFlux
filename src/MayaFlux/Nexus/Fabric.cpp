#include "Fabric.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Spatial/SpatialIndex.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nexus {

Fabric::Fabric(
    Vruta::TaskScheduler& scheduler,
    Vruta::EventManager& event_manager,
    float cell_size)
    : m_scheduler(scheduler)
    , m_event_manager(event_manager)
    , m_index(Kinesis::make_spatial_index_3d(cell_size))
{
}

Fabric::~Fabric()
{
    for (auto& [id, reg] : m_registrations) {
        if (!reg.task_name.empty()) {
            m_scheduler.cancel_task(reg.task_name);
        }
        if (!reg.chain_name.empty()) {
            m_scheduler.cancel_task(reg.chain_name);
        }
        if (!reg.event_name.empty()) {
            m_event_manager.cancel_event(reg.event_name);
        }
    }
}

// =============================================================================
// Registration
// =============================================================================

Wiring Fabric::wire(std::shared_ptr<Emitter> emitter)
{
    Member m = emitter;
    uint32_t id = assign_id(m);
    if (emitter->m_position.has_value()) {
        m_index->insert(*emitter->m_position);
    }
    m_registrations[id] = Registration { .member = std::move(m) };
    return Wiring { *this, id };
}

Wiring Fabric::wire(std::shared_ptr<Sensor> sensor)
{
    Member m = sensor;
    uint32_t id = assign_id(m);
    if (sensor->m_position.has_value()) {
        m_index->insert(*sensor->m_position);
    }
    m_registrations[id] = Registration { .member = std::move(m) };
    return Wiring { *this, id };
}

Wiring Fabric::wire(std::shared_ptr<Agent> agent)
{
    Member m = agent;
    uint32_t id = assign_id(m);
    if (agent->m_position.has_value()) {
        m_index->insert(*agent->m_position);
    }
    m_registrations[id] = Registration { .member = std::move(m) };
    return Wiring { *this, id };
}

void Fabric::remove(uint32_t id)
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end()) {
        return;
    }

    auto& reg = it->second;

    if (!reg.task_name.empty()) {
        m_scheduler.cancel_task(reg.task_name);
    }
    if (!reg.chain_name.empty()) {
        m_scheduler.cancel_task(reg.chain_name);
    }
    if (!reg.event_name.empty()) {
        m_event_manager.cancel_event(reg.event_name);
    }

    std::visit([&](const auto& ptr) {
        if (ptr->m_position.has_value()) {
            m_index->remove(id);
        }
    },
        reg.member);

    m_registrations.erase(it);
}

// =============================================================================
// Commit
// =============================================================================

void Fabric::commit()
{
    for (auto& [id, reg] : m_registrations) {
        std::visit([&](const auto& ptr) {
            if (ptr->m_position.has_value()) {
                m_index->update(id, *ptr->m_position);
            }
        },
            reg.member);
    }

    m_index->publish();

    for (auto& [id, reg] : m_registrations) {
        if (reg.commit_driven) {
            fire(reg);
        }
    }
}

void Fabric::fire(uint32_t id)
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Fabric::fire: id {} not registered", id);
        return;
    }
    fire(it->second);
}

// =============================================================================
// Spatial queries
// =============================================================================

std::vector<Kinesis::QueryResult> Fabric::within_radius(
    const glm::vec3& center, float radius) const
{
    return m_index->within_radius(center, radius);
}

std::vector<Kinesis::QueryResult> Fabric::k_nearest(
    const glm::vec3& center, uint32_t k) const
{
    return m_index->k_nearest(center, k);
}

// =============================================================================
// Private
// =============================================================================

uint32_t Fabric::assign_id(Member& m)
{
    uint32_t id = m_next_id++;
    std::visit([id](const auto& ptr) {
        ptr->m_id = id;
    },
        m);
    return id;
}

void Fabric::fire(const Registration& reg) const
{
    std::visit([&](const auto& ptr) {
        using T = std::decay_t<decltype(*ptr)>;

        if constexpr (std::is_same_v<T, Emitter>) {
            InfluenceContext ctx;
            if (ptr->m_position.has_value()) {
                ctx.position = *ptr->m_position;
            }
            ptr->invoke(ctx);

        } else if constexpr (std::is_same_v<T, Sensor>) {
            PerceptionContext ctx;
            if (ptr->m_position.has_value()) {
                ctx.position = *ptr->m_position;
                auto results = m_index->within_radius(*ptr->m_position, ptr->m_query_radius);
                ctx.spatial_results = std::span(results);
            }
            ptr->invoke(ctx);

        } else if constexpr (std::is_same_v<T, Agent>) {
            if (ptr->m_position.has_value()) {
                auto results = m_index->within_radius(*ptr->m_position, ptr->m_query_radius);
                PerceptionContext pctx;
                pctx.position = *ptr->m_position;
                pctx.spatial_results = std::span(results);
                ptr->invoke_perception(pctx);

                InfluenceContext ictx;
                ictx.position = *ptr->m_position;
                ptr->invoke_influence(ictx);
            } else {
                ptr->invoke_perception(PerceptionContext {});
                ptr->invoke_influence(InfluenceContext {});
            }
        }
    },
        reg.member);
}

} // namespace MayaFlux::Nexus
