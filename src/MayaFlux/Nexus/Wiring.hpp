#pragma once

#include "Principals/Agent.hpp"

#include "MayaFlux/IO/Keys.hpp"
#include "MayaFlux/Vruta/NetworkSource.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
class Routine;
class SoundRoutine;
class GraphicsRoutine;
class CrossRoutine;
class Event;
}

namespace MayaFlux::Nexus {

class Fabric;

/**
 * @class Wiring
 * @brief Fluent builder that wires an entity into Fabric's scheduling infrastructure.
 *
 * Obtained via @c Fabric::wire. Describes when and how the object's function
 * is invoked. Call @c finalise() to apply the configuration.
 *
 * No coroutine is created unless a scheduling modifier is set. @c bind()
 * performs an immediate single call with no coroutine. Any scheduling
 * modifier (@c every, @c on, @c move_to, @c position_from, @c use) creates
 * and registers a coroutine owned by Fabric.
 */
class MAYAFLUX_API Wiring {
public:
    using PositionFn = std::function<glm::vec3()>;
    using SoundFactory = std::function<Vruta::SoundRoutine()>;
    using GraphicsFactory = std::function<Vruta::GraphicsRoutine()>;
    using CrossFactory = std::function<Vruta::CrossRoutine()>;
    using EventFactory = std::function<Vruta::Event(Vruta::TaskScheduler&)>;
    using NullFunc = std::nullptr_t;
    static constexpr NullFunc no_release = nullptr;

    // =====================================================================
    // Scheduling modifiers
    // =====================================================================

