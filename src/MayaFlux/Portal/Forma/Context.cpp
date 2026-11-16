#include "Context.hpp"

#include "Primitives/Mapped.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"
#include "MayaFlux/Kriya/Awaiters/GetPromise.hpp"
#include "MayaFlux/Kriya/InputEvents.hpp"
#include "MayaFlux/Vruta/Event.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    template <typename Fn>
    Fn callback_for(const std::unordered_map<int, Fn>& callbacks, int button)
    {
        const auto found = callbacks.find(button);
        return found == callbacks.end() ? Fn {} : found->second;
    }

    /**
     * Runs an element's own callback. It is taken by value, so a callback that
     * subscribes, replaces a callback or unbinds the element does not
     * invalidate what is still being called.
     */
    template <typename Primary, typename... Args>
    void notify(Primary primary, Args... args)
    {
        if (primary)
            primary(args...);
    }

    bool matches(const Vruta::WindowEventFilter& filter, Core::WindowEventType type,
        std::optional<IO::MouseButtons> button, std::optional<IO::Keys> key)
    {
        const bool button_matches = type == Core::WindowEventType::MOUSE_MOTION
            ? filter.button == button
            : (!filter.button || filter.button == button);
        return (!filter.event_type || *filter.event_type == type)
            && button_matches
            && (!filter.key_code || filter.key_code == key);
    }

    Vruta::Event subscribed_key_events(std::shared_ptr<Core::Window> window, IO::Keys key,
        std::function<void(Core::WindowEventType)> callback)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        auto& source = window->get_event_source();

        Vruta::WindowEventFilter filter;
        filter.key_code = key;

        while (true) {
            if (promise.should_terminate)
                break;
            const auto event = co_await Kriya::WindowEventAwaiter(source, filter);
            if (promise.should_terminate)
                break;
            callback(event.type);
        }
    }

}

Context::Context(std::shared_ptr<Layer> layer,
    std::shared_ptr<Core::Window> window,
    Vruta::EventManager& event_manager,
    std::string name)
    : m_layer(std::move(layer))
    , m_window(std::move(window))
    , m_event_manager(event_manager)
    , m_name(std::move(name))
{
    if (m_window) {
        const auto& state = m_window->get_state();
        m_known_width = state.current_width;
        m_known_height = state.current_height;
    }
    register_handlers();
}

Context::~Context()
{
    cancel_handlers();
}

// =============================================================================
// Per-element callback registration
// =============================================================================

void Context::on_press(uint32_t id, IO::MouseButtons btn, PressFn fn)
{
    m_callbacks[id].press[static_cast<int>(btn)] = std::move(fn);
}

void Context::on_release(uint32_t id, IO::MouseButtons btn, PressFn fn)
{
    m_callbacks[id].release[static_cast<int>(btn)] = std::move(fn);
}

void Context::on_move(uint32_t id, MoveFn fn)
{
    m_callbacks[id].move = std::move(fn);
}

void Context::on_drag(uint32_t id, IO::MouseButtons btn, MoveFn fn)
{
    m_callbacks[id].drag[static_cast<int>(btn)] = std::move(fn);
}

void Context::on_enter(uint32_t id, EnterFn fn)
{
    m_callbacks[id].enter = std::move(fn);
}

void Context::on_leave(uint32_t id, LeaveFn fn)
{
    m_callbacks[id].leave = std::move(fn);
}

void Context::on_scroll(uint32_t id, ScrollFn fn)
{
    m_callbacks[id].scroll = std::move(fn);
}

bool Context::receives(const Vruta::WindowEventFilter& filter) noexcept
{
    using Type = Core::WindowEventType;

    if (!filter.event_type)
        return true;

    switch (*filter.event_type) {
    case Type::KEY_PRESSED:
    case Type::KEY_RELEASED:
    case Type::KEY_REPEAT:
        return filter.key_code.has_value();
    case Type::MOUSE_MOTION:
    case Type::MOUSE_BUTTON_PRESSED:
    case Type::MOUSE_BUTTON_RELEASED:
    case Type::MOUSE_SCROLLED:
    case Type::MOUSE_ENTERED:
    case Type::MOUSE_EXITED:
    case Type::TEXT_INPUT:
    case Type::WINDOW_FOCUS_GAINED:
    case Type::WINDOW_FOCUS_LOST:
        return true;
    default:
        return false;
    }
}

