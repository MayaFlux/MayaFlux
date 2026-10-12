#pragma once

#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(WAYLAND_BACKEND)

#include <cstdlib>
#include <optional>
#include <string>

#include <wayland-client.h>

#include "scoped_env.hpp"

#include "MayaFlux/Core/Backends/Windowing/Wayland/WaylandWindow.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Test {

inline bool compositor_available()
{
    static const bool available = [] {
        wl_display* display = wl_display_connect(nullptr);
        if (!display) {
            return false;
        }
        wl_display_disconnect(display);
        return true;
    }();
    return available;
}

inline Core::WindowCreateInfo small_window(const std::string& title = "MayaFlux Wayland Test")
{
    Core::WindowCreateInfo info;
    info.title = title;
    info.width = 320;
    info.height = 240;
    return info;
}

inline void pump(Core::WaylandWindow& window, int iterations = 3)
{
    for (int i = 0; i < iterations; ++i) {
        window.poll();
    }
}

}

#define REQUIRE_COMPOSITOR()                                   \
    do {                                                       \
        if (!MayaFlux::Test::compositor_available())           \
            GTEST_SKIP() << "No Wayland compositor reachable"; \
    } while (0)

#endif
