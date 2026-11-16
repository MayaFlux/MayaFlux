#pragma once

#include "Layer.hpp"

#include "MayaFlux/IO/Keys.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class EventManager;
}

namespace MayaFlux::Portal::Forma {

template <typename T>
struct MappedState;

/**
 * @class Context
 * @brief Event wiring between a Layer and a window surface.
 *
 * Subscribes to mouse events on a window by constructing Kriya input
 * coroutines and registering them with a Vruta::EventManager, following
 * the same pattern as Nexus::Fabric. The EventManager and Window are
 * injected; Context holds no engine-level singletons.
 *
 * Callbacks fire on the graphics thread, from window polling at the end of
 * each frame, after that frame's Bridge tasks and buffer processing have run.
 * They may write a MappedState directly, since it is the same thread, and the
 * value is read by the next frame. They must not touch GPU resources directly.
 *
 * All registered events are cancelled on destruction via
 * EventManager::cancel_event, keyed by names scoped to this Context.
 *
 * Window dimensions are read from window->get_state() at event time,
 * so resize is handled automatically.
 */
class MAYAFLUX_API Context {
public:
    using PressFn = std::function<void(uint32_t id, glm::vec2 ndc)>;
    using MoveFn = std::function<void(uint32_t id, glm::vec2 ndc)>;
    using EnterFn = std::function<void(uint32_t id)>;
    using LeaveFn = std::function<void(uint32_t id)>;
    using ScrollFn = std::function<void(uint32_t id, glm::vec2 ndc, double dx, double dy)>;
    using KeyFn = std::function<void(uint32_t id)>;
    using TextFn = std::function<void(uint32_t id, uint32_t codepoint)>;
    using ResizeFn = std::function<void(uint32_t width, uint32_t height)>;
    using CloseFn = std::function<void()>;

    /**
     * @brief Receives a window event that reached an element.
     *
     * ndc is the pointer in NDC for pointer events and zero for the others.
     */
    using SubscriberFn = std::function<void(uint32_t id, const Core::WindowEvent& event, glm::vec2 ndc)>;

    /**
     * @brief Construct and immediately register event coroutines.
     * @param layer         Layer to hit-test against. Must outlive Context.
     * @param window        Window whose surface the layer lives on.
     * @param event_manager Engine EventManager for coroutine registration.
     * @param name          Unique name scoping all registered event handlers.
     *                      Must be unique across all live Contexts.
     */
    Context(std::shared_ptr<Layer> layer,
        std::shared_ptr<Core::Window> window,
        Vruta::EventManager& event_manager,
        std::string name);

    /**
     * @brief Cancel all registered event coroutines.
     */
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    // =========================================================================
    // Per-element callbacks
    // =========================================================================

    /**
     * @brief Called when a mouse button is pressed over an element.
     * @param id  Element id to bind to.
     * @param btn Mouse button.
     * @param fn  Callback receiving element id and NDC press position.
     */
    void on_press(uint32_t id, IO::MouseButtons btn, PressFn fn);

    /**
     * @brief Called when a mouse button is released over an element.
     */
    void on_release(uint32_t id, IO::MouseButtons btn, PressFn fn);

    /**
     * @brief Called each move event while the cursor is over an element.
     */
    void on_move(uint32_t id, MoveFn fn);

    /**
     * @brief Called on each mouse-move event while @p btn is held, tracking
     *        the element where the drag began even when the cursor leaves its bounds.
     *
     * Drag tracking begins when the element is first hit (either at initial press
     * or first drag motion while button is held). Once tracking starts, the callback
     * continues to fire with the current cursor position until the button is released,
     * regardless of whether the cursor remains over the element.
     *
     * This matches standard UI expectations for sliders, scrollbars, resize handles,
     * and other controls that must respond continuously during a drag gesture.
     *
     * Driven by the same motion event as on_move, for every button the
     * window's event source reports as held.
     *
     * @param id  Element id to bind to.
     * @param btn Mouse button that must be held.
     * @param fn  Callback receiving element id and current NDC cursor position
     *            (which may be outside the element's bounds during drag).
     */
    void on_drag(uint32_t id, IO::MouseButtons btn, MoveFn fn);

    /**
     * @brief Called once when the cursor enters an element's region.
     */
    void on_enter(uint32_t id, EnterFn fn);

    /**
     * @brief Called once when the cursor leaves an element's region.
     */
    void on_leave(uint32_t id, LeaveFn fn);

