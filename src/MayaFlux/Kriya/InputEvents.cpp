#include "InputEvents.hpp"

#include "Awaiters/EventAwaiter.hpp"
#include "Awaiters/GetPromise.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/Event.hpp"

namespace MayaFlux::Kriya {

Vruta::Event key_pressed(
    std::shared_ptr<Core::Window> window,
    IO::Keys key,
    std::function<void()> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::KEY_PRESSED;
    filter.key_code = key;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        co_await EventAwaiter(source, filter);
        callback();
    }
}

Vruta::Event key_released(
    std::shared_ptr<Core::Window> window,
    IO::Keys key,
    std::function<void()> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::KEY_RELEASED;
    filter.key_code = key;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        co_await EventAwaiter(source, filter);
        callback();
    }
}

Vruta::Event any_key(
    std::shared_ptr<Core::Window> window,
    std::function<void(IO::Keys)> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::KEY_PRESSED;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto event = co_await EventAwaiter(source, filter);

        if (auto* key_data = std::get_if<Core::WindowEvent::KeyData>(&event.data)) {
            callback(static_cast<IO::Keys>(key_data->key));
        }
    }
}

Vruta::Event mouse_pressed(
    std::shared_ptr<Core::Window> window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::MOUSE_BUTTON_PRESSED;
    filter.button = button;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        co_await EventAwaiter(source, filter);

        auto [x, y] = source.get_mouse_position();
        callback(x, y);
    }
}

Vruta::Event mouse_released(
    std::shared_ptr<Core::Window> window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::MOUSE_BUTTON_RELEASED;
    filter.button = button;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        co_await EventAwaiter(source, filter);

        auto [x, y] = source.get_mouse_position();
        callback(x, y);
    }
}

Vruta::Event mouse_moved(
    std::shared_ptr<Core::Window> window,
    std::function<void(double, double)> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::MOUSE_MOTION;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto event = co_await EventAwaiter(source, filter);

        if (auto* pos_data = std::get_if<Core::WindowEvent::MousePosData>(&event.data)) {
            callback(pos_data->x, pos_data->y);
        }
    }
}

Vruta::Event mouse_scrolled(
    std::shared_ptr<Core::Window> window,
    std::function<void(double, double)> callback)
{
    auto& promise = co_await GetEventPromise {};
    auto& source = window->get_event_source();

    Vruta::EventFilter filter;
    filter.event_type = Core::WindowEventType::MOUSE_SCROLLED;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto event = co_await EventAwaiter(source, filter);

        if (auto* scroll_data = std::get_if<Core::WindowEvent::ScrollData>(&event.data)) {
            callback(scroll_data->x_offset, scroll_data->y_offset);
        }
    }
}

} // namespace MayaFlux::Kriya
