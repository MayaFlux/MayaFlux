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
    Registration reg { .member = std::move(m) };
    if (emitter->m_position.has_value()) {
        reg.spatial_id = m_index->insert(*emitter->m_position);
    }
    m_registrations.try_emplace(id, std::move(reg));
    return Wiring { *this, id };
}

Wiring Fabric::wire(std::shared_ptr<Sensor> sensor)
{
    Member m = sensor;
    uint32_t id = assign_id(m);
    Registration reg { .member = std::move(m) };
    if (sensor->m_position.has_value()) {
        reg.spatial_id = m_index->insert(*sensor->m_position);
    }
    m_registrations.try_emplace(id, std::move(reg));
    return Wiring { *this, id };
}

Wiring Fabric::wire(std::shared_ptr<Agent> agent)
{
    Member m = agent;
    uint32_t id = assign_id(m);
    Registration reg { .member = std::move(m) };
    if (agent->m_position.has_value()) {
        reg.spatial_id = m_index->insert(*agent->m_position);
    }
    m_registrations.try_emplace(id, std::move(reg));
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
    if (reg.spatial_id.has_value()) {
        m_index->remove(*reg.spatial_id);
    }

    m_registrations.erase(it);
}

std::vector<uint32_t> Fabric::all_ids() const
{
    std::vector<uint32_t> ids;
    ids.reserve(m_registrations.size());
    for (const auto& [id, reg] : m_registrations) {
        ids.push_back(id);
    }
    return ids;
}

Fabric::Kind Fabric::kind(uint32_t id) const
{
    const auto& reg = m_registrations.at(id);
    return std::visit([](const auto& ptr) -> Kind {
        using T = std::decay_t<decltype(*ptr)>;
        if constexpr (std::is_same_v<T, Emitter>) {
            return Kind::Emitter;
        } else if constexpr (std::is_same_v<T, Sensor>) {
            return Kind::Sensor;
        } else {
            return Kind::Agent;
        }
    },
        reg.member);
}

std::shared_ptr<Emitter> Fabric::get_emitter(uint32_t id) const
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end())
        return nullptr;
    if (auto* ptr = std::get_if<std::shared_ptr<Emitter>>(&it->second.member)) {
        return *ptr;
    }
    return nullptr;
}

std::shared_ptr<Sensor> Fabric::get_sensor(uint32_t id) const
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end())
        return nullptr;
    if (auto* ptr = std::get_if<std::shared_ptr<Sensor>>(&it->second.member)) {
        return *ptr;
    }
    return nullptr;
}

std::shared_ptr<Agent> Fabric::get_agent(uint32_t id) const
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end())
        return nullptr;
    if (auto* ptr = std::get_if<std::shared_ptr<Agent>>(&it->second.member)) {
        return *ptr;
    }
    return nullptr;
}

// =============================================================================
// Commit
// =============================================================================

void Fabric::commit()
{
    for (auto& [id, reg] : m_registrations) {
        if (reg.spatial_id.has_value()) {
            std::visit([&](const auto& ptr) {
                if (ptr->m_position.has_value()) {
                    m_index->update(*reg.spatial_id, *ptr->m_position);
                }
            },
                reg.member);
        }
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

const Wiring* Fabric::wiring_for(uint32_t id) const
{
    auto it = m_registrations.find(id);
    if (it == m_registrations.end() || !it->second.wiring.has_value()) {
        return nullptr;
    }
    return &*it->second.wiring;
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
            ctx.intensity = ptr->m_intensity;
            ctx.radius = ptr->m_radius;
            ctx.color = ptr->m_color;
            ctx.size = ptr->m_size;
            ctx.render_proc = ptr->m_influence_target;
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
                ictx.intensity = ptr->m_intensity;
                ictx.radius = ptr->m_radius;
                ictx.color = ptr->m_color;
                ictx.size = ptr->m_size;
                ictx.render_proc = ptr->m_influence_target;
                ptr->invoke_influence(ictx);
            } else {
                ptr->invoke_perception(PerceptionContext {});
                InfluenceContext ictx;
                ictx.intensity = ptr->m_intensity;
                ictx.radius = ptr->m_radius;
                ictx.color = ptr->m_color;
                ictx.size = ptr->m_size;
                ictx.render_proc = ptr->m_influence_target;
                ptr->invoke_influence(ictx);
            }
        }
    },
        reg.member);
}

} // namespace MayaFlux::Nexus
