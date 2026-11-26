#pragma once

#include "Principals/Agent.hpp"

#include "MayaFlux/IO/Keys.hpp"
#include "MayaFlux/Kriya/BroadcastEvents.hpp"
#include "MayaFlux/Transitive/Reflect/FunctionCall.hpp"
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
class StateEncoder;

/**
 * @struct WiringEnding
 * @brief The completion and detach callbacks of one finalised wiring, each run at most once.
 */
struct WiringEnding {
    std::function<void()> complete;
    std::function<void()> detach;
    bool closed {};

    /** @brief The wiring ran out by itself: complete, then detach. Ignored once closed. */
    void finish()
    {
        if (closed) {
            return;
        }
        closed = true;
        if (auto fn = std::exchange(complete, nullptr)) {
            fn();
        }
        release();
    }

    /** @brief The wiring is being taken away: close it and detach. */
    void release()
    {
        closed = true;
        if (auto fn = std::exchange(detach, nullptr)) {
            fn();
        }
    }
};

/**
 * @class Wiring
 * @brief Fluent builder that wires an entity into Fabric's scheduling infrastructure.
 *
 * Obtained via @c Fabric::wire. Describes when and how the object's function
 * is invoked. Call @c finalise() to apply the configuration.
 *
 * No coroutine is created unless a scheduling modifier is set. @c bind()
 * performs an immediate single call with no coroutine. Any scheduling
 * modifier (@c every, @c on, @c move_to, @c use) creates and registers a
 * coroutine owned by Fabric. A wiring with none of these is fired by commit.
 *
 * @c on and @c on_scroll add a trigger, so one wiring may hold any number of them:
 * every key, button and window event an entity answers to goes through one wiring.
 * The triggers share @c times, and a firing a window event caused carries that event
 * in @c InfluenceContext::input, so the entity can tell which trigger fired.
 */