    /**
     * @brief Called on scroll while the cursor is over an element.
     */
    void on_scroll(uint32_t id, ScrollFn fn);

    // =========================================================================
    // Additive subscriptions
    // =========================================================================

    /**
     * @brief Subscribe to the window events that reach an element, without
     *        replacing any of its own callbacks.
     *
     * The on_* callbacks keep one callback per element and slot, so a second
     * registration replaces the first. A subscription is added beside them:
     * the element's own callback runs first, then every matching subscriber in
     * the order they subscribed. unbind(id) removes the callbacks and every
     * subscriber of the element together.
     *
     * The filter is the one the window's event source uses, and an unset field
     * matches anything. Events reach an element as follows:
     * - MOUSE_BUTTON_PRESSED and MOUSE_BUTTON_RELEASED when the pointer is over
     *   it, with the button.
     * - MOUSE_MOTION while the pointer is over it. A filter with no button
     *   receives this hover motion. A filter with a button receives the drag of
     *   that button, which continues outside the element until the release and
     *   starts on press, as on_drag does.
     * - MOUSE_ENTERED and MOUSE_EXITED when the pointer enters or leaves it.
     * - MOUSE_SCROLLED while the pointer is over it.
     * - KEY_PRESSED, KEY_RELEASED and KEY_REPEAT of a key, while it has focus.
     *   A filter for these must name the key.
     * - TEXT_INPUT while it has focus.
     * - WINDOW_FOCUS_GAINED and WINDOW_FOCUS_LOST when it gains or loses focus.
     *
     * @param filter What to receive.
     * @param fn     Called for each matching event.
     * @return False, subscribing nothing, when receives() refuses the filter or
     *         fn is empty.
     */
    bool subscribe(uint32_t id, Vruta::WindowEventFilter filter, SubscriberFn fn);

    /**
     * @brief Whether an element can receive what a filter asks for.
     *
     * False for a key event without a key and for an event an element never
     * receives, such as a window resize.
     */
    [[nodiscard]] static bool receives(const Vruta::WindowEventFilter& filter) noexcept;

    /**
     * @brief Remove all callbacks registered for an element id.
     *
     * An element that has focus loses it first, with its focus-lost callback
     * and WINDOW_FOCUS_LOST subscriptions, as clear_focus() does.
     *
     * Also clears m_hovered/m_dragging entries pointing at @p id, so a
     * removed element cannot be left as the stale current hover or drag
     * target.
     */
    void unbind(uint32_t id);

    /**
     * @brief Called when a key is pressed while the element has focus.
     *
     * Focus is transferred on mouse press. The callback fires only once per
     * key press, even if the key is held.
     *
     * @param id  Element id to bind to.
     * @param key The key to listen for.
     * @param fn  Callback receiving element id.
     */
    void on_press(uint32_t id, IO::Keys key, KeyFn fn);

    /**
     * @brief Called when a key is released while the element has focus.
     *
     * @param id  Element id to bind to.
     * @param key The key to listen for.
     * @param fn  Callback receiving element id.
     */
    void on_release(uint32_t id, IO::Keys key, KeyFn fn);

    /**
     * @brief Called repeatedly while a key is held and the element has focus.
     *
     * Fires on initial press and continues on each repeat tick until the key
     * is released. Useful for continuous adjustments (arrow key nudging,
     * value increments) without requiring the user to repeatedly press.
     *
     * @param id  Element id to bind to.
     * @param key The key to listen for.
     * @param fn  Callback receiving element id, fired on press and each repeat.
     */
    void on_held(uint32_t id, IO::Keys key, KeyFn fn);

    /**
     * @brief Called with each resolved Unicode codepoint while the element
     *        has keyboard focus.
     *
     * Delivers composed text (shift, layout, dead-key/compose, IME already
     * resolved by the backend), distinct from on_press/on_held's raw keycodes.
     * Focus transfers on mouse press, same as the other keyboard callbacks.
     *
     * @param id  Element id to bind to.
     * @param fn  Callback receiving element id and the resolved codepoint.
     */
    void on_text(uint32_t id, TextFn fn);

    /**
     * @brief Called once when an element gains keyboard focus (via click
     *        or set_focus()).
     */
    void on_focus_gained(uint32_t id, EnterFn fn);

    /**
     * @brief Called once when an element loses keyboard focus.
     */
    void on_focus_lost(uint32_t id, LeaveFn fn);

