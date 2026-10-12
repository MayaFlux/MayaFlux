#include "gtest/gtest.h"

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Registry/Service/ComputeService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Test {

namespace {

    using Core::GlobalGraphicsConfig;
    using Core::VulkanBackend;

    bool buffer_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::BufferService>();
    }

    bool compute_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::ComputeService>();
    }

    bool display_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::DisplayService>();
    }

    bool initialize_or_fail(VulkanBackend& backend, const GlobalGraphicsConfig& config)
    {
        try {
            return backend.initialize(config);
        } catch (const std::exception&) {
            return false;
        }
    }

}

TEST(VulkanBackendLifecycleTest, ReportsVulkanBackendType)
{
    VulkanBackend backend;
    EXPECT_EQ(backend.get_backend_type(), GlobalGraphicsConfig::GraphicsApi::VULKAN);
}

TEST(VulkanBackendLifecycleTest, DestructionWithoutInitializeIsSafe)
{
    EXPECT_NO_THROW({ VulkanBackend backend; });
}

TEST(VulkanBackendLifecycleTest, CleanupWithoutInitializeIsNoOp)
{
    VulkanBackend backend;
    EXPECT_NO_THROW(backend.cleanup());
    EXPECT_FALSE(buffer_service_registered());
    EXPECT_FALSE(compute_service_registered());
    EXPECT_FALSE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, RejectsNonVulkanApiWithoutRegisteringServices)
{
    VulkanBackend backend;
    GlobalGraphicsConfig config = headless_graphics_config();
    config.requested_api = GlobalGraphicsConfig::GraphicsApi::OPENGL;

    EXPECT_FALSE(backend.initialize(config));
    EXPECT_FALSE(buffer_service_registered());
    EXPECT_FALSE(compute_service_registered());
    EXPECT_FALSE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, InitializeRegistersAllServices)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    EXPECT_TRUE(buffer_service_registered());
    EXPECT_TRUE(compute_service_registered());
    EXPECT_TRUE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, CleanupUnregistersAllServices)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    backend.cleanup();

    EXPECT_FALSE(buffer_service_registered());
    EXPECT_FALSE(compute_service_registered());
    EXPECT_FALSE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, DestructorUnregistersServices)
{
    REQUIRE_VULKAN();

    {
        VulkanBackend backend;
        ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    }

    EXPECT_FALSE(buffer_service_registered());
    EXPECT_FALSE(compute_service_registered());
    EXPECT_FALSE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, SecondInitializeSucceedsAndKeepsServices)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    auto* buffer_service = Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>();
    ASSERT_NE(buffer_service, nullptr);

    EXPECT_TRUE(backend.initialize(headless_graphics_config()));
    EXPECT_EQ(Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>(), buffer_service);
}

TEST(VulkanBackendLifecycleTest, CleanupIsIdempotent)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    backend.cleanup();
    EXPECT_NO_THROW(backend.cleanup());
}

TEST(VulkanBackendLifecycleTest, NativeContextIsTheBackendContext)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    EXPECT_EQ(backend.get_native_context(), static_cast<void*>(&backend.get_context()));
    EXPECT_EQ(static_cast<const VulkanBackend&>(backend).get_native_context(), backend.get_native_context());
}

TEST(VulkanBackendLifecycleTest, InitializedContextHasUsableDeviceAndQueues)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    auto& context = backend.get_context();
    EXPECT_TRUE(context.get_instance());
    EXPECT_TRUE(context.get_physical_device());
    EXPECT_TRUE(context.get_device());
    EXPECT_TRUE(context.get_graphics_queue());

    const auto& families = context.get_queue_families();
    EXPECT_TRUE(families.graphics_family.has_value());
    EXPECT_TRUE(families.compute_family.has_value());
}

TEST(VulkanBackendLifecycleTest, CommandManagerIsInitializedWithContext)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    auto cmd = backend.get_command_manager().allocate_command_buffer();
    EXPECT_TRUE(cmd);
    backend.get_command_manager().free_command_buffer(cmd);
}

TEST(VulkanBackendLifecycleTest, WaitIdleSucceedsAfterInitialize)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    EXPECT_NO_THROW(backend.wait_idle());
}

TEST(VulkanBackendLifecycleTest, UnsatisfiableRequiredExtensionLeavesNoServices)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    GlobalGraphicsConfig config = headless_graphics_config();
    config.backend_info.required_extensions.emplace_back("VK_MAYAFLUX_not_a_real_extension");

    EXPECT_FALSE(initialize_or_fail(backend, config));
    EXPECT_FALSE(buffer_service_registered());
    EXPECT_FALSE(compute_service_registered());
    EXPECT_FALSE(display_service_registered());
}

TEST(VulkanBackendLifecycleTest, UnsatisfiableOptionalExtensionIsSkipped)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    GlobalGraphicsConfig config = headless_graphics_config();
    config.backend_info.optional_extensions.emplace_back("VK_MAYAFLUX_not_a_real_extension");

    EXPECT_TRUE(backend.initialize(config));
}

TEST(VulkanBackendLifecycleTest, StrictSelectorWithUnknownDeviceNameDoesNotInitialize)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    GlobalGraphicsConfig config = headless_graphics_config();
    config.backend_info.device_name = "no such gpu exists";
    config.backend_info.strict_device_selection = true;

    EXPECT_FALSE(initialize_or_fail(backend, config));
    EXPECT_FALSE(buffer_service_registered());
}

TEST(VulkanBackendLifecycleTest, DeviceNameSelectorWithoutStrictFallsBackToScoring)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    GlobalGraphicsConfig config = headless_graphics_config();
    config.backend_info.device_name = "no such gpu exists";
    config.backend_info.strict_device_selection = false;

    EXPECT_TRUE(backend.initialize(config));
}

TEST(VulkanBackendLifecycleTest, RestartAfterCleanupInSameProcess)
{
    REQUIRE_VULKAN();

    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    backend.cleanup();

    ASSERT_TRUE(backend.initialize(headless_graphics_config()));
    EXPECT_TRUE(buffer_service_registered());
    EXPECT_NO_THROW(backend.wait_idle());
}

TEST(VulkanBackendLifecycleTest, SequentialBackendsInSameProcess)
{
    REQUIRE_VULKAN();

    for (int i = 0; i < 3; ++i) {
        VulkanBackend backend;
        ASSERT_TRUE(backend.initialize(headless_graphics_config())) << "iteration " << i;
        EXPECT_TRUE(buffer_service_registered());
    }
    EXPECT_FALSE(buffer_service_registered());
}

TEST(VulkanBackendRestartDeathTest, ServicesAllocateAfterRestart)
{
    REQUIRE_VULKAN();
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_EXIT(
        {
            VulkanBackend backend;
            if (!backend.initialize(headless_graphics_config())) {
                std::exit(2);
            }
            backend.cleanup();
            if (!backend.initialize(headless_graphics_config())) {
                std::exit(3);
            }

            auto* service = Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>();
            if (!service) {
                std::exit(4);
            }

            void* buffer = nullptr;
            void* memory = nullptr;
            void* mapped = nullptr;
            service->allocate_raw_buffer(64, 0x3U, 0x6U, true, buffer, memory, mapped);
            std::exit(mapped ? 0 : 5);
        },
        ::testing::ExitedWithCode(0), "");
}

}