class MAYAFLUX_API Wiring {
public:
    using PositionFn = std::function<glm::vec3()>;
    using PlacementFn = std::function<PlacementControls()>;
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
     * @param interval_seconds Period between invocations; zero or less fires once per cycle of the token.
     * @param token Scheduler rate to use (default: SAMPLE_ACCURATE). Every token runs on its own clock, and a choreography built with @c move_to uses it too.
     */
    Wiring& every(double interval_seconds, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Fire the entity once per cycle of a clock.
     *
     * No seconds-to-unit rounding: FRAME_ACCURATE fires once per frame, SAMPLE_ACCURATE
     * once per sample. Equivalent to @c every(0.0, token).
     * @param token Scheduler rate whose cycle drives the entity.
     */
    Wiring& every(Vruta::ProcessingToken token);

    /**
     * @brief Limit a recurring registration to a fixed duration then cancel.
     * @param seconds Total active time. Pairs with @c every().
     * @param token If set, uses the specified scheduler's clock for timing; otherwise defaults to SAMPLE_ACCURATE.
     */
    Wiring& for_duration(double seconds, Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /**
     * @brief Fire the entity on a key event from a window.
     * @param window Source window.
     * @param key    Key to listen for, with optional modifiers (IO::Keys::S | IO::Modifiers::Ctrl).
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::ModifiedKey key);

    /**
     * @brief Fire the entity repeatedly while a key is held, invoking a release callback on release.
     * @param window      Source window.
     * @param key         Key to listen for, with optional modifiers. The release
     *                    callback matches the key alone.
     * @param held        If true, fires on repeat ticks while held.
     * @param on_release  Called when the key is released.
     */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::ModifiedKey key, bool held,
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

    /** @brief A key trigger whose release callback can be made again from its call. */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::ModifiedKey key, bool held,
        Reflect::FunctionCall release_call, std::function<void()> on_release);

    /** @brief A mouse trigger whose release callback can be made again from its call. */
    Wiring& on(std::shared_ptr<Core::Window> window, IO::MouseButtons button, bool held,
        Reflect::FunctionCall release_call, std::function<void(double, double)> on_release);

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
     * @param source OSC or raw network source; the wiring keeps it alive.
     */
    Wiring& on(std::shared_ptr<Vruta::NetworkSource> source);

    /**
     * @brief Fire the entity on each value a broadcast source signals.
     *
     * The firing runs on the thread that signals the source, so a Kriya::Course event
     * fires the entity on the routine that advances the course. The value is not passed
     * on: the entity is fired as for any trigger. A source holds one pending value, so
     * give each wiring its own, as Course::subscribe does.
     * @tparam T Payload type of the source.
     * @param source Source to listen to; the wiring keeps it alive.
     */
    template <typename T>
    Wiring& on(std::shared_ptr<Vruta::BroadcastSource<T>> source)
    {
        m_triggers.emplace_back(BroadcastTrigger {
            .listen = [source](std::function<void()> fire) {
                return Kriya::on_signal(source, [fire = std::move(fire)](const T&) { fire(); });
            },
            .source = source });
        return *this;
    }

    /**
     * @brief on() for a broadcast source, handing each value over before the entity fires.
     *
     * This is how the entity learns what fired it: for a Course, the id of the marker
     * passed. A source holds one pending value, so values signalled before the wiring
     * resumes reach it as the last of them.
     * @param source  Source to listen to; the wiring keeps it alive.
     * @param receive Called with the value, on the signalling thread, just before the firing.
     *
     * @code
     * auto door = course->add_marker("door", 4.0);
     * auto passed = std::make_shared<uint64_t>(0);
     *
     * fabric->wire(emitter)
     *     .on(course->subscribe(Kriya::CourseEventKind::Marked),
     *         [passed](const Kriya::CourseEvent& event) { *passed = event.value; })
     *     .finalise();
     * @endcode
     */
    template <typename T>
    Wiring& on(std::shared_ptr<Vruta::BroadcastSource<T>> source,
        std::type_identity_t<std::function<void(const T&)>> receive)
    {
        const bool receives = static_cast<bool>(receive);
        m_triggers.emplace_back(BroadcastTrigger {
            .listen = [source, receive = std::move(receive)](std::function<void()> fire) {
                return Kriya::on_signal(source, [fire = std::move(fire), receive](const T& value) {
                    if (receive) {
                        receive(value);
                    }
                    fire();
                });
            },
            .source = source,
            .receives = receives });
        return *this;
    }

    /**
     * @brief Fire the entity on each matching event of a window.
     *
     * A mouse position carried by the event is made available to the entity as the cursor.
     * @param window Source window.
     * @param filter Filter criteria; an empty filter matches every event. Constrained to
     *               exactly WindowEventFilter so a key or ModifiedKey still selects the key overload.
     */
    template <std::same_as<Vruta::WindowEventFilter> Filter>
    Wiring& on(std::shared_ptr<Core::Window> window, Filter filter)
    {
        return window_filter(std::move(window), std::move(filter));
    }

    /**
     * @brief Fire the entity on each mouse scroll event from a window.
     *
     * The scroll delta (dx, dy) is passed directly to the emitter's fn via a
     * shared glm::vec2 written before invoke(). No new InfluenceContext fields
     * are added; read the delta through the captured shared_ptr in your fn.
     *
     * @param window Source window.
     * @param fn     Called with (dx, dy) on each scroll event.
     */
    Wiring& on_scroll(std::shared_ptr<Core::Window> window,
        std::function<void(double, double)> fn);

    /** @brief on_scroll with the call that made the function, so it can be made again. */
    Wiring& on_scroll(std::shared_ptr<Core::Window> window,
        Reflect::FunctionCall call, std::function<void(double, double)> fn);

    /**
     * @brief Choreograph a position move as an EventChain step.
     * @param pos           Target position.
     * @param delay_seconds Delay after the previous step (0 = immediate on start).
     */
    Wiring& move_to(const glm::vec3& pos, double delay_seconds = 0.0);

    /**
     * @brief Drive position and orientation from a callable evaluated before each firing.
     *
     * Applies to every way the entity is fired: commit, every, triggers, move_to and
     * Fabric::fire. A member the source leaves empty stays as it is. Where move_to
     * also sets the position, the source wins. A Locus takes its placement from its
     * viewpoint instead.
     * @param fn Returns the placement to apply.
     */
    Wiring& place_from(PlacementFn fn);

    /**
     * @brief Same, with a name used for state encoding.
     * @param fn_name Identifier used for state encoding.
     * @param fn      Returns the placement to apply.
     */
    Wiring& place_from(std::string fn_name, PlacementFn fn);

    /** @brief place_from with the call that made the source, so it can be made again. */
    Wiring& place_from(Reflect::FunctionCall call, PlacementFn fn);

    /**
     * @brief place_from for position alone.
     * @param fn Returns the new position.
     */
    Wiring& position_from(PositionFn fn);

    /**
     * @brief place_from for position alone, with a name used for state encoding.
     * @param fn_name Identifier used for state encoding.
     * @param fn      Returns the new position.
     */
    Wiring& position_from(std::string fn_name, PositionFn fn);

    /** @brief position_from with the call that made the source. */
    Wiring& position_from(Reflect::FunctionCall call, PositionFn fn);

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
    Wiring& use(Reflect::FunctionCall call, SoundFactory factory);
    Wiring& use(std::string fn_name, GraphicsFactory factory);
    Wiring& use(Reflect::FunctionCall call, GraphicsFactory factory);
    Wiring& use(std::string fn_name, CrossFactory factory);
    Wiring& use(Reflect::FunctionCall call, CrossFactory factory);
    Wiring& use(std::string fn_name, EventFactory factory);
    Wiring& use(Reflect::FunctionCall call, EventFactory factory);

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

    /** @brief Call once a custom function, with the call that made it. */
    Wiring& bind(Reflect::FunctionCall call, std::function<void()> fn);

    /** @brief Call named attach/detach functions. */
    Wiring& bind(std::string attach_name, std::function<void()> attach,
        std::string detach_name, std::function<void()> detach);

    /** @brief Attach and detach functions, with the calls that made them. */
    Wiring& bind(Reflect::FunctionCall attach_call, std::function<void()> attach,
        Reflect::FunctionCall detach_call, std::function<void()> detach);

    // =====================================================================
    // Endings
    // =====================================================================

    /**
     * @brief Run @p fn when the wiring runs out by itself.
     *
     * That is: times is used up, for_duration expires, a move_to choreography ends
     * or the routine or event from use returns. Never runs for cancel, remove or
     * the end of the Fabric. times(1) on every or on a trigger means no limit, so it
     * never runs out. Replaces any on_complete a factory set on its own routine.
     * @param fn Called once, on the thread that ends the wiring.
     */
    Wiring& on_complete(std::function<void()> fn);

    /** @brief on_complete with the call that made the function. */
    Wiring& on_complete(Reflect::FunctionCall call, std::function<void()> fn);

    /**
     * @brief Run @p fn once when the wiring stops, for whatever reason.
     *
     * After on_complete when it runs out, and on cancel, Fabric::remove or the end
     * of the Fabric. Runs after a bind detach, if there is one.
     * @param fn Called once.
     */
    Wiring& on_detach(std::function<void()> fn);

    /** @brief on_detach with the call that made the function. */
    Wiring& on_detach(Reflect::FunctionCall call, std::function<void()> fn);

    /**
     * @brief Set up the wiring with code and keep its call, so a loaded state can run it again.
     *
     * For what the other methods cannot name, such as a broadcast trigger. The
     * triggers and choreography steps @p apply adds are saved as the call alone.
     * @param call  Call that makes @p apply from a Reflect::FunctionTable.
     * @param apply Run now on this wiring.
     */
    Wiring& configure(Reflect::FunctionCall call, const std::function<void(Wiring&)>& apply);

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
     *
     * Runs the bind detach and on_detach once, whether it is called again or a
     * duration timer fires afterwards. The placement source stops with it.
     */
    void cancel();

private:
    friend class Fabric;
    friend class StateEncoder;

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
        IO::ModifiedKey key;
        std::optional<std::function<void()>> on_release;
        bool held {};
        Reflect::FunctionCall release_call;
    };

