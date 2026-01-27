#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Vruta {
class EventSource;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Core {

/**
 * @class Window
 * @brief Platform-agnostic window wrapper
 *
 * Wraps a window (provided via a backend) and provides a unified interface
 * for window management, event handling, and state tracking.
 */
class MAYAFLUX_API Window {
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
     * @brief Destroy the window and release resources
     */
    virtual void destroy() = 0;

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
    virtual void set_size(uint32_t width, uint32_t height) = 0;

    /**
     * @brief Move the window to a new position
     * @param x New X coordinate
     * @param y New Y coordinate
     */
    virtual void set_position(uint32_t x, uint32_t y) = 0;

    /**
     * @brief Set the clear color for the window
     * @param color RGBA color array
     */
    virtual void set_color(const std::array<float, 4>& color) = 0;

    /**
     * @brief Gets the event source for awaiting events
     */
    [[nodiscard]] virtual Vruta::EventSource& get_event_source() = 0;
    [[nodiscard]] virtual const Vruta::EventSource& get_event_source() const = 0;

    /**
     * @brief Check if window is registered with graphics subsystem
     */
    [[nodiscard]] virtual bool is_graphics_registered() const = 0;

    /**
     * @brief Mark window as registered/unregistered with graphics
     * Called by GraphicsSubsystem during register/unregister
     */
    virtual void set_graphics_registered(bool registered) = 0;

    /**
     * @brief Register a VKBuffer as rendering to this window
     * @param buffer Buffer that will render to this window
     *
     * Used for tracking and queries. Does not affect rendering directly.
     */
    virtual void register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) = 0;

    /**
     * @brief Unregister a VKBuffer from this window
     * @param buffer Buffer to unregister
     */
    virtual void unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) = 0;

    /**
     * @brief Track a secondary command buffer for this frame
     * @param cmd_id Command buffer ID that contains draw commands for this window
     *
     * Called by RenderProcessor after recording. PresentProcessor queries these
     * to know which secondary buffers to execute.
     */
    virtual void track_frame_command(uint64_t cmd_id) = 0;

    /**
     * @brief Get all command buffers recorded for this frame
     * @return Vector of command buffer IDs
     *
     * Called by PresentProcessor to collect secondary buffers for execution.
     */
    [[nodiscard]] virtual const std::vector<uint64_t>& get_frame_commands() const = 0;

    /**
     * @brief Clear tracked commands for this frame
     *
     * Called after presenting to reset for next frame.
     */
    virtual void clear_frame_commands() = 0;

    /**
     * @brief Get all VKBuffers currently rendering to this window
     * @return Vector of buffers (weak_ptr to avoid ownership issues)
     */
    [[nodiscard]] virtual std::vector<std::shared_ptr<Buffers::VKBuffer>> get_rendering_buffers() const = 0;
};
}