    /**
     * @brief Clear keyboard focus (no element focused).
     *
     * The element that had focus gets its focus-lost callback and its
     * WINDOW_FOCUS_LOST subscriptions, as when a press moves focus away.
     */
    void clear_focus();

    /**
     * @brief Give keyboard focus to an element from code.
     *
     * Runs the same focus-lost and focus-gained callbacks, and the
     * WINDOW_FOCUS_LOST and WINDOW_FOCUS_GAINED subscriptions, as a press
     * that moves focus. Key and text callbacks then reach the element, so a
     * menu driven by keys or a controller works without the pointer.
     *
     * Refused for an id the Layer does not hold, or whose element is hidden
     * or not interactive. Focusing the element that already has focus does
     * nothing and returns true. Call it on the graphics thread, where the
     * other callbacks run.
     *
     * A focus callback or subscriber may itself move focus, here or on a
     * press, and the later move wins. An element whose focus-gained callback
     * has run is always told when it loses focus, by its callback and its
     * subscriptions. Its WINDOW_FOCUS_GAINED subscriptions are skipped if
     * focus moved on before they were reached.
     *
     * @param id Element to focus.
     * @return True if @p id has focus when the call returns: false if it was
     *         refused, or if a focus callback moved focus elsewhere.
     */
    bool set_focus(uint32_t id);

    /**
     * @brief Called on every window resize.
     *
     * Per-element like the other callbacks, so more than one bound id (e.g.
     * several pixel-anchored TextFields on the same Context) can each react
     * independently. Cleared by unbind(id) same as every other callback.
     *
     * Hit-test-driven placement (to_ndc() at event time) already tracks
     * resize for free; this is for callers anchored by a fixed pixel rect
     * via to_ndc_rect(), which must be recomputed explicitly when the
     * window's dimensions change.
     *
     * @param id Element id to bind to.
     * @param fn Callback receiving the new (width, height).
     */
    void on_resize(uint32_t id, ResizeFn fn);

    /**
     * @brief Run the resize callbacks if the window's size differs from the
     *        size they last saw.
     *
     * A window can reach its real size after the callbacks were registered,
     * for instance when the compositor answers the requested size with
     * another, and the resize event that carried it may predate them. Input
     * events call this first, so the callbacks catch up on the first pointer
     * or key event; call it directly to catch up sooner. A no-op when the
     * size has not changed.
     */
    void sync_size();

    /**
     * @brief Get currently focused element, if any.
     *
     * Inside a focus callback or subscriber this is already the element focus
     * is moving to, or none when it is being cleared.
     */
    [[nodiscard]] std::optional<uint32_t> focused() const { return m_focused; }

    /**
     * @brief Register a callback invoked when the Context window closes.
     * @param id Element id associated with the callback.
     * @param fn Callback invoked during close handling.
     */
    void on_close(uint32_t id, CloseFn fn);

    /**
     * @brief Attach key-delta handlers to a Mapped<float> element.
     *
     * Binds key handlers that adjust the element's state by a delta on each
     * press/hold. The element must be focused (via mouse click) for keys to fire.
     *
     * @param id          Element id.
     * @param state       MappedState to adjust.
     * @param decrease    Key that decreases the value.
     * @param increase    Key that increases the value.
     * @param delta       Step size per key event.
     * @param clamp_min   Minimum clamped value (default 0.0F).
     * @param clamp_max   Maximum clamped value (default 1.0F).
     * @return *this for chaining.
     */
    Context& key_step(
        uint32_t id,
        std::shared_ptr<MappedState<float>> state,
        IO::Keys decrease,
        IO::Keys increase,
        float delta,
        float clamp_min = 0.0F,
        float clamp_max = 1.0F);

    /**
     * @brief Register a key handler that fires on the initial press and on
     *        every repeat, with a deterministic first event.
     *
     * on_held already fires on the initial KEY_PRESSED and on each repeat, so
     * this registers it alone; also registering on_press would count the
     * initial press twice, since every listener on a key sees every event.
     * One fire per physical press, a clean repeat cadence after.
     *
     * @param id  Element id to bind to.
     * @param key The key to listen for.
     * @param fn  Callback receiving element id, fired once on press and
     *            again on each repeat tick.
     * @return *this for chaining.
     */
    Context& press_and_hold(uint32_t id, IO::Keys key, KeyFn fn);