    struct MouseTrigger {
        std::shared_ptr<Core::Window> window;
        IO::MouseButtons button;
        std::optional<std::function<void(double, double)>> on_release;
        bool held {};
        Reflect::FunctionCall release_call;
    };

    struct ScrollTrigger {
        std::shared_ptr<Core::Window> window;
        std::function<void(double, double)> fn;
        Reflect::FunctionCall call;
    };

    struct NetworkTrigger {
        std::shared_ptr<Vruta::NetworkSource> source;
    };

    struct WindowEventTrigger {
        std::shared_ptr<Core::Window> window;
        Vruta::WindowEventFilter filter;
    };

    struct BroadcastTrigger {
        std::function<Vruta::Event(std::function<void()>)> listen;
        std::shared_ptr<const void> source {}; ///< Identifies what is listened to, for saving.
        bool receives {}; ///< A receive function was given, which cannot be saved.
    };

    using Trigger = std::variant<
        std::monostate,
        KeyTrigger,
        MouseTrigger,
        NetworkTrigger,
        WindowEventTrigger,
        ScrollTrigger,
        BroadcastTrigger>;
    using Factory = std::variant<std::monostate, SoundFactory, GraphicsFactory, CrossFactory>;
    using EFactory = std::optional<EventFactory>;

    std::string make_name(const char* prefix) const;
    Fabric& m_fabric;
    uint32_t m_entity_id;
    bool m_has_scheduling {};

