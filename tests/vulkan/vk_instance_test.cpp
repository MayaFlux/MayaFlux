#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKInstance.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::VKInstance;
    using Core::VulkanBackend;

    bool validation_layer_available()
    {
        const auto layers = vk::enumerateInstanceLayerProperties();
        return std::ranges::any_of(layers, [](const vk::LayerProperties& layer) {
            return std::strcmp(layer.layerName.data(), "VK_LAYER_KHRONOS_validation") == 0;
        });
    }

    bool try_initialize(VKInstance& instance, bool validation, const std::vector<const char*>& extensions = {})
    {
        try {
            return instance.initialize(validation, extensions);
        } catch (const std::exception&) {
            return false;
        }
    }

}

TEST(VKInstanceTest, InitializeWithoutValidationCreatesAnInstance)
{
    REQUIRE_VULKAN();
    VKInstance instance;

    ASSERT_TRUE(try_initialize(instance, false));

    EXPECT_TRUE(instance.get_instance());
}

TEST(VKInstanceTest, CleanupReleasesTheInstanceAndIsRepeatable)
{
    REQUIRE_VULKAN();
    VKInstance instance;
    ASSERT_TRUE(try_initialize(instance, false));

    instance.cleanup();

    EXPECT_FALSE(instance.get_instance());
    EXPECT_NO_THROW(instance.cleanup());
}

TEST(VKInstanceTest, CleanupWithoutInitializeIsSafe)
{
    VKInstance instance;
    EXPECT_FALSE(instance.get_instance());
    EXPECT_NO_THROW(instance.cleanup());
}

TEST(VKInstanceTest, ReinitializeAfterCleanupCreatesAFreshInstance)
{
    REQUIRE_VULKAN();
    VKInstance instance;
    ASSERT_TRUE(try_initialize(instance, false));
    instance.cleanup();

    ASSERT_TRUE(try_initialize(instance, false));
    EXPECT_TRUE(instance.get_instance());
}

TEST(VKInstanceTest, UnknownInstanceExtensionIsNotAccepted)
{
    REQUIRE_VULKAN();
    VKInstance instance;

    EXPECT_FALSE(try_initialize(instance, false, { "VK_MAYAFLUX_not_a_real_instance_extension" }));
    EXPECT_FALSE(instance.get_instance());
}

TEST(VKInstanceTest, ValidationRequestSucceedsExactlyWhenTheLayerIsInstalled)
{
    REQUIRE_VULKAN();
    VKInstance instance;
    const bool installed = validation_layer_available();

    EXPECT_EQ(try_initialize(instance, true), installed);
    EXPECT_EQ(static_cast<bool>(instance.get_instance()), installed);
}

TEST(VKInstanceTest, MovedInstanceKeepsTheHandleAndTheSourceLetsGo)
{
    REQUIRE_VULKAN();
    VKInstance source;
    ASSERT_TRUE(try_initialize(source, false));
    const auto handle = source.get_instance();

    VKInstance moved(std::move(source));

    EXPECT_EQ(moved.get_instance(), handle);
    EXPECT_FALSE(source.get_instance());
    EXPECT_NO_THROW(source.cleanup());
}

TEST(VKInstanceTest, MoveAssignmentReleasesTheTargetsInstanceFirst)
{
    REQUIRE_VULKAN();
    VKInstance source;
    VKInstance target;
    ASSERT_TRUE(try_initialize(source, false));
    ASSERT_TRUE(try_initialize(target, false));
    const auto handle = source.get_instance();

    target = std::move(source);

    EXPECT_EQ(target.get_instance(), handle);
    EXPECT_FALSE(source.get_instance());
}

TEST(VKInstanceTest, MovedValidationInstanceDestroysItsDebugMessengerCleanly)
{
    REQUIRE_VULKAN();
    if (!validation_layer_available()) {
        GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is not installed";
    }
    VKInstance source;
    ASSERT_TRUE(try_initialize(source, true));

    VKInstance moved(std::move(source));
    EXPECT_TRUE(moved.get_instance());
    EXPECT_NO_THROW(moved.cleanup());

    VKInstance first;
    VKInstance second;
    ASSERT_TRUE(try_initialize(first, true));
    second = std::move(first);
    EXPECT_NO_THROW(second.cleanup());
}

TEST(VKInstanceBackendTest, BackendWithValidationInitializesExactlyWhenTheLayerIsInstalled)
{
    REQUIRE_VULKAN();
    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.enable_validation = true;

    bool initialized = false;
    try {
        initialized = backend.initialize(config);
    } catch (const std::exception&) {
        initialized = false;
    }

    EXPECT_EQ(initialized, validation_layer_available());
    EXPECT_EQ(Registry::BackendRegistry::instance().has_service<Registry::Service::BufferService>(), initialized);
}

TEST(VKInstanceBackendTest, BackendWithoutValidationInitializesRegardlessOfTheLayer)
{
    REQUIRE_VULKAN();
    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.enable_validation = false;

    EXPECT_TRUE(backend.initialize(config));
}

}
