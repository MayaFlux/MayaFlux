#include "Wiring.hpp"

#include "Fabric.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Kriya/InputEvents.hpp"
#include "MayaFlux/Kriya/NetworkEvents.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"

#include "MayaFlux/Kriya/Timers.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nexus/Principals/Locus.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nexus {

// =============================================================================
// Scheduling modifiers
// =============================================================================

Wiring& Wiring::every(double interval_seconds, Vruta::ProcessingToken token)
{
    m_interval = interval_seconds;
    m_metro_token = token;
    return *this;
}

Wiring& Wiring::every(Vruta::ProcessingToken token)
{
    return every(0.0, token);
}

Wiring& Wiring::for_duration(double seconds, Vruta::ProcessingToken token)
{
    m_duration = seconds;
    m_duration_token = token;
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::ModifiedKey key)
{
    m_triggers.emplace_back(KeyTrigger { .window = std::move(window), .key = key });
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::ModifiedKey key, bool held,
    std::function<void()> on_release)
{
    m_triggers.emplace_back(KeyTrigger {
        .window = std::move(window),
        .key = key,
        .on_release = on_release ? std::optional<std::function<void()>>(std::move(on_release)) : std::nullopt,
        .held = held
    });
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::MouseButtons button)
{
    m_triggers.emplace_back(MouseTrigger { .window = std::move(window), .button = button });
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::MouseButtons button, bool held, std::function<void(double, double)> on_release)
{
    m_triggers.emplace_back(MouseTrigger {
        .window = std::move(window),
        .button = button,
        .on_release = on_release ? std::optional<std::function<void(double, double)>>(std::move(on_release)) : std::nullopt,
        .held = held
    });

    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Core::Window> window, IO::MouseButtons button,
    std::function<void(double, double)> on_release)
{
    m_triggers.emplace_back(MouseTrigger {
        .window = std::move(window),
        .button = button,
        .on_release = std::move(on_release)
    });
    return *this;
}

Wiring& Wiring::on(std::shared_ptr<Vruta::NetworkSource> source)
{
    m_triggers.emplace_back(NetworkTrigger { .source = std::move(source) });
    return *this;
}

Wiring& Wiring::window_filter(std::shared_ptr<Core::Window> window, Vruta::WindowEventFilter filter)
{
    m_triggers.emplace_back(WindowEventTrigger { .window = std::move(window), .filter = std::move(filter) });
    return *this;
}

Wiring& Wiring::on_scroll(std::shared_ptr<Core::Window> window,
    std::function<void(double, double)> fn)
{
    m_triggers.emplace_back(ScrollTrigger { .window = std::move(window), .fn = std::move(fn) });
    return *this;
}

Wiring& Wiring::move_to(const glm::vec3& pos, double delay_seconds)
{
    m_move_steps.push_back({ .position = pos, .delay_seconds = delay_seconds });
    return *this;
}

Wiring& Wiring::place_from(PlacementFn fn)
{
    m_placement_fn = std::move(fn);
    return *this;
}

Wiring& Wiring::place_from(std::string fn_name, PlacementFn fn)
{
    m_position_fn_name = std::move(fn_name);
    return place_from(std::move(fn));
}

Wiring& Wiring::position_from(PositionFn fn)
{
    if (!fn) {
        return place_from(PlacementFn {});
    }
    return place_from([fn = std::move(fn)] { return PlacementControls { .position = fn() }; });
}

Wiring& Wiring::on_complete(std::function<void()> fn)
{
    m_on_complete = std::move(fn);
    return *this;
}

Wiring& Wiring::on_detach(std::function<void()> fn)
{
    m_on_detach = std::move(fn);
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

Wiring& Wiring::use(CrossFactory factory)
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
    return position_from(std::move(fn));
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

Wiring& Wiring::use(std::string fn_name, CrossFactory factory)
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
    return std::string(prefix) + "_" + std::to_string(m_entity_id) + "_" + std::to_string(m_fabric.m_scheduler->get_next_task_id());
}

namespace {

    /**
     * Completion handler for a routine or event the caller made. It reports a
     * termination request as well as a natural end, and only the latter counts.
     */
    template <typename Handler>
    std::function<void()> finish_unless_stopped(
        const std::shared_ptr<Handler>& handler,
        const std::shared_ptr<WiringEnding>& ending)
    {
        return [running = std::weak_ptr<Handler>(handler), ending] {
            const auto live = running.lock();
            if (live && !live->get_should_terminate()) {
                ending->finish();
            }
        };
    }

