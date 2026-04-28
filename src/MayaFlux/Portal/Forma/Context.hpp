#pragma once

#include "Layer.hpp"

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class EventManager;
class EventSource;
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

    // =========================================================================
    // State query
    // =========================================================================

    [[nodiscard]] std::optional<uint32_t> hovered() const { return m_hovered; }

    [[nodiscard]] const Vruta::EventSource& event_source() const;

private:
    struct ElementCallbacks {
        std::unordered_map<int, PressFn> press;
        std::unordered_map<int, PressFn> release;
        MoveFn move;
        EnterFn enter;
        LeaveFn leave;
        ScrollFn scroll;
    };

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
};

} // namespace MayaFlux::Portal::Forma