    std::optional<double> m_interval;
    std::optional<double> m_duration;
    PlacementFn m_placement_fn;
    std::function<void()> m_on_complete;
    std::function<void()> m_on_detach;
    Reflect::FunctionCall m_complete_call;
    Reflect::FunctionCall m_detach_call;
    Reflect::FunctionCall m_position_call;
    std::vector<MoveStep> m_move_steps;
    size_t m_times { 1 };
    Vruta::ProcessingToken m_metro_token { Vruta::ProcessingToken::SAMPLE_ACCURATE };
    Vruta::ProcessingToken m_duration_token { Vruta::ProcessingToken::SAMPLE_ACCURATE };

    std::vector<Trigger> m_triggers;
    Factory m_factory;
    EFactory m_event_factory;
    Reflect::FunctionCall m_factory_call;

    std::optional<std::function<void()>> m_bind_attach;
    bool m_bind_self {};
    std::vector<Reflect::FunctionCall> m_configurations;
    std::vector<size_t> m_configured_triggers;
    std::vector<size_t> m_configured_steps;
    std::optional<std::function<void()>> m_bind_detach;
    Reflect::FunctionCall m_bind_attach_call;
    Reflect::FunctionCall m_bind_detach_call;

    Wiring& window_filter(std::shared_ptr<Core::Window> window, Vruta::WindowEventFilter filter);

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

    /** @brief True if @c on_scroll was called. */
    [[nodiscard]] bool is_scroll() const
    {
        return std::ranges::any_of(m_triggers, [](const Trigger& t) { return std::holds_alternative<ScrollTrigger>(t); });
    }

    /** @brief Triggers added by @c on and @c on_scroll, in the order they were added. */
    [[nodiscard]] const std::vector<Trigger>& triggers() const { return m_triggers; }

    /** @brief Active factory variant set by @c use for non-Event factories. */
    [[nodiscard]] const Factory& factory() const { return m_factory; }

    /** @brief Active event-factory, if @c use(EventFactory) was called. */
    [[nodiscard]] const EFactory& event_factory() const { return m_event_factory; }

    /** @brief True if @c place_from or @c position_from was called. */
    [[nodiscard]] bool has_position_fn() const { return static_cast<bool>(m_placement_fn); }

    /** @brief True if any @c bind overload was called. */
    [[nodiscard]] bool has_bind() const { return m_bind_attach.has_value(); }

    /** @brief True if @c bind(attach, detach) was called. */
    [[nodiscard]] bool has_bind_detach() const { return m_bind_detach.has_value(); }

    /** @brief Name of the placement source, empty if anonymous. */
    [[nodiscard]] const std::string& position_fn_name() const { return m_position_call.first; }

    /** @brief The placement source as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& position_call() const { return m_position_call; }

    /** @brief Name of the active factory, empty if anonymous or none. */
    [[nodiscard]] const std::string& factory_name() const { return m_factory_call.first; }

    /** @brief The active factory as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& factory_call() const { return m_factory_call; }

    /** @brief Name of the bind attach function, empty if anonymous or none. */
    [[nodiscard]] const std::string& bind_attach_name() const { return m_bind_attach_call.first; }

    /** @brief The bind attach function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& bind_attach_call() const { return m_bind_attach_call; }

    /** @brief Name of the bind detach function, empty if anonymous or none. */
    [[nodiscard]] const std::string& bind_detach_name() const { return m_bind_detach_call.first; }

    /** @brief The bind detach function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& bind_detach_call() const { return m_bind_detach_call; }

    /** @brief The on_complete function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& complete_call() const { return m_complete_call; }

    /** @brief The on_detach function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& detach_call() const { return m_detach_call; }
};

} // namespace MayaFlux::Nexus