bool Context::subscribe(uint32_t id, Vruta::WindowEventFilter filter, SubscriberFn fn)
{
    if (!fn || !receives(filter))
        return false;

    if (filter.key_code) {
        const IO::Keys key = *filter.key_code;
        const int key_code = static_cast<int>(key);
        if (m_subscribed_keys.insert(key_code).second) {
            m_event_manager.add_event(
                std::make_shared<Vruta::Event>(
                    subscribed_key_events(m_window, key,
                        [this, key](Core::WindowEventType type) { handle_subscribed_key(key, type); })),
                m_name + "_subscribed_key_" + std::to_string(key_code));
        }
    }

    m_callbacks[id].subscribers.push_back({ .filter = filter, .fn = std::move(fn) });
    return true;
}

void Context::on_press(uint32_t id, IO::Keys key, KeyFn fn)
{
    const int key_code = static_cast<int>(key);
    m_callbacks[id].key_press[key_code] = std::move(fn);

    if (!m_registered_keys[key_code].has_press) {
        const std::string key_name = std::to_string(key_code);
        m_event_manager.add_event(
            std::make_shared<Vruta::Event>(
                Kriya::key_pressed(m_window, key,
                    [this, key]() { handle_key_press(key); })),
            m_name + "_key_press_" + key_name);
        m_registered_keys[key_code].has_press = true;
    }
}

void Context::on_release(uint32_t id, IO::Keys key, KeyFn fn)
{
    const int key_code = static_cast<int>(key);
    m_callbacks[id].key_release[key_code] = std::move(fn);

    if (!m_registered_keys[key_code].has_release) {
        const std::string key_name = std::to_string(key_code);
        m_event_manager.add_event(
            std::make_shared<Vruta::Event>(
                Kriya::key_released(m_window, key,
                    [this, key]() { handle_key_release(key); })),
            m_name + "_key_release_" + key_name);
        m_registered_keys[key_code].has_release = true;
    }
}

void Context::on_held(uint32_t id, IO::Keys key, KeyFn fn)
{
    const int key_code = static_cast<int>(key);
    m_callbacks[id].key_held[key_code] = std::move(fn);

    if (!m_registered_keys[key_code].has_held) {
        const std::string key_name = std::to_string(key_code);
        m_event_manager.add_event(
            std::make_shared<Vruta::Event>(
                Kriya::key_held(m_window, key,
                    [this, key]() { handle_key_held(key); })),
            m_name + "_key_held_" + key_name);
        m_registered_keys[key_code].has_held = true;
    }
}

void Context::on_text(uint32_t id, TextFn fn)
{
    m_callbacks[id].text = std::move(fn);
}

void Context::on_focus_gained(uint32_t id, EnterFn fn)
{
    m_callbacks[id].focus_gained = std::move(fn);
}

void Context::on_focus_lost(uint32_t id, LeaveFn fn)
{
    m_callbacks[id].focus_lost = std::move(fn);
}

void Context::on_resize(uint32_t id, ResizeFn fn)
{
    m_callbacks[id].resize = std::move(fn);
}

void Context::detach_window(bool cancel_close)
{
    cancel_handlers(cancel_close);
    m_window.reset();
    m_hovered.reset();
    m_focused.reset();
    m_focus_notified.reset();
    m_focus_pending.reset();
    for (auto& drag : m_dragging)
        drag.reset();
    m_registered_keys.clear();
    m_subscribed_keys.clear();
    m_callbacks.clear();
}

void Context::on_close(uint32_t id, CloseFn fn)
{
    m_callbacks[id].close = std::move(fn);
}

void Context::clear_focus()
{
    move_focus(std::nullopt, glm::vec2(0.0F));
}

bool Context::set_focus(uint32_t id)
{
    const Element* element = m_layer->get(id);
    if (!element || !element->visible || !element->interactive)
        return false;

    move_focus(id, glm::vec2(0.0F));
    return m_focused == id;
}

void Context::move_focus(std::optional<uint32_t> target, glm::vec2 ndc)
{
    using Type = Core::WindowEventType;

    m_focused = target;

    while (true) {
        if (const auto pending = std::exchange(m_focus_pending, std::nullopt)) {
            if (pending->type == Type::WINDOW_FOCUS_LOST || m_focused == pending->id)
                dispatch(pending->id, pending->type, pending->ndc);
            continue;
        }

        if (m_focus_notified == m_focused)
            return;

        if (m_focus_notified) {
            const uint32_t lost = *std::exchange(m_focus_notified, std::nullopt);
            m_focus_pending = FocusNotice { .id = lost, .type = Type::WINDOW_FOCUS_LOST, .ndc = ndc };
            const auto it = m_callbacks.find(lost);
            if (it != m_callbacks.end())
                notify(it->second.focus_lost, lost);
        } else {
            const uint32_t gained = *m_focused;
            m_focus_notified = gained;
            m_focus_pending = FocusNotice { .id = gained, .type = Type::WINDOW_FOCUS_GAINED, .ndc = ndc };
            const auto it = m_callbacks.find(gained);
            if (it != m_callbacks.end())
                notify(it->second.focus_gained, gained);
        }
    }
}

