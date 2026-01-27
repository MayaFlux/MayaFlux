#pragma once

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux {
namespace Core {
    class Window;
}

namespace Vruta {
    class TaskScheduler;
    class Event;
    class EventSource;
}
namespace Kriya {

    /**
     * @brief Creates an Event coroutine that triggers on specific key press
     * @param window Window to listen to
     * @param key The key to wait for
     * @param callback Function to call on key press
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = on_key_pressed(window, IO::Keys::Escape,
     *     []() {
     *         // Handle Escape key press
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event key_pressed(
        std::shared_ptr<Core::Window> window,
        IO::Keys key,
        std::function<void()> callback);

    /**
     * @brief Creates an Event coroutine that triggers on specific key release
     * @param window Window to listen to
     * @param key The key to wait for
     * @param callback Function to call on key release
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = key_released(window, IO::Keys::Enter,
     *     []() {
     *         // Handle Enter key release
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event key_released(
        std::shared_ptr<Core::Window> window,
        IO::Keys key,
        std::function<void()> callback);

    /**
     * @brief Creates an Event coroutine that triggers on any key press
     * @param window Window to listen to
     * @param callback Function to call with key code when any key is pressed
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = any_key(window,
     *     [](IO::Keys key) {
     *         // Handle any key press, key code in 'key'
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event any_key(
        std::shared_ptr<Core::Window> window,
        std::function<void(IO::Keys)> callback);

    /**
     * @brief Creates an Event coroutine that triggers on specific mouse button press
     * @param window Window to listen to
     * @param button Mouse button to wait for
     * @param callback Function to call on button press
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = mouse_pressed(window, IO::MouseButtons::Left,
     *     [](double x, double y) {
     *         // Handle left mouse button press at (x, y)
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event mouse_pressed(
        std::shared_ptr<Core::Window> window,
        IO::MouseButtons button,
        std::function<void(double, double)> callback);

    /**
     * @brief Creates an Event coroutine that triggers on specific mouse button release
     * @param window Window to listen to
     * @param button Mouse button to wait for
     * @param callback Function to call on button release
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = mouse_released(window, IO::MouseButtons::Right,
     *     [](double x, double y) {
     *         // Handle right mouse button release at (x, y)
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event mouse_released(
        std::shared_ptr<Core::Window> window,
        IO::MouseButtons button,
        std::function<void(double, double)> callback);

    /**
     * @brief Creates an Event coroutine that triggers on mouse movement
     * @param window Window to listen to
     * @param callback Function to call on mouse move
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = mouse_moved(window,
     *     [](double x, double y) {
     *         // Handle mouse move at (x, y)
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event mouse_moved(
        std::shared_ptr<Core::Window> window,
        std::function<void(double, double)> callback);

    /**
     * @brief Creates an Event coroutine that triggers on mouse scroll
     * @param window Window to listen to
     * @param callback Function to call on scroll with ScrollData
     * @return Event coroutine that can be added to EventManager
     *
     * Example:
     * @code
     * auto task = mouse_scrolled(window,
     *     [](double xoffset, double yoffset) {
     *         // Handle mouse scroll with offsets
     *     });
     * @endcode
     */
    MAYAFLUX_API Vruta::Event mouse_scrolled(
        std::shared_ptr<Core::Window> window,
        std::function<void(double, double)> callback);

} // namespace Kriya
} // namespace MayaFlux