    /**
     * @brief Register a drag handler receiving only the cursor position.
     *
     * Thin composition over on_drag for callers that already know the element
     * id and have no use for it in the callback. The sink performs whatever
     * mapping and write the gesture implies.
     *
     * @param id   Element id.
     * @param sink Called with the current NDC cursor position on each drag event.
     * @param btn  Mouse button that must be held.
     * @return *this for chaining.
     */
    Context& wire_drag(
        uint32_t id,
        std::function<void(glm::vec2)> sink,
        IO::MouseButtons btn = IO::MouseButtons::Left);

    // =========================================================================
    // State query
    // =========================================================================

    [[nodiscard]] std::optional<uint32_t> hovered() const { return m_hovered; }

    [[nodiscard]] const Vruta::WindowEventSource& event_source() const;

    /// @brief The Layer this Context hit-tests against.
    [[nodiscard]] const std::shared_ptr<Layer>& layer() const noexcept { return m_layer; }

    /**
     * @brief Rect of pixel size (@p w, @p h) at pixel position (@p x, @p y),
     *        top-left origin, converted to NDC from the window's current
     *        dimensions.
     *
     * Reads window state live at call time via to_ndc() - call again after
     * a resize for that size's new NDC bounds.
     */
    [[nodiscard]] Kinesis::AABB2D to_ndc_rect(double x, double y, double w, double h) const noexcept;

private:
    struct ElementCallbacks {
        std::unordered_map<int, PressFn> press;
        std::unordered_map<int, PressFn> release;
        std::unordered_map<int, MoveFn> drag;
        std::unordered_map<int, KeyFn> key_press;
        std::unordered_map<int, KeyFn> key_release;
        std::unordered_map<int, KeyFn> key_held;

        MoveFn move;
        EnterFn enter;
        LeaveFn leave;
        ScrollFn scroll;

        struct Subscription {
            Vruta::WindowEventFilter filter;
            SubscriberFn fn;
        };
        std::vector<Subscription> subscribers;

        EnterFn focus_gained;
        LeaveFn focus_lost;
        TextFn text;
        ResizeFn resize;
        CloseFn close;
    };

    struct KeyHandlerState {
        bool has_press = false;
        bool has_release = false;
        bool has_held = false;
    };
    std::unordered_map<int, KeyHandlerState> m_registered_keys;
    std::unordered_set<int> m_subscribed_keys;

    std::shared_ptr<Layer> m_layer;
    std::shared_ptr<Core::Window> m_window;
    Vruta::EventManager& m_event_manager;
    std::string m_name;
    std::optional<uint32_t> m_hovered;

    std::unordered_map<uint32_t, ElementCallbacks> m_callbacks;

    void register_handlers();
    void cancel_handlers(bool cancel_close = true);

    [[nodiscard]] glm::vec2 to_ndc(double px, double py) const noexcept;

    void handle_move(double px, double py);
    void handle_press(double px, double py, IO::MouseButtons btn);
    void move_focus(std::optional<uint32_t> target, glm::vec2 ndc);
    void handle_release(double px, double py, IO::MouseButtons btn);
    void handle_scroll(double dx, double dy);
    void handle_drag(double px, double py, IO::MouseButtons btn);
    void handle_drags(double px, double py);
    void dispatch(uint32_t id, Core::WindowEventType type, glm::vec2 ndc,
        Core::WindowEvent::EventData data = {},
        std::optional<IO::MouseButtons> button = std::nullopt,
        std::optional<IO::Keys> key = std::nullopt);
    void handle_subscribed_key(IO::Keys key, Core::WindowEventType type);
    void handle_key_press(IO::Keys key);
    void handle_key_release(IO::Keys key);
    void handle_key_held(IO::Keys key);
    void handle_text(uint32_t codepoint);
    void handle_resize(uint32_t width, uint32_t height);
    void handle_close();
    void detach_window(bool cancel_close = true);

    std::optional<uint32_t> m_dragging[3];
    std::optional<uint32_t> m_focused;

    /**
     * Focus notifications run user code that may move focus again, so
     * move_focus() tracks what it has told apart from where focus is.
     * m_focus_notified is the element last told it gained focus.
     * m_focus_pending is the subscriber half of the notification whose own
     * callback is running, which a nested move delivers or drops first.
     */
    struct FocusNotice {
        uint32_t id;
        Core::WindowEventType type;
        glm::vec2 ndc;
    };
    std::optional<uint32_t> m_focus_notified;
    std::optional<FocusNotice> m_focus_pending;

    uint32_t m_known_width {};
    uint32_t m_known_height {};
};

} // namespace MayaFlux::Portal::Forma
