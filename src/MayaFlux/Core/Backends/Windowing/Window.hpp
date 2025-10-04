#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Vruta {
class EventSource;
}

namespace MayaFlux::Core {

/**
 * @class Window
 * @brief Platform-agnostic window wrapper
 *
 * Wraps a window (provided via a backend) and provides a unified interface
 * for window management, event handling, and state tracking.
 */
class Window {
public:
    virtual ~Window() = default;

    /**
     * @brief Show the window
     */
    virtual void show() = 0;

    /**
     * @brief Hide the window
     */
    virtual void hide() = 0;

    /**
     * @brief Poll for window events (non-blocking)
     */
    [[nodiscard]] virtual bool should_close() const = 0;

    /**
     * @brief Get the current window state
     */
    [[nodiscard]] virtual const WindowState& get_state() const = 0;

    /*
     * @brief Get the window creation parameters
     */
    [[nodiscard]] virtual const WindowCreateInfo& get_create_info() const = 0;

    /**
     * @brief Set input configuration (keyboard, mouse, cursor)
     * @param config Input configuration settings
     */
    virtual void set_input_config(const InputConfig& config) = 0;

    /**
     * @brief Get current input configuration
     * @return Current input configuration settings
     */
    [[nodiscard]] virtual const InputConfig& get_input_config() const = 0;

    /**
     * @brief Set the callback function for window events
     * @param callback Function to be called on window events
     */
    virtual void set_event_callback(WindowEventCallback callback) = 0;

    /**
     * @brief Get native window handle (platform-specific)
     * @return Pointer to the native window handle
     */
    [[nodiscard]] virtual void* get_native_handle() const = 0;

    /**
     * @brief Get native display handle (platform-specific)
     * @return Pointer to the native display handle
     */
    [[nodiscard]] virtual void* get_native_display() const = 0;

    /**
     * @brief Set window title, size, or position
     * @param title New window title
     */
    virtual void set_title(const std::string& title) = 0;

    /**
     * @brief Resize the window
     * @param width New window width
     * @param height New window height
     */
    virtual void set_size(u_int32_t width, u_int32_t height) = 0;

    /**
     * @brief Move the window to a new position
     * @param x New X coordinate
     * @param y New Y coordinate
     */
    virtual void set_position(u_int32_t x, u_int32_t y) = 0;

    /**
     * @brief Gets the event source for awaiting events
     */
    [[nodiscard]] virtual Vruta::EventSource& get_event_source() = 0;
    [[nodiscard]] virtual const Vruta::EventSource& get_event_source() const = 0;
};
}
