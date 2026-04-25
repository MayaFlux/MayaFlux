#include "Wiring.hpp"

#include "Fabric.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"
#include "MayaFlux/Kriya/Awaiters/GetPromise.hpp"
#include "MayaFlux/Kriya/Awaiters/NetworkAwaiter.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Kriya/InputEvents.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"

#include "MayaFlux/Kriya/Timers.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nexus {

// =============================================================================
// Scheduling modifiers
// =============================================================================

Wiring& Wiring::every(double interval_seconds)
{
    m_interval = interval_seconds;
    return *this;
}

Wiring& Wiring::for_duration(double seconds)
{
    m_duration = seconds;
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::Keys key)
{
    m_trigger = KeyTrigger { .window = std::move(window), .key = key };
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::MouseButtons button)
{
    m_trigger = MouseTrigger { .window = std::move(window), .button = button };
    return *this;
}

Wiring& Wiring::on(Vruta::NetworkSource& source)
{
    m_trigger = NetworkTrigger { .source = &source };
    return *this;
}

Wiring& Wiring::on(Vruta::EventSource& source, Vruta::EventFilter filter)
{
    m_trigger = EventTrigger { .source = &source, .filter = filter };
    return *this;
}

Wiring& Wiring::move_to(const glm::vec3& pos, double delay_seconds)
{
    m_move_steps.push_back({ .position = pos, .delay_seconds = delay_seconds });
    return *this;
}

Wiring& Wiring::position_from(PositionFn fn)
{
    m_position_fn = std::move(fn);
    return *this;
}

Wiring& Wiring::times(size_t count)
{
    m_times = count;
    return *this;
}

