#pragma once

namespace MayaFlux {

namespace Core {
    struct GraphicsSurfaceInfo;
    class WindowManager;
    class Window;
    struct WindowCreateInfo;
}

/**
 * @brief Gets a handle to default window manager
 * @return Reference to the WindowManager
 */
Core::WindowManager& get_window_manager();

std::shared_ptr<Core::Window> create_window(const Core::WindowCreateInfo& create_info);

}
