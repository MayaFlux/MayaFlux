#pragma once

#include "gtest/gtest.h"

#include <string>

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Test {

inline Core::GlobalGraphicsConfig headless_graphics_config()
{
    Core::GlobalGraphicsConfig config;
    config.windowing_backend = Core::GlobalGraphicsConfig::WindowingBackend::NONE;
    config.backend_info.require_presentation = false;
    return config;
}

inline const std::string& vulkan_unavailable_reason()
{
    static const std::string reason = [] {
        try {
            Core::VulkanBackend backend;
            if (!backend.initialize(headless_graphics_config())) {
                return std::string("VulkanBackend::initialize returned false");
            }
            backend.cleanup();
            return std::string();
        } catch (const std::exception& e) {
            return std::string(e.what());
        }
    }();
    return reason;
}

}

#define REQUIRE_VULKAN()                                                                  \
    do {                                                                                  \
        if (!MayaFlux::Test::vulkan_unavailable_reason().empty())                         \
            GTEST_SKIP() << "No usable Vulkan device: "                                   \
                         << MayaFlux::Test::vulkan_unavailable_reason();                  \
    } while (0)