void Context::unbind(uint32_t id)
{
    if (m_focused == id)
        move_focus(std::nullopt, glm::vec2(0.0F));

    if (m_hovered && *m_hovered == id)
        m_hovered = std::nullopt;

    for (auto& dragging : m_dragging) {
        if (dragging && *dragging == id)
            dragging = std::nullopt;
    }

    m_callbacks.erase(id);
}

const Vruta::WindowEventSource& Context::event_source() const
{
    return m_window->get_event_source();
}

Context& Context::key_step(
    uint32_t id,
    std::shared_ptr<MappedState<float>> state,
    IO::Keys decrease,
    IO::Keys increase,
    float delta,
    float clamp_min,
    float clamp_max)
{
    on_held(id, decrease, [state, delta, clamp_min, clamp_max](uint32_t) {
        state->write(std::clamp(state->value - delta, clamp_min, clamp_max));
    });

    on_held(id, increase, [state, delta, clamp_min, clamp_max](uint32_t) {
        state->write(std::clamp(state->value + delta, clamp_min, clamp_max));
    });

    return *this;
}

Context& Context::press_and_hold(uint32_t id, IO::Keys key, KeyFn fn)
{
    on_held(id, key, std::move(fn));
    return *this;
}

Context& Context::wire_drag(
    uint32_t id,
    std::function<void(glm::vec2)> sink,
    IO::MouseButtons btn)
{
    if (!sink)
        return *this;

    on_drag(id, btn,
        [sink = std::move(sink)](uint32_t, glm::vec2 ndc) { sink(ndc); });

    return *this;
}

// =============================================================================
// Handler registration / cancellation
// =============================================================================

void Context::register_handlers()
{
    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_moved(m_window,
                [this](double px, double py) {
                    handle_move(px, py);
                    handle_drags(px, py);
                })),
        m_name + "_move");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_pressed(m_window, IO::MouseButtons::Left,
                [this](double px, double py) { handle_press(px, py, IO::MouseButtons::Left); })),
        m_name + "_press_left");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_released(m_window, IO::MouseButtons::Left,
                [this](double px, double py) { handle_release(px, py, IO::MouseButtons::Left); })),
        m_name + "_release_left");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_pressed(m_window, IO::MouseButtons::Right,
                [this](double px, double py) { handle_press(px, py, IO::MouseButtons::Right); })),
        m_name + "_press_right");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_released(m_window, IO::MouseButtons::Right,
                [this](double px, double py) { handle_release(px, py, IO::MouseButtons::Right); })),
        m_name + "_release_right");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_pressed(m_window, IO::MouseButtons::Middle,
                [this](double px, double py) { handle_press(px, py, IO::MouseButtons::Middle); })),
        m_name + "_press_middle");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_released(m_window, IO::MouseButtons::Middle,
                [this](double px, double py) { handle_release(px, py, IO::MouseButtons::Middle); })),
        m_name + "_release_middle");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_scrolled(m_window,
                [this](double dx, double dy) { handle_scroll(dx, dy); })),
        m_name + "_scroll");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::text_input(m_window,
                [this](uint32_t codepoint) { handle_text(codepoint); })),
        m_name + "_text");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::window_resized(m_window,
                [this](uint32_t w, uint32_t h) { handle_resize(w, h); })),
        m_name + "_resize");

    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::window_closed(m_window,
                [this]() { handle_close(); })),
        m_name + "_close");
}

void Context::cancel_handlers(bool cancel_close)
{
    for (const char* suffix : {
             "_move",
             "_press_left", "_release_left",
             "_press_right", "_release_right",
             "_press_middle", "_release_middle",
             "_scroll", "_text", "_resize" }) {
        m_event_manager.cancel_event(m_name + suffix);
    }

    if (cancel_close)
        m_event_manager.cancel_event(m_name + "_close");

    for (const int key_code : m_subscribed_keys)
        m_event_manager.cancel_event(m_name + "_subscribed_key_" + std::to_string(key_code));

    for (const auto& [key_code, handlers] : m_registered_keys) {
        const std::string key_name = std::to_string(key_code);
        if (handlers.has_press)
            m_event_manager.cancel_event(m_name + "_key_press_" + key_name);
        if (handlers.has_release)
            m_event_manager.cancel_event(m_name + "_key_release_" + key_name);
        if (handlers.has_held)
            m_event_manager.cancel_event(m_name + "_key_held_" + key_name);
    }
}

