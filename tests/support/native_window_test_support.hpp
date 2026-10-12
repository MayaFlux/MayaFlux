#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
#define MAYAFLUX_TEST_NATIVE_WINDOWS 1
#elif defined(MAYAFLUX_PLATFORM_MACOS) && defined(COCOA_BACKEND)
#define MAYAFLUX_TEST_NATIVE_WINDOWS 1
#include "cocoa_event_injector.hpp"
#elif defined(MAYAFLUX_PLATFORM_LINUX) && defined(WAYLAND_BACKEND)
#define MAYAFLUX_TEST_NATIVE_WINDOWS 1
#include "wayland_test_support.hpp"
#endif

#if defined(MAYAFLUX_TEST_NATIVE_WINDOWS)

namespace MayaFlux::Test {

inline bool native_display_available()
{
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    return true;
#elif defined(MAYAFLUX_PLATFORM_MACOS)
    return CocoaInput::display_available();
#else
    return compositor_available();
#endif
}

inline Core::GlobalGraphicsConfig presenting_graphics_config()
{
    return Core::GlobalGraphicsConfig {};
}

inline Core::WindowCreateInfo native_window_info(const std::string& title)
{
    Core::WindowCreateInfo info;
    info.title = title;
    info.width = 320;
    info.height = 240;
    return info;
}

}

#endif
