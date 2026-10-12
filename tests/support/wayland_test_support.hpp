#pragma once

#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(WAYLAND_BACKEND)

#include <cstdlib>
#include <optional>
#include <string>

#include <wayland-client.h>

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

class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value)
        : m_name(name)
    {
        if (const char* previous = std::getenv(name)) {
            m_previous = previous;
        }
        if (value) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    ~ScopedEnv()
    {
        if (m_previous) {
            setenv(m_name.c_str(), m_previous->c_str(), 1);
        } else {
            unsetenv(m_name.c_str());
        }
    }

    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ScopedEnv(ScopedEnv&&) = delete;
    ScopedEnv& operator=(ScopedEnv&&) = delete;

private:
    std::string m_name;
    std::optional<std::string> m_previous;
};

}

#define REQUIRE_COMPOSITOR()                                   \
    do {                                                       \
        if (!MayaFlux::Test::compositor_available())           \
            GTEST_SKIP() << "No Wayland compositor reachable"; \
    } while (0)

#endif