// =============================================================================
// Event dispatch
// =============================================================================

glm::vec2 Context::to_ndc(double px, double py) const noexcept
{
    const auto& s = m_window->get_state();
    return {
        (static_cast<float>(px) / static_cast<float>(s.current_width)) * 2.0F - 1.0F,
        1.0F - (static_cast<float>(py) / static_cast<float>(s.current_height)) * 2.0F
    };
}

Kinesis::AABB2D Context::to_ndc_rect(double x, double y, double w, double h) const noexcept
{
    return { .min = to_ndc(x, y + h), .max = to_ndc(x + w, y) };
}

void Context::handle_move(double px, double py)
{
    sync_size();
    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);

    if (hit != m_hovered) {
        if (m_hovered) {
            const uint32_t left = *m_hovered;
            const auto it = m_callbacks.find(left);
            if (it != m_callbacks.end())
                notify(it->second.leave, left);
            dispatch(left, Core::WindowEventType::MOUSE_EXITED, ndc);
        }
        if (hit) {
            const uint32_t entered = *hit;
            const auto it = m_callbacks.find(entered);
            if (it != m_callbacks.end())
                notify(it->second.enter, entered);
            dispatch(entered, Core::WindowEventType::MOUSE_ENTERED, ndc);
        }
        m_hovered = hit;
    }

    if (hit) {
        const uint32_t over = *hit;
        const auto it = m_callbacks.find(over);
        if (it != m_callbacks.end())
            notify(it->second.move, over, ndc);
        dispatch(over, Core::WindowEventType::MOUSE_MOTION, ndc,
            Core::WindowEvent::MousePosData { px, py });
    }
}

void Context::handle_press(double px, double py, IO::MouseButtons btn)
{
    sync_size();
    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);

    const int btn_idx = static_cast<int>(btn);

    move_focus(hit, ndc);

    if (hit) {
        const auto found = m_callbacks.find(*hit);
        if (found != m_callbacks.end()) {
            const auto drags = [&](const Vruta::WindowEventFilter& filter) {
                return filter.event_type == Core::WindowEventType::MOUSE_MOTION && filter.button == btn;
            };
            const bool dragged = found->second.drag.contains(btn_idx)
                || std::ranges::any_of(found->second.subscribers,
                    [&](const auto& subscription) { return drags(subscription.filter); });
            if (dragged)
                m_dragging[btn_idx] = hit;
        }
    }

    if (hit) {
        const uint32_t pressed = *hit;
        const auto it = m_callbacks.find(pressed);
        if (it != m_callbacks.end())
            notify(callback_for(it->second.press, btn_idx), pressed, ndc);
        dispatch(pressed, Core::WindowEventType::MOUSE_BUTTON_PRESSED, ndc,
            Core::WindowEvent::MouseButtonData { static_cast<int8_t>(btn), 0 }, btn);
    }
}

void Context::handle_release(double px, double py, IO::MouseButtons btn)
{
    const int btn_idx = static_cast<int>(btn);
    m_dragging[btn_idx] = std::nullopt;

    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);
    if (!hit)
        return;

    const uint32_t released = *hit;
    const auto it = m_callbacks.find(released);
    if (it != m_callbacks.end())
        notify(callback_for(it->second.release, btn_idx), released, ndc);
    dispatch(released, Core::WindowEventType::MOUSE_BUTTON_RELEASED, ndc,
        Core::WindowEvent::MouseButtonData { static_cast<int8_t>(btn), 0 }, btn);
}

void Context::handle_drags(double px, double py)
{
    const auto& source = m_window->get_event_source();
    for (const IO::MouseButtons btn : { IO::MouseButtons::Left, IO::MouseButtons::Right, IO::MouseButtons::Middle }) {
        if (source.is_mouse_pressed(static_cast<int>(btn)))
            handle_drag(px, py, btn);
    }
}

void Context::handle_drag(double px, double py, IO::MouseButtons btn)
{
    const glm::vec2 ndc = to_ndc(px, py);
    const int btn_idx = static_cast<int>(btn);

    if (!m_dragging[btn_idx]) {
        const auto hit = m_layer->hit_test(ndc);
        if (hit)
            m_dragging[btn_idx] = hit;
        else
            return;
    }

    const uint32_t target = *m_dragging[btn_idx];
    const auto it = m_callbacks.find(target);
    if (it == m_callbacks.end())
        return;

    notify(callback_for(it->second.drag, btn_idx), target, ndc);
    dispatch(target, Core::WindowEventType::MOUSE_MOTION, ndc,
        Core::WindowEvent::MousePosData { px, py }, btn);
}

