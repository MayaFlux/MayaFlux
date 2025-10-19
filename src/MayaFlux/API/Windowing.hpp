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
MAYAFLUX_API Core::WindowManager& get_window_manager();

MAYAFLUX_API std::shared_ptr<Core::Window> create_window(const Core::WindowCreateInfo& create_info);

}