    struct FireLimit {
        size_t remaining;
        bool limited;
        std::function<void()> stop;

        [[nodiscard]] bool spent() const { return limited && remaining == 0; }

        void fired()
        {
            if (limited && remaining > 0 && --remaining == 0 && stop) {
                stop();
            }
        }
    };
}

// =============================================================================
// Terminal
// =============================================================================

void Wiring::finalise()
{
    const auto scheduler_ptr = m_fabric.m_scheduler;
    const auto ev_manager_ptr = m_fabric.m_event_manager;
    auto& scheduler = *scheduler_ptr;
    auto& ev_manager = *ev_manager_ptr;
    const uint32_t id = m_entity_id;
    auto& reg = m_fabric.m_registrations[id];

    bool has_any_path = !std::holds_alternative<std::monostate>(m_factory)
        || m_event_factory.has_value()
        || m_bind_attach.has_value()
        || !m_move_steps.empty()
        || !m_triggers.empty()
        || m_interval.has_value();

    reg.commit_driven = !has_any_path;
    reg.placement = m_placement_fn;

    const auto ending = std::make_shared<WiringEnding>();
    ending->complete = m_on_complete;
    if (m_bind_detach.has_value() || m_on_detach) {
        ending->detach = [bound = m_bind_detach.value_or(nullptr), then = m_on_detach] {
            if (bound) {
                bound();
            }
            if (then) {
                then();
            }
        };
    }
    reg.ending = ending;
    const bool watched = ending->complete || ending->detach;

    // ------------------------------------------------------------------
    // Register named callables before any early return.
    // ------------------------------------------------------------------
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
            if (const auto presence = std::dynamic_pointer_cast<Presence>(ptr);
                presence && !presence->radiate_fn_name().empty() && presence->radiate_fn()) {
                m_fabric.m_radiate_fns.try_emplace(
                    presence->radiate_fn_name(), std::make_shared<Presence::RadiateFn>(presence->radiate_fn()));
            }
        }
    },
        reg.member);

    // ------------------------------------------------------------------
    // Routine factory path
    // ------------------------------------------------------------------
    if (!std::holds_alternative<std::monostate>(m_factory)) {
        auto name = make_name("nexus_task");
        reg.task_name = name;
        std::visit([&](auto& factory) {
            using F = std::decay_t<decltype(factory)>;
            if constexpr (!std::is_same_v<F, std::monostate>) {
                const auto routine = std::make_shared<std::invoke_result_t<F>>(factory());
                if (watched) {
                    routine->on_complete(finish_unless_stopped(routine, ending));
                }
                scheduler.add_task(routine, name, false);
            }
        },
            m_factory);
        m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
        return;
    }

    // ------------------------------------------------------------------
    // Event factory path
    // ------------------------------------------------------------------
    if (m_event_factory.has_value()) {
        auto name = make_name("nexus_event");
        reg.event_names.push_back(name);
        const auto event = std::make_shared<Vruta::Event>((*m_event_factory)(scheduler));
        if (watched) {
            event->on_complete(finish_unless_stopped(event, ending));
        }
        ev_manager.add_event(event, name);
        m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
        return;
    }

    // ------------------------------------------------------------------
    // Immediate bind path
    // ------------------------------------------------------------------
    if (m_bind_attach.has_value()) {
        (*m_bind_attach)();

        if (m_duration.has_value() && watched) {
            auto timer = std::make_shared<Kriya::Timer>(scheduler_ptr, m_duration_token);
            timer->schedule(*m_duration, [timer, ending]() {
                ending->finish();
            });
        }
        m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
        return;
    }

    // ------------------------------------------------------------------
    // Choreographed position path
    // ------------------------------------------------------------------
    if (!m_move_steps.empty()) {
        auto name = make_name("nexus_chain");
        reg.chain_name = name;

        Kriya::EventChain chain(
            scheduler_ptr, name, m_metro_token);
        auto& fab = m_fabric;
        const auto steps_left = std::make_shared<size_t>(m_move_steps.size() * std::max<size_t>(m_times, 1));
        for (auto& step : m_move_steps) {
            glm::vec3 pos = step.position;
            chain.then([&fab, pos, id, ending, steps_left]() {
                std::visit([&pos](const auto& ptr) {
                    ptr->set(PlacementControls { .position = pos });
                },
                    fab.m_registrations[id].member);
                fab.fire(id);
                if (*steps_left > 0 && --*steps_left == 0) {
                    ending->finish();
                }
            },
                step.delay_seconds);
        }
        if (m_times > 1) {
            chain.times(m_times);
        }
        chain.start();
        m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
        return;
    }

    // ------------------------------------------------------------------
    // Trigger path
    // ------------------------------------------------------------------
    if (!m_triggers.empty()) {
        const auto limit = std::make_shared<FireLimit>(FireLimit { .remaining = m_times, .limited = m_times > 1, .stop = {} });
        const auto stoppable = std::make_shared<std::vector<std::string>>();
        limit->stop = [ev_manager_ptr, stoppable, ending]() {
            for (const auto& name : *stoppable) {
                if (const auto event = ev_manager_ptr->get_event(name)) {
                    event->set_should_terminate(true);
                }
            }
            ending->finish();
        };

        using FireAt = std::function<void(std::optional<glm::vec2>, const Core::WindowEvent*)>;
        const FireAt fire_at = [&fab = m_fabric, id, limit](std::optional<glm::vec2> cursor, const Core::WindowEvent* input) {
            if (limit->spent()) {
                return;
            }
            fab.fire(id, cursor, input);
            limit->fired();
        };

        const auto add = [&](Vruta::Event event, const char* prefix, bool counted) {
            auto name = make_name(prefix);
            reg.event_names.push_back(name);
            if (counted) {
                stoppable->push_back(name);
            }
            ev_manager.add_event(std::make_shared<Vruta::Event>(std::move(event)), name);
        };

        for (auto& trigger : m_triggers) {
            std::visit([&](auto& trig) {
                using T = std::decay_t<decltype(trig)>;

                if constexpr (std::is_same_v<T, KeyTrigger>) {
                    Vruta::WindowEventFilter filter { trig.key };
                    if (!trig.held) {
                        filter.event_type = Core::WindowEventType::KEY_PRESSED;
                    }
                    add(Kriya::window_event(trig.window, filter,
                            [fire_at, held = trig.held](const Core::WindowEvent& event) {
                                if (held && event.type == Core::WindowEventType::KEY_RELEASED) {
                                    return;
                                }
                                fire_at(std::nullopt, &event);
                            }),
                        "nexus_event", true);
                    if (trig.on_release) {
                        add(Kriya::key_released(trig.window, trig.key.key, *trig.on_release),
                            "nexus_event_release", false);
                    }

                } else if constexpr (std::is_same_v<T, MouseTrigger>) {
                    Vruta::WindowEventFilter filter;
                    if (trig.held) {
                        filter.event_type = Core::WindowEventType::MOUSE_MOTION;
                    } else {
                        filter.event_type = Core::WindowEventType::MOUSE_BUTTON_PRESSED;
                        filter.button = trig.button;
                    }
                    add(Kriya::window_event(trig.window, filter,
                            [fire_at, window = trig.window, button = trig.button, held = trig.held](const Core::WindowEvent& event) {
                                auto& source = window->get_event_source();
                                if (!held) {
                                    const auto [x, y] = source.get_mouse_position();
                                    fire_at(glm::vec2(x, y), &event);
                                    return;
                                }
                                if (!source.is_mouse_pressed(static_cast<int>(button))) {
                                    return;
                                }
                                if (const auto* pos = std::get_if<Core::WindowEvent::MousePosData>(&event.data)) {
                                    fire_at(glm::vec2(pos->x, pos->y), &event);
                                }
                            }),
                        "nexus_event", true);
                    if (trig.on_release) {
                        add(Kriya::mouse_released(trig.window, trig.button, *trig.on_release),
                            "nexus_event_release", false);
                    }

                } else if constexpr (std::is_same_v<T, ScrollTrigger>) {
                    add(Kriya::mouse_scrolled(trig.window,
                            [limit, fn = trig.fn](double x, double y) {
                                if (limit->spent()) {
                                    return;
                                }
                                fn(x, y);
                                limit->fired();
                            }),
                        "nexus_scroll", true);

                } else if constexpr (std::is_same_v<T, NetworkTrigger>) {
                    add(Kriya::message_received(trig.source,
                            [fire_at](const Core::NetworkMessage&) { fire_at(std::nullopt, nullptr); }),
                        "nexus_event", true);

                } else if constexpr (std::is_same_v<T, BroadcastTrigger>) {
                    add(trig.listen([fire_at] { fire_at(std::nullopt, nullptr); }),
                        "nexus_event", true);

                } else if constexpr (std::is_same_v<T, WindowEventTrigger>) {
                    add(Kriya::window_event(trig.window, trig.filter,
                            [fire_at](const Core::WindowEvent& event) {
                                std::optional<glm::vec2> cursor;
                                if (const auto* pos = std::get_if<Core::WindowEvent::MousePosData>(&event.data)) {
                                    cursor = glm::vec2(pos->x, pos->y);
                                }
                                fire_at(cursor, &event);
                            }),
                        "nexus_event", true);
                }
            },
                trigger);
        }
        m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
        return;
    }

    // ------------------------------------------------------------------
    // Metro path
    // ------------------------------------------------------------------
    if (m_interval.has_value()) {
        if (m_metro_token == Vruta::ProcessingToken::SAMPLE_ACCURATE) {
            const bool drives_graphics = std::visit([](const auto& ptr) {
                using T = std::decay_t<decltype(*ptr)>;
                if constexpr (std::is_same_v<T, Emitter>) {
                    return !ptr->render_sinks().empty();
                } else if constexpr (std::is_same_v<T, Agent>) {
                    return !ptr->render_sinks().empty() || std::dynamic_pointer_cast<Locus>(ptr) != nullptr;
                } else {
                    return false;
                }
            },
                reg.member);
            if (drives_graphics) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
                    "Entity {} renders or navigates but every() runs on the audio token; "
                    "pass Vruta::ProcessingToken::FRAME_ACCURATE to run it on the graphics clock",
                    m_entity_id);
            }
        }

        auto name = make_name("nexus_metro");
        reg.task_name = name;
        auto& fab = m_fabric;
        const auto limit = std::make_shared<FireLimit>(FireLimit { .remaining = m_times, .limited = m_times > 1, .stop = {} });

        const auto cycle = [&fab, id, limit]() {
            if (limit->spent()) {
                return;
            }
            fab.fire(id);
            limit->fired();
        };

        auto routine = *m_interval > 0.0
            ? Kriya::metro(*m_interval, cycle, m_metro_token)
            : Kriya::each_cycle(cycle, m_metro_token);

        if (!routine) {
            reg.task_name.clear();
            m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
            return;
        }

        limit->stop = [running = std::weak_ptr<Vruta::Routine>(routine), ending]() {
            if (const auto live = running.lock()) {
                live->set_should_terminate(true);
            }
            ending->finish();
        };
        scheduler.add_task(routine, name, false);

        if (m_duration.has_value()) {
            auto timer = std::make_shared<Kriya::Timer>(scheduler_ptr, m_duration_token);
            timer->schedule(*m_duration, [name, timer, scheduler_ptr, ending]() {
                scheduler_ptr->cancel_task(name);
                ending->finish();
            });
        }
    }

    m_fabric.m_registrations[m_entity_id].wiring.emplace(std::move(*this));
}

// =============================================================================
// Cancel
// =============================================================================

void Wiring::cancel()
{
    auto& reg = m_fabric.m_registrations[m_entity_id];
    const auto ending = reg.ending;
    if (ending) {
        ending->closed = true;
    }
    reg.placement = nullptr;
    reg.commit_driven = false;

    if (!reg.task_name.empty()) {
        m_fabric.m_scheduler->cancel_task(reg.task_name);
        reg.task_name.clear();
    }
    if (!reg.chain_name.empty()) {
        m_fabric.m_scheduler->cancel_task(reg.chain_name);
        reg.chain_name.clear();
    }
    for (const auto& name : reg.event_names) {
        m_fabric.m_event_manager->cancel_event(name);
    }
    reg.event_names.clear();

    if (ending) {
        ending->release();
    }
}

} // namespace MayaFlux::Nexus