void Context::handle_scroll(double dx, double dy)
{
    sync_size();
    if (!m_hovered)
        return;

    const uint32_t over = *m_hovered;
    const auto it = m_callbacks.find(over);
    if (it == m_callbacks.end())
        return;

    const auto [px, py] = m_window->get_event_source().get_mouse_position();
    const glm::vec2 ndc = to_ndc(px, py);
    notify(it->second.scroll, over, ndc, dx, dy);
    dispatch(over, Core::WindowEventType::MOUSE_SCROLLED, ndc,
        Core::WindowEvent::ScrollData { dx, dy });
}

// =============================================================================
// Keyboard event dispatch
// =============================================================================

void Context::handle_key_press(IO::Keys key)
{
    sync_size();
    if (!m_focused)
        return;

    auto it = m_callbacks.find(*m_focused);
    if (it == m_callbacks.end())
        return;

    const int key_code = static_cast<int>(key);
    auto key_it = it->second.key_press.find(key_code);
    if (key_it != it->second.key_press.end())
        key_it->second(*m_focused);
}

void Context::handle_key_release(IO::Keys key)
{
    if (!m_focused)
        return;

    auto it = m_callbacks.find(*m_focused);
    if (it == m_callbacks.end())
        return;

    const int key_code = static_cast<int>(key);
    auto key_it = it->second.key_release.find(key_code);
    if (key_it != it->second.key_release.end())
        key_it->second(*m_focused);
}

void Context::handle_key_held(IO::Keys key)
{
    if (!m_focused)
        return;

    auto it = m_callbacks.find(*m_focused);
    if (it == m_callbacks.end())
        return;

    const int key_code = static_cast<int>(key);
    auto key_it = it->second.key_held.find(key_code);
    if (key_it != it->second.key_held.end())
        key_it->second(*m_focused);
}

void Context::handle_text(uint32_t codepoint)
{
    sync_size();
    if (!m_focused)
        return;

    const uint32_t focused = *m_focused;
    auto it = m_callbacks.find(focused);
    if (it == m_callbacks.end())
        return;

    notify(it->second.text, focused, codepoint);
    dispatch(focused, Core::WindowEventType::TEXT_INPUT, glm::vec2(0.F),
        Core::WindowEvent::TextData { codepoint });
}

void Context::handle_subscribed_key(IO::Keys key, Core::WindowEventType type)
{
    sync_size();
    if (!m_focused)
        return;

    dispatch(*m_focused, type, glm::vec2(0.F),
        Core::WindowEvent::KeyData { static_cast<int16_t>(key), 0, 0 }, std::nullopt, key);
}

void Context::dispatch(uint32_t id, Core::WindowEventType type, glm::vec2 ndc,
    Core::WindowEvent::EventData data, std::optional<IO::MouseButtons> button,
    std::optional<IO::Keys> key)
{
    const auto found = m_callbacks.find(id);
    if (found == m_callbacks.end())
        return;

    const auto subscribers = found->second.subscribers;
    Core::WindowEvent event;
    event.type = type;
    event.data = std::move(data);
    for (const auto& subscription : subscribers) {
        if (matches(subscription.filter, type, button, key))
            subscription.fn(id, event, ndc);
    }
}

void Context::sync_size()
{
    if (!m_window) {
        return;
    }

    const auto& state = m_window->get_state();
    if (state.current_width != m_known_width || state.current_height != m_known_height) {
        handle_resize(state.current_width, state.current_height);
    }
}

void Context::handle_resize(uint32_t width, uint32_t height)
{
    m_known_width = width;
    m_known_height = height;
    for (auto& [id, callbacks] : m_callbacks) {
        if (callbacks.resize)
            callbacks.resize(width, height);
    }
}

void Context::handle_close()
{
    auto* event_source = &m_window->get_event_source();
    const std::string close_event_name = m_name + "_close";

    for (auto& [id, callbacks] : m_callbacks) {
        if (callbacks.close)
            callbacks.close();
    }
    clear_focus();
    detach_window(false);
    event_source->defer([event_manager = &m_event_manager,
                            name = std::move(close_event_name)]() {
        event_manager->cancel_event(name);
    });
}

} // namespace MayaFlux::Portal::Forma
