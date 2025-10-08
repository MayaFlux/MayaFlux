#pragma once

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Core {

/**
 * @enum GraphicsBackendType
 * @brief Enumeration of supported graphics backend types
 */
enum class GraphicsBackendType {
    VULKAN,
    OPENGL,
};

class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    /**
     * @brief Initialize the graphics backend with global configuration
     * @param config Global graphics configuration
     * @return True if initialization was successful, false otherwise
     */
    virtual bool initialize(const GlobalGraphicsConfig& config) = 0;

    /**
     * @brief Cleanup the graphics backend and release all resources
     */
    virtual void cleanup() = 0;

    /**
     * @brief Get the type of the graphics backend
     * @return GraphicsBackendType enum value representing the backend type
     */
    virtual GraphicsBackendType get_backend_type() = 0;

    /**
     * @brief Register a window with the graphics backend for rendering
     * @param window Shared pointer to the window to register
     * @return True if registration was successful, false otherwise
     */
    virtual bool register_window(std::shared_ptr<Window> window) = 0;

    /**
     * @brief Unregister a window from the graphics backend
     * @param window Shared pointer to the window to unregister
     */
    virtual void unregister_window(std::shared_ptr<Window> window) = 0;

    /**
     * @brief Check if a window is registered with the graphics backend
     * @param window Shared pointer to the window to check
     * @return True if the window is registered, false otherwise
     */
    [[nodiscard]]
    virtual bool is_window_registered(std::shared_ptr<Window> window)
        = 0;

    /**
     * @brief Begin rendering frame for the specified window
     * @param window Shared pointer to the window to begin frame for
     */
    virtual void begin_frame(std::shared_ptr<Window> window) = 0;

    /**
     * @brief Render the contents of the specified window
     * @param window Shared pointer to the window to render
     */
    virtual void render_window(std::shared_ptr<Window> window) = 0;

    /**
     * @brief Render all registered windows (batch optimization)
     * Default: calls render_window() for each registered window
     */
    virtual void render_all_windows() = 0;

    /**
     * @brief End rendering frame for the specified window
     * @param window Shared pointer to the window to end frame for
     */
    virtual void end_frame(std::shared_ptr<Window> window) = 0;

    /**
     * @brief Wait until the graphics backend is idle
     */
    virtual void wait_idle() = 0;

    /**
     * @brief Handle window resize event for the specified window
     */
    virtual void handle_window_resize() = 0;

    /**
     * @brief Get context pointer specific to the graphics backend (e.g., OpenGL context, Vulkan instance, etc.)
     */
    virtual void* get_native_context() = 0;
    [[nodiscard]] virtual const void* get_native_context() const = 0;
};

}
