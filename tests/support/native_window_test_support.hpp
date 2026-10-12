#pragma once

#include "gtest/gtest.h"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"

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

class PresentingVulkanTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!native_display_available()) {
            s_reason = "No display session reachable";
            return;
        }
        const auto config = presenting_graphics_config();
        try {
            auto candidate = std::make_unique<Core::VulkanBackend>();
            if (!candidate->initialize(config)) {
                s_reason = "VulkanBackend::initialize returned false";
                return;
            }
            s_backend = std::move(candidate);
        } catch (const std::exception& e) {
            s_reason = e.what();
            return;
        }
        s_manager = std::make_unique<Core::WindowManager>(config);
    }

    static void TearDownTestSuite()
    {
        s_backend.reset();
        s_manager.reset();
    }

    void SetUp() override
    {
        if (!s_backend) {
            GTEST_SKIP() << "Presenting Vulkan backend unavailable: " << s_reason;
        }
    }

    void TearDown() override
    {
        if (!s_backend) {
            return;
        }
        for (auto& window : windows) {
            backend().unregister_window(window);
            manager().destroy_window(window);
        }
        windows.clear();
    }

    static Core::VulkanBackend& backend() { return *s_backend; }
    static Core::WindowManager& manager() { return *s_manager; }

    std::shared_ptr<Core::Window> make_window(std::array<float, 4> clear = { 0.0F, 0.0F, 0.0F, 1.0F })
    {
        static int counter = 0;
        auto info = native_window_info("presenting-window-" + std::to_string(++counter));
        info.clear_color = clear;
        return make_window(info);
    }

    std::shared_ptr<Core::Window> make_window(const Core::WindowCreateInfo& info)
    {
        auto window = manager().create_window(info);
        if (window) {
            windows.push_back(window);
        }
        return window;
    }

    std::vector<std::shared_ptr<Core::Window>> windows;

private:
    static inline std::unique_ptr<Core::VulkanBackend> s_backend;
    static inline std::unique_ptr<Core::WindowManager> s_manager;
    static inline std::string s_reason;
};

}

#endif