    /**
     * @brief Fire the entity on a recurring interval.
     * @param interval_seconds Period between invocations.
     * @param token Scheduler rate to use (default: SAMPLE_ACCURATE).
     */
    Wiring& every(double interval_seconds, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Limit a recurring registration to a fixed duration then cancel.
     * @param seconds Total active time. Pairs with @c every().
     * @param token If set, uses the specified scheduler's clock for timing; otherwise defaults to SAMPLE_ACCURATE.
     */
    Wiring& for_duration(double seconds, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Fire the entity on a key event from a window.
     * @param window Source window.
     * @param key    Key to listen for.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::Keys key);

    /**
     * @brief Fire the entity repeatedly while a key is held, invoking a release callback on release.
     * @param window      Source window.
     * @param key         Key to listen for.
     * @param held        If true, fires on repeat ticks while held.
     * @param on_release  Called when the key is released.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::Keys key, bool held,
        std::function<void()> on_release = nullptr);

    /**
     * @brief Fire the entity on a mouse button event from a window.
     * @param window Source window.
     * @param button Mouse button to listen for.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::MouseButtons button);

    /**
     * @brief Fire the entity on mouse-motion events while @p button is held.
     *
     * Mirrors the key held overload. Cursor position is available via
     * InfluenceContext on each qualifying event.
     *
     * @param window Source window.
     * @param button Button that must be pressed for the entity to fire.
     * @param held   Pass @c true to select the motion-while-held path.
     * @param on_release Called when the button is released.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::MouseButtons button, bool held,
        std::function<void(double, double)> on_release = nullptr);

    /**
     * @brief Fire the entity on mouse button press and invoke a release callback on release.
     * @param window      Source window.
     * @param button      Mouse button to listen for.
     * @param on_release  Called with cursor position when the button is released. Cancelled with the wiring.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::MouseButtons button,
        std::function<void(double, double)> on_release);

    /**
     * @brief Fire the entity on each incoming message from a network source.
     * @param source OSC or raw network source.
     */
    Wiring& on(Vruta::NetworkSource& source);

    /**
     * @brief Fire the entity on each matching window event.
     * @param source Event stream.
     * @param filter Optional filter criteria.
     */
    Wiring& on(Vruta::WindowEventSource& source, Vruta::WindowEventFilter filter = {});

    /**
     * @brief Choreograph a position move as an EventChain step.
     * @param pos           Target position.
     * @param delay_seconds Delay after the previous step (0 = immediate on start).
     */
    Wiring& move_to(const glm::vec3& pos, double delay_seconds = 0.0);

    /**
     * @brief Drive position from a callable evaluated on each @c every() tick.
     * @param fn Returns the new position on each invocation.
     */
    Wiring& position_from(PositionFn fn);

    /**
     * @brief Drive position from a named callable evaluated on each @c every() tick.
     * @param fn_name Identifier used for state encoding.
     * @param fn      Returns the new position on each invocation.
     */
    Wiring& position_from(std::string fn_name, PositionFn fn);

    /**
     * @brief Repeat the configured sequence or choreography N times.
     * @param count Number of repetitions.
     */
    Wiring& times(size_t count);

    /**
     * @brief Delegate coroutine creation entirely to the caller.
     *
     * Fabric registers the entity with the spatial index and adds the
     * returned coroutine. All timing and entity commit logic is the
     * caller's responsibility.
     */
    Wiring& use(SoundFactory factory);
    Wiring& use(GraphicsFactory factory);
    Wiring& use(CrossFactory factory);
    Wiring& use(EventFactory factory);

    /** @brief Register a named factory. */
    Wiring& use(std::string fn_name, SoundFactory factory);
    Wiring& use(std::string fn_name, GraphicsFactory factory);
    Wiring& use(std::string fn_name, CrossFactory factory);
    Wiring& use(std::string fn_name, EventFactory factory);

    // =====================================================================
    // Immediate bind — no coroutine
    // =====================================================================

    /**
     * @brief Call the entity's influence function once immediately.
     *
     * No coroutine is created. If @c for_duration was set, a Timer fires
     * the detach function after expiry.
     */
    Wiring& bind();

    /**
     * @brief Call a custom function once immediately instead of the entity's own.
     * @param fn Callable to invoke in place of the entity's influence function.
     */
    Wiring& bind(std::function<void()> fn);

    /**
     * @brief Call attach immediately; call detach after @c for_duration expires
     *        or on explicit @c cancel().
     * @param attach Called immediately on finalise().
     * @param detach Called on expiry or cancellation.
     */
    Wiring& bind(std::function<void()> attach, std::function<void()> detach);

    /** @brief Call a named custom function once immediately. */
    Wiring& bind(std::string fn_name, std::function<void()> fn);

    /** @brief Call named attach/detach functions. */
    Wiring& bind(std::string attach_name, std::function<void()> attach,
        std::string detach_name, std::function<void()> detach);

    // =====================================================================
    // Terminal
    // =====================================================================

    /**
     * @brief Apply the configured wiring.
     *
     * Resolves the builder state into a coroutine (if any scheduling
     * modifier was set) or an immediate call (if @c bind was used),
     * and registers the result with Fabric.
     */
    void finalise();

    /**
     * @brief Cancel an active wiring and release any owned coroutine.
     */
    void cancel();

private:
    friend class Fabric;

    explicit Wiring(Fabric& fabric, uint32_t entity_id)
        : m_fabric(fabric)
        , m_entity_id(entity_id)
    {
    }

    // =====================================================================
    // Stored configuration
    // =====================================================================

    struct MoveStep {
        glm::vec3 position;
        double delay_seconds;
    };

    struct KeyTrigger {
        std::shared_ptr<Core::Window> window;
        IO::Keys key;
        std::optional<std::function<void()>> on_release;
        bool held {};
    };

    struct MouseTrigger {
        std::shared_ptr<Core::Window> window;
        IO::MouseButtons button;
        std::optional<std::function<void(double, double)>> on_release;
        bool held {};
    };

    struct NetworkTrigger {
        Vruta::NetworkSource* source;
    };

    struct EventTrigger {
        Vruta::EventSource* source;
        Vruta::EventFilter filter;
    };

    struct WindowEventTrigger {
        Vruta::WindowEventSource* source {};
        Vruta::WindowEventFilter filter;
    };

    using Trigger = std::variant<std::monostate, KeyTrigger, MouseTrigger, NetworkTrigger, EventTrigger, WindowEventTrigger>;
    using Factory = std::variant<std::monostate, SoundFactory, GraphicsFactory, CrossFactory>;
    using EFactory = std::optional<EventFactory>;

    std::string make_name(const char* prefix) const;
    Fabric& m_fabric;
    uint32_t m_entity_id;
    bool m_has_scheduling {};

    std::optional<double> m_interval;
    std::optional<double> m_duration;
    std::optional<PositionFn> m_position_fn;
    std::string m_position_fn_name;
    std::vector<MoveStep> m_move_steps;
    size_t m_times { 1 };
    Vruta::ProcessingToken m_metro_token { Vruta::ProcessingToken::SAMPLE_ACCURATE };
    Vruta::ProcessingToken m_duration_token { Vruta::ProcessingToken::SAMPLE_ACCURATE };

    Trigger m_trigger;
    Factory m_factory;
    EFactory m_event_factory;
    std::string m_factory_name;

    std::optional<std::function<void()>> m_bind_attach;
    std::optional<std::function<void()>> m_bind_detach;
    std::string m_bind_attach_name;
    std::string m_bind_detach_name;

    Vruta::Event window_event_source_loop(Vruta::WindowEventSource& source, Vruta::WindowEventFilter filter, Fabric& fabric, uint32_t id);

public:
    // =====================================================================
    // Move semantics
    // =====================================================================
    ~Wiring() = default;
    Wiring(const Wiring&) = delete;
    Wiring& operator=(const Wiring&) = delete;
    Wiring(Wiring&&) noexcept = default;
    Wiring& operator=(Wiring&&) = delete;

    // =====================================================================
    // Introspection
    // =====================================================================

    /** @brief Stable id of the wired entity. */
    [[nodiscard]] uint32_t entity_id() const { return m_entity_id; }

    /** @brief Recurring interval in seconds, if @c every was called. */
    [[nodiscard]] std::optional<double> interval() const { return m_interval; }

    /** @brief Active duration in seconds, if @c for_duration was called. */
    [[nodiscard]] std::optional<double> duration() const { return m_duration; }

    /** @brief Repetition count set by @c times, default 1. */
    [[nodiscard]] size_t times_count() const { return m_times; }

    /** @brief Choreography steps from @c move_to calls. */
    [[nodiscard]] const std::vector<MoveStep>& move_steps() const { return m_move_steps; }

    /** @brief Active trigger variant set by @c on. */
    [[nodiscard]] const Trigger& trigger() const { return m_trigger; }

    /** @brief Active factory variant set by @c use for non-Event factories. */
    [[nodiscard]] const Factory& factory() const { return m_factory; }

    /** @brief Active event-factory, if @c use(EventFactory) was called. */
    [[nodiscard]] const EFactory& event_factory() const { return m_event_factory; }

    /** @brief True if @c position_from was called. */
    [[nodiscard]] bool has_position_fn() const { return m_position_fn.has_value(); }

    /** @brief True if any @c bind overload was called. */
    [[nodiscard]] bool has_bind() const { return m_bind_attach.has_value(); }

    /** @brief True if @c bind(attach, detach) was called. */
    [[nodiscard]] bool has_bind_detach() const { return m_bind_detach.has_value(); }

    /** @brief Name of the position function, empty if anonymous. */
    [[nodiscard]] const std::string& position_fn_name() const { return m_position_fn_name; }

    /** @brief Name of the active factory, empty if anonymous or none. */
    [[nodiscard]] const std::string& factory_name() const { return m_factory_name; }

    /** @brief Name of the bind attach function, empty if anonymous or none. */
    [[nodiscard]] const std::string& bind_attach_name() const { return m_bind_attach_name; }

    /** @brief Name of the bind detach function, empty if anonymous or none. */
    [[nodiscard]] const std::string& bind_detach_name() const { return m_bind_detach_name; }
};

} // namespace MayaFlux::Nexus
