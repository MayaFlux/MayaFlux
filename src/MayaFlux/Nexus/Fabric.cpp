#include "Fabric.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Spatial/SpatialIndex.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nexus {

Fabric::Fabric(
    Vruta::TaskScheduler& scheduler,
    Vruta::EventManager& event_manager,
    Buffers::BufferManager& buffer_manager,
    float cell_size)
    : m_scheduler(scheduler)
    , m_event_manager(event_manager)
    , m_buffers(buffer_manager)
    , m_index(Kinesis::make_spatial_index_3d(cell_size))
    , m_cell_size(cell_size)
{
}

Fabric::~Fabric()
{
    for (auto& [id, expanse] : m_expanses) {
        expanse->evict(m_fabric_id);
    }

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
        release(reg);
    }
}

void Fabric::release(Registration& reg)
{
    std::visit([this](const auto& ptr) {
        using T = std::decay_t<decltype(*ptr)>;
        if constexpr (std::is_same_v<T, Emitter> || std::is_same_v<T, Agent>) {
            ptr->detach();
            ptr->release_sinks(m_buffers);
        }
    },
        reg.member);
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
        track(id, reg, *emitter->m_position);
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
        track(id, reg, *sensor->m_position);
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
        track(id, reg, *agent->m_position);
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
    untrack(reg);
    release(reg);

    m_registrations.erase(it);
}

void Fabric::cancel_wiring(uint32_t id)
{
    const auto it = m_registrations.find(id);
    if (it != m_registrations.end() && it->second.wiring.has_value()) {
        it->second.wiring->cancel();
    }
}

uint32_t Fabric::add_expanse(std::shared_ptr<Expanse> expanse)
{
    const uint32_t id = expanse->id();

    if (!expanse->fn_name().empty() && expanse->contains_fn()) {
        m_expanse_fns.try_emplace(
            expanse->fn_name(), std::make_shared<Expanse::ContainsFn>(expanse->contains_fn()));
    }
    if (!expanse->on_enter_fn_name().empty() && expanse->on_enter_fn()) {
        m_crossing_fns.try_emplace(
            expanse->on_enter_fn_name(), std::make_shared<Expanse::CrossingFn>(expanse->on_enter_fn()));
    }
    if (!expanse->on_exit_fn_name().empty() && expanse->on_exit_fn()) {
        m_crossing_fns.try_emplace(
            expanse->on_exit_fn_name(), std::make_shared<Expanse::CrossingFn>(expanse->on_exit_fn()));
    }

    m_expanses.try_emplace(id, std::move(expanse));
    return id;
}

namespace {

    template <typename Map, typename Fn>
    void register_named(Map& registry, std::string name, Fn fn, std::string_view kind)
    {
        if (name.empty()) {
            return;
        }
        if (registry.contains(name)) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
                "Fabric: {} function '{}' already registered, replacing", kind, name);
        }
        registry.insert_or_assign(std::move(name), std::make_shared<Fn>(std::move(fn)));
    }

}

void Fabric::register_influence(std::string name, Emitter::InfluenceFn fn)
{
    register_named(m_influence_fns, std::move(name), std::move(fn), "influence");
}

void Fabric::register_perception(std::string name, Sensor::PerceptionFn fn)
{
    register_named(m_perception_fns, std::move(name), std::move(fn), "perception");
}

void Fabric::register_contains(std::string name, Expanse::ContainsFn fn)
{
    register_named(m_expanse_fns, std::move(name), std::move(fn), "containment");
}

void Fabric::register_crossing(std::string name, Expanse::CrossingFn fn)
{
    register_named(m_crossing_fns, std::move(name), std::move(fn), "crossing");
}

void Fabric::register_radiate(std::string name, Presence::RadiateFn fn)
{
    register_named(m_radiate_fns, std::move(name), std::move(fn), "radiate");
}

void Fabric::remove_expanse(uint32_t id)
{
    const auto it = m_expanses.find(id);
    if (it == m_expanses.end()) {
        return;
    }

    it->second->evict(m_fabric_id);
    m_expanses.erase(it);
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
        std::visit([&](const auto& ptr) {
            if (!ptr->m_position.has_value()) {
                untrack(reg);
                return;
            }
            if (reg.spatial_id.has_value()) {
                m_index->update(*reg.spatial_id, *ptr->m_position);
            } else {
                track(id, reg, *ptr->m_position);
            }
        },
            reg.member);
    }

    m_index->publish();

    if (!m_expanses.empty()) {
        auto snapshot = m_index->all();
        for (auto& [spatial_id, position] : snapshot) {
            if (const auto it = m_entity_of_spatial.find(spatial_id); it != m_entity_of_spatial.end()) {
                spatial_id = it->second;
            }
        }

        for (auto& [xid, expanse] : m_expanses)
            expanse->evaluate(m_fabric_id, snapshot);
    }

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
    it->second.pending_cursor.reset();
}

// =============================================================================
// Spatial queries
// =============================================================================

std::vector<Kinesis::QueryResult> Fabric::within_radius(
    const glm::vec3& center, float radius) const
{
    auto results = m_index->within_radius(center, radius);
    translate(results);
    return results;
}

