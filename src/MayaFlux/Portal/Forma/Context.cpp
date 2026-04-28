#include "Context.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kriya/InputEvents.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/EventSource.hpp"

namespace MayaFlux::Portal::Forma {

Context::Context(std::shared_ptr<Layer> layer,
    std::shared_ptr<Core::Window> window,
    Vruta::EventManager& event_manager,
    std::string name)
    : m_layer(std::move(layer))
    , m_window(std::move(window))
    , m_event_manager(event_manager)
    , m_name(std::move(name))
{
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

void Context::unbind(uint32_t id)
{
    m_callbacks.erase(id);
}

const Vruta::EventSource& Context::event_source() const
{
    return m_window->get_event_source();
}

// =============================================================================
// Handler registration / cancellation
// =============================================================================

void Context::register_handlers()
{
    m_event_manager.add_event(
        std::make_shared<Vruta::Event>(
            Kriya::mouse_moved(m_window,
                [this](double px, double py) { handle_move(px, py); })),
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
            Kriya::mouse_scrolled(m_window,
                [this](double dx, double dy) { handle_scroll(dx, dy); })),
        m_name + "_scroll");
}

void Context::cancel_handlers()
{
    for (const char* suffix : {
             "_move",
             "_press_left", "_release_left",
             "_press_right", "_release_right",
             "_scroll" }) {
        m_event_manager.cancel_event(m_name + suffix);
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

void Context::handle_move(double px, double py)
{
    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);

    if (hit != m_hovered) {
        if (m_hovered) {
            auto it = m_callbacks.find(*m_hovered);
            if (it != m_callbacks.end() && it->second.leave)
                it->second.leave(*m_hovered);
        }
        if (hit) {
            auto it = m_callbacks.find(*hit);
            if (it != m_callbacks.end() && it->second.enter)
                it->second.enter(*hit);
        }
        m_hovered = hit;
    }

    if (hit) {
        auto it = m_callbacks.find(*hit);
        if (it != m_callbacks.end() && it->second.move)
            it->second.move(*hit, ndc);
    }
}

void Context::handle_press(double px, double py, IO::MouseButtons btn)
{
    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);
    if (!hit)
        return;

    auto it = m_callbacks.find(*hit);
    if (it == m_callbacks.end())
        return;

    auto btn_it = it->second.press.find(static_cast<int>(btn));
    if (btn_it != it->second.press.end() && btn_it->second)
        btn_it->second(*hit, ndc);
}

void Context::handle_release(double px, double py, IO::MouseButtons btn)
{
    const glm::vec2 ndc = to_ndc(px, py);
    const auto hit = m_layer->hit_test(ndc);
    if (!hit)
        return;

    auto it = m_callbacks.find(*hit);
    if (it == m_callbacks.end())
        return;

    auto btn_it = it->second.release.find(static_cast<int>(btn));
    if (btn_it != it->second.release.end() && btn_it->second)
        btn_it->second(*hit, ndc);
}

void Context::handle_scroll(double dx, double dy)
{
    if (!m_hovered)
        return;

    auto it = m_callbacks.find(*m_hovered);
    if (it == m_callbacks.end() || !it->second.scroll)
        return;

    const auto [px, py] = m_window->get_event_source().get_mouse_position();
    it->second.scroll(*m_hovered, to_ndc(px, py), dx, dy);
}

} // namespace MayaFlux::Portal::Forma
