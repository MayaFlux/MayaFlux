#pragma once

#include "gtest/gtest.h"

#include <string>

#include "MayaFlux/Core/Backends/Graphics/Vulkan/BackendResoureManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
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

class VulkanDeviceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!vulkan_unavailable_reason().empty()) {
            return;
        }
        s_backend = std::make_unique<Core::VulkanBackend>();
        if (!s_backend->initialize(headless_graphics_config())) {
            s_backend.reset();
        }
    }

    static void TearDownTestSuite()
    {
        s_backend.reset();
    }

    void SetUp() override
    {
        if (!s_backend) {
            GTEST_SKIP() << "No usable Vulkan device: " << vulkan_unavailable_reason();
        }
    }

    static Core::VulkanBackend& backend() { return *s_backend; }

private:
    static inline std::unique_ptr<Core::VulkanBackend> s_backend;
};

class RawVulkanBuffer {
public:
    RawVulkanBuffer(Core::VulkanBackend& backend, size_t size, vk::BufferUsageFlags usage, bool host_visible)
        : m_device(backend.get_context().get_device())
    {
        const auto properties = host_visible
            ? vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            : vk::MemoryPropertyFlagBits::eDeviceLocal;
        backend.get_resource_manager().allocate_raw_buffer(
            size, usage, properties, host_visible, m_buffer, m_memory, m_mapped);
    }

    ~RawVulkanBuffer()
    {
        if (m_mapped) {
            m_device.unmapMemory(m_memory);
        }
        m_device.destroyBuffer(m_buffer);
        m_device.freeMemory(m_memory);
    }

    RawVulkanBuffer(const RawVulkanBuffer&) = delete;
    RawVulkanBuffer& operator=(const RawVulkanBuffer&) = delete;
    RawVulkanBuffer(RawVulkanBuffer&&) = delete;
    RawVulkanBuffer& operator=(RawVulkanBuffer&&) = delete;

    [[nodiscard]] vk::Buffer buffer() const { return m_buffer; }
    [[nodiscard]] vk::DeviceMemory memory() const { return m_memory; }
    [[nodiscard]] void* mapped() const { return m_mapped; }

private:
    vk::Device m_device;
    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
    void* m_mapped {};
};

}

#define REQUIRE_VULKAN()                                                                  \
    do {                                                                                  \
        if (!MayaFlux::Test::vulkan_unavailable_reason().empty())                         \
            GTEST_SKIP() << "No usable Vulkan device: "                                   \
                         << MayaFlux::Test::vulkan_unavailable_reason();                  \
    } while (0)