std::vector<Kinesis::QueryResult> Fabric::k_nearest(
    const glm::vec3& center, uint32_t k) const
{
    auto results = m_index->k_nearest(center, k);
    translate(results);
    return results;
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

void Fabric::track(uint32_t id, Registration& reg, const glm::vec3& position)
{
    reg.spatial_id = m_index->insert(position);
    m_entity_of_spatial[*reg.spatial_id] = id;
}

void Fabric::untrack(Registration& reg)
{
    if (!reg.spatial_id.has_value()) {
        return;
    }
    m_index->remove(*reg.spatial_id);
    m_entity_of_spatial.erase(*reg.spatial_id);
    reg.spatial_id.reset();
}

void Fabric::translate(std::vector<Kinesis::QueryResult>& results) const
{
    std::erase_if(results, [this](Kinesis::QueryResult& hit) {
        const auto it = m_entity_of_spatial.find(hit.id);
        if (it == m_entity_of_spatial.end()) {
            return true;
        }
        hit.id = it->second;
        return false;
    });
}

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
            ptr->follow_attachment();
            InfluenceContext ctx;
            if (ptr->m_position.has_value()) {
                ctx.position = *ptr->m_position;
            }
            ctx.intensity = ptr->m_intensity;
            ctx.radius = ptr->m_radius;
            ctx.color = ptr->m_color;
            ctx.size = ptr->m_size;
            ctx.cursor_pos = reg.pending_cursor;
            ptr->invoke(ctx);

        } else if constexpr (std::is_same_v<T, Sensor>) {
            PerceptionContext ctx;
            ctx.radius = ptr->m_query_radius;
            std::vector<Kinesis::QueryResult> results;
            if (ptr->m_position.has_value()) {
                ctx.position = *ptr->m_position;
                results = m_index->within_radius(*ptr->m_position, ptr->m_query_radius);
                translate(results);
                ctx.spatial_results = std::span(results);
            }
            ptr->invoke(ctx);

        } else if constexpr (std::is_same_v<T, Agent>) {
            ptr->follow_attachment();
            if (ptr->m_position.has_value()) {
                auto results = m_index->within_radius(*ptr->m_position, ptr->m_query_radius);
                translate(results);
                PerceptionContext pctx;
                pctx.position = *ptr->m_position;
                pctx.radius = ptr->m_query_radius;
                pctx.orientation = ptr->m_orientation;
                pctx.spatial_results = std::span(results);
                ptr->invoke_perception(pctx);

                InfluenceContext ictx;
                ictx.position = *ptr->m_position;
                ictx.orientation = ptr->m_orientation;
                ictx.intensity = ptr->m_intensity;
                ictx.radius = ptr->m_radius;
                ictx.color = ptr->m_color;
                ictx.size = ptr->m_size;
                ictx.cursor_pos = reg.pending_cursor;
                ptr->invoke_influence(ictx);
            } else {
                PerceptionContext pctx;
                pctx.radius = ptr->m_query_radius;
                pctx.orientation = ptr->m_orientation;
                ptr->invoke_perception(pctx);
                InfluenceContext ictx;
                ictx.orientation = ptr->m_orientation;
                ictx.intensity = ptr->m_intensity;
                ictx.radius = ptr->m_radius;
                ictx.color = ptr->m_color;
                ictx.size = ptr->m_size;
                ptr->invoke_influence(ictx);
            }
        }
    },
        reg.member);
}

std::shared_ptr<Emitter::InfluenceFn> Fabric::resolve_influence_fn(std::string_view name) const
{
    auto it = m_influence_fns.find(std::string(name));
    return it != m_influence_fns.end() ? it->second : nullptr;
}

std::shared_ptr<Sensor::PerceptionFn> Fabric::resolve_perception_fn(std::string_view name) const
{
    auto it = m_perception_fns.find(std::string(name));
    return it != m_perception_fns.end() ? it->second : nullptr;
}

std::shared_ptr<Expanse::ContainsFn> Fabric::resolve_expanse_fn(std::string_view name) const
{
    auto it = m_expanse_fns.find(std::string(name));
    return it != m_expanse_fns.end() ? it->second : nullptr;
}

std::shared_ptr<Expanse::CrossingFn> Fabric::resolve_crossing_fn(std::string_view name) const
{
    auto it = m_crossing_fns.find(std::string(name));
    return it != m_crossing_fns.end() ? it->second : nullptr;
}

std::vector<uint32_t> Fabric::all_expanse_ids() const
{
    std::vector<uint32_t> ids;
    ids.reserve(m_expanses.size());
    for (const auto& [id, _] : m_expanses)
        ids.push_back(id);
    return ids;
}

std::shared_ptr<Expanse> Fabric::get_expanse(uint32_t id) const
{
    auto it = m_expanses.find(id);
    return it != m_expanses.end() ? it->second : nullptr;
}

std::shared_ptr<Presence::RadiateFn> Fabric::resolve_radiate_fn(std::string_view name) const
{
    auto it = m_radiate_fns.find(std::string(name));
    return it != m_radiate_fns.end() ? it->second : nullptr;
}

} // namespace MayaFlux::Nexus