Wiring& Wiring::use(SoundFactory factory)
{
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(GraphicsFactory factory)
{
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(ComplexFactory factory)
{
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(EventFactory factory)
{
    m_event_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::position_from(std::string fn_name, PositionFn fn)
{
    m_position_fn_name = std::move(fn_name);
    m_position_fn = std::move(fn);
    return *this;
}

Wiring& Wiring::use(std::string fn_name, SoundFactory factory)
{
    m_factory_name = std::move(fn_name);
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(std::string fn_name, GraphicsFactory factory)
{
    m_factory_name = std::move(fn_name);
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(std::string fn_name, ComplexFactory factory)
{
    m_factory_name = std::move(fn_name);
    m_factory = std::move(factory);
    return *this;
}

Wiring& Wiring::use(std::string fn_name, EventFactory factory)
{
    m_factory_name = std::move(fn_name);
    m_event_factory = std::move(factory);
    return *this;
}

// =============================================================================
// Immediate bind
// =============================================================================

Wiring& Wiring::bind()
{
    m_bind_attach = [this]() { m_fabric.fire(m_entity_id); };
    return *this;
}

Wiring& Wiring::bind(std::function<void()> fn)
{
    m_bind_attach = std::move(fn);
    return *this;
}

Wiring& Wiring::bind(std::function<void()> attach, std::function<void()> detach)
{
    m_bind_attach = std::move(attach);
    m_bind_detach = std::move(detach);
    return *this;
}

Wiring& Wiring::bind(std::string fn_name, std::function<void()> fn)
{
    m_bind_attach_name = std::move(fn_name);
    m_bind_attach = std::move(fn);
    return *this;
}

Wiring& Wiring::bind(
    std::string attach_name, std::function<void()> attach,
    std::string detach_name, std::function<void()> detach)
{
    m_bind_attach_name = std::move(attach_name);
    m_bind_attach = std::move(attach);
    m_bind_detach_name = std::move(detach_name);
    m_bind_detach = std::move(detach);
    return *this;
}

// =============================================================================
// make_name
// =============================================================================

std::string Wiring::make_name(const char* prefix) const
{
    return std::string(prefix) + "_" + std::to_string(m_entity_id) + "_" + std::to_string(m_fabric.m_scheduler.get_next_task_id());
}

namespace {

    // =============================================================================
    // Network coroutine
    // =============================================================================

    Vruta::SoundRoutine network_loop(
        Vruta::NetworkSource& source,
        Fabric& fabric,
        uint32_t id)
    {
        auto& promise = co_await Kriya::GetAudioPromise {};
        while (true) {
            if (promise.should_terminate) {
                break;
            }
            co_await source.next_message();
            fabric.fire(id);
        }
    }

    // =============================================================================
    // EventSource coroutine
    // =============================================================================

    Vruta::Event event_source_loop(
        Vruta::EventSource& source,
        Fabric& fabric,
        uint32_t id)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (true) {
            if (promise.should_terminate) {
                break;
            }
            co_await source.next_event();
            fabric.fire(id);
        }
    }

}

// =============================================================================
// Terminal
// =============================================================================

void Wiring::finalise()
{
    auto& scheduler = m_fabric.m_scheduler;
    auto& ev_manager = m_fabric.m_event_manager;
    const uint32_t id = m_entity_id;
    auto& reg = m_fabric.m_registrations[id];

    bool has_any_path = !std::holds_alternative<std::monostate>(m_factory)
        || m_event_factory.has_value()
        || m_bind_attach.has_value()
        || !m_move_steps.empty()
        || !std::holds_alternative<std::monostate>(m_trigger)
        || m_interval.has_value();

    reg.commit_driven = !has_any_path;

    // ------------------------------------------------------------------
    // Routine factory path
    // ------------------------------------------------------------------
    if (!std::holds_alternative<std::monostate>(m_factory)) {
        auto name = make_name("nexus_task");
        reg.task_name = name;
        std::visit([&](auto& factory) {
            using F = std::decay_t<decltype(factory)>;
            if constexpr (!std::is_same_v<F, std::monostate>) {
                scheduler.add_task(
                    std::make_shared<std::invoke_result_t<F, Vruta::TaskScheduler&>>(factory(scheduler)),
                    name, false);
            }
        },
            m_factory);
        return;
    }

    // ------------------------------------------------------------------
    // Event factory path
    // ------------------------------------------------------------------
    if (m_event_factory.has_value()) {
        auto name = make_name("nexus_event");
        reg.event_name = name;
        ev_manager.add_event(
            std::make_shared<Vruta::Event>((*m_event_factory)(scheduler)),
            name);
        return;
    }

    // ------------------------------------------------------------------
    // Immediate bind path
    // ------------------------------------------------------------------
    if (m_bind_attach.has_value()) {
        (*m_bind_attach)();

        if (m_duration.has_value() && m_bind_detach.has_value()) {
            auto timer = std::make_shared<Kriya::Timer>(scheduler);
            auto detach = *m_bind_detach;
            timer->schedule(*m_duration, [timer, detach]() {
                detach();
            });
        }
        return;
    }

    // ------------------------------------------------------------------
    // Choreographed position path
    // ------------------------------------------------------------------
    if (!m_move_steps.empty()) {
        auto name = make_name("nexus_chain");
        reg.chain_name = name;

        Kriya::EventChain chain(scheduler, name);
        auto& fab = m_fabric;
        for (auto& step : m_move_steps) {
            glm::vec3 pos = step.position;
            chain.then([&fab, pos, id]() {
                std::visit([&pos](const auto& ptr) {
                    ptr->set_position(pos);
                },
                    fab.m_registrations[id].member);
                fab.fire(id);
            },
                step.delay_seconds);
        }
        if (m_times > 1) {
            chain.times(m_times);
        }
        chain.start();
        return;
    }

    // ------------------------------------------------------------------
    // Trigger path
    // ------------------------------------------------------------------
    if (!std::holds_alternative<std::monostate>(m_trigger)) {
        std::visit([&](auto& trig) {
            using T = std::decay_t<decltype(trig)>;

            if constexpr (std::is_same_v<T, KeyTrigger>) {
                auto name = make_name("nexus_event");
                reg.event_name = name;
                auto& fab = m_fabric;
                ev_manager.add_event(
                    std::make_shared<Vruta::Event>(
                        Kriya::key_pressed(trig.window, trig.key,
                            [&fab, id]() { fab.fire(id); })),
                    name);

            } else if constexpr (std::is_same_v<T, MouseTrigger>) {
                auto name = make_name("nexus_event");
                reg.event_name = name;
                ev_manager.add_event(
                    std::make_shared<Vruta::Event>(
                        Kriya::mouse_pressed(trig.window, trig.button,
                            [this, id](double, double) { m_fabric.fire(id); })),
                    name);

            } else if constexpr (std::is_same_v<T, NetworkTrigger>) {
                auto name = make_name("nexus_task");
                reg.task_name = name;
                scheduler.add_task(
                    std::make_shared<Vruta::SoundRoutine>(
                        network_loop(*trig.source, m_fabric, id)),
                    name, false);

            } else if constexpr (std::is_same_v<T, EventTrigger>) {
                auto name = make_name("nexus_event");
                reg.event_name = name;
                ev_manager.add_event(
                    std::make_shared<Vruta::Event>(
                        event_source_loop(*trig.source, m_fabric, id)),
                    name);
            }
        },
            m_trigger);
        return;
    }

    // ------------------------------------------------------------------
    // Metro path
    // ------------------------------------------------------------------
    if (m_interval.has_value()) {
        auto pos_fn = m_position_fn;
        auto name = make_name("nexus_metro");
        reg.task_name = name;
        auto& fab = m_fabric;

        scheduler.add_task(
            std::make_shared<Vruta::SoundRoutine>(
                Kriya::metro(scheduler, *m_interval, [&fab, id, pos_fn]() mutable {
                    if (pos_fn.has_value()) {
                        glm::vec3 p = (*pos_fn)();
                        std::visit([&p](const auto& ptr) {
                            ptr->set_position(p);
                        },
                            fab.m_registrations[id].member);
                    }
                    fab.fire(id);
                })),
            name, false);

        if (m_duration.has_value()) {
            auto cancel_name = make_name("nexus_metro_cancel");
            auto timer = std::make_shared<Kriya::Timer>(scheduler);
            timer->schedule(*m_duration, [name, timer, &scheduler]() {
                scheduler.cancel_task(name);
            });
        }
    }

    std::visit([&](const auto& ptr) {
        using T = std::decay_t<decltype(*ptr)>;
        if constexpr (std::is_same_v<T, Emitter>) {
            if (!ptr->fn_name().empty()) {
                m_fabric.m_influence_fns.try_emplace(
                    ptr->fn_name(), std::make_shared<Emitter::InfluenceFn>(ptr->fn()));
            }
        } else if constexpr (std::is_same_v<T, Sensor>) {
            if (!ptr->fn_name().empty()) {
                m_fabric.m_perception_fns.try_emplace(
                    ptr->fn_name(), std::make_shared<Sensor::PerceptionFn>(ptr->fn()));
            }
        } else if constexpr (std::is_same_v<T, Agent>) {
            if (!ptr->influence_fn_name().empty()) {
                m_fabric.m_influence_fns.try_emplace(
                    ptr->influence_fn_name(), std::make_shared<Emitter::InfluenceFn>(ptr->influence_fn()));
            }
            if (!ptr->perception_fn_name().empty()) {
                m_fabric.m_perception_fns.try_emplace(
                    ptr->perception_fn_name(), std::make_shared<Sensor::PerceptionFn>(ptr->perception_fn()));
            }
        }
    },
        m_fabric.m_registrations[m_entity_id].member);

    m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
}

// =============================================================================
// Cancel
// =============================================================================

void Wiring::cancel()
{
    auto& reg = m_fabric.m_registrations[m_entity_id];

    if (!reg.task_name.empty()) {
        m_fabric.m_scheduler.cancel_task(reg.task_name);
        reg.task_name.clear();
    }
    if (!reg.chain_name.empty()) {
        m_fabric.m_scheduler.cancel_task(reg.chain_name);
        reg.chain_name.clear();
    }
    if (!reg.event_name.empty()) {
        m_fabric.m_event_manager.cancel_event(reg.event_name);
        reg.event_name.clear();
    }

    if (m_bind_detach.has_value()) {
        (*m_bind_detach)();
    }
}

} // namespace MayaFlux::Nexus
