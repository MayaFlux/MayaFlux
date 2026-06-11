#pragma once

#include "Layer.hpp"

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class EventManager;
class WindowEventSource;
}

namespace MayaFlux::Portal::Forma {

/**
 * @class Context
 * @brief Event wiring between a Layer and a window surface.
 *
 * Subscribes to mouse events on a window by constructing Kriya input
 * coroutines and registering them with a Vruta::EventManager, following
 * the same pattern as Nexus::Fabric. The EventManager and Window are
 * injected; Context holds no engine-level singletons.
 *
 * Callbacks fire on the scheduler tick. They must not touch GPU resources
 * directly. State intended for the graphics tick should be written through
 * shared_ptr captures and read on the next graphics cycle.
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
     * Backed by Kriya::mouse_dragged, which gates on button state natively.
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

    /**
     * @brief Remove all callbacks registered for an element id.
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
     * @brief Called once when an element gains keyboard focus (via click).
     */
    void on_focus_gained(uint32_t id, EnterFn fn);

    /**
     * @brief Called once when an element loses keyboard focus.
     */
    void on_focus_lost(uint32_t id, LeaveFn fn);

    /**
     * @brief Clear keyboard focus (no element focused).
     */
    void clear_focus();

    /**
     * @brief Get currently focused element, if any.
     */
    [[nodiscard]] std::optional<uint32_t> focused() const { return m_focused; }

    // =========================================================================
    // State query
    // =========================================================================

    [[nodiscard]] std::optional<uint32_t> hovered() const { return m_hovered; }

    [[nodiscard]] const Vruta::WindowEventSource& event_source() const;

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

        EnterFn focus_gained;
        LeaveFn focus_lost;
    };

    struct KeyHandlerState {
        bool has_press = false;
        bool has_release = false;
        bool has_held = false;
    };
    std::unordered_map<int, KeyHandlerState> m_registered_keys;

    std::shared_ptr<Layer> m_layer;
    std::shared_ptr<Core::Window> m_window;
    Vruta::EventManager& m_event_manager;
    std::string m_name;
    std::optional<uint32_t> m_hovered;

    std::unordered_map<uint32_t, ElementCallbacks> m_callbacks;

    void register_handlers();
    void cancel_handlers();

    [[nodiscard]] glm::vec2 to_ndc(double px, double py) const noexcept;

    void handle_move(double px, double py);
    void handle_press(double px, double py, IO::MouseButtons btn);
    void handle_release(double px, double py, IO::MouseButtons btn);
    void handle_scroll(double dx, double dy);
    void handle_drag(double px, double py, IO::MouseButtons btn);
    void handle_key_press(IO::Keys key);
    void handle_key_release(IO::Keys key);
    void handle_key_held(IO::Keys key);

    std::optional<uint32_t> m_dragging[3];
    std::optional<uint32_t> m_focused;
};

} // namespace MayaFlux::Portal::Forma
