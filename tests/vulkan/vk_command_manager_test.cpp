#include "gtest/gtest.h"

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"

namespace MayaFlux::Test {
namespace {

    class VKCommandManagerTest : public VulkanDeviceTest {
    protected:
        static Core::VKContext& context() { return backend().get_context(); }
        static Core::VKCommandManager& commands() { return backend().get_command_manager(); }

        static uint32_t graphics_family() { return context().get_queue_families().graphics_family.value(); }
        static uint32_t compute_family() { return context().get_queue_families().compute_family.value(); }
    };

}

TEST_F(VKCommandManagerTest, AllocatesPrimaryAndSecondaryBuffers)
{
    auto primary = commands().allocate_command_buffer();
    auto secondary = commands().allocate_command_buffer(vk::CommandBufferLevel::eSecondary);

    EXPECT_TRUE(primary);
    EXPECT_TRUE(secondary);
    EXPECT_NE(static_cast<VkCommandBuffer>(primary), static_cast<VkCommandBuffer>(secondary));

    commands().free_command_buffer(primary);
    commands().free_command_buffer(secondary);
}

TEST_F(VKCommandManagerTest, FreeingNullOrUnknownBufferIsNoOp)
{
    EXPECT_NO_THROW(commands().free_command_buffer(vk::CommandBuffer {}));
}

TEST_F(VKCommandManagerTest, FreedBufferIsNotFreedTwice)
{
    auto cmd = commands().allocate_command_buffer();
    commands().free_command_buffer(cmd);
    EXPECT_NO_THROW(commands().free_command_buffer(cmd));
}

TEST_F(VKCommandManagerTest, SingleTimeGraphicsCommandsSubmitAndComplete)
{
    auto cmd = commands().begin_single_time_commands();
    ASSERT_TRUE(cmd);

    EXPECT_NO_THROW(commands().end_single_time_commands(cmd, context().get_graphics_queue()));
}

TEST_F(VKCommandManagerTest, SingleTimeComputeCommandsSubmitOnComputeQueue)
{
    auto cmd = commands().begin_single_time_commands_compute();
    ASSERT_TRUE(cmd);

    EXPECT_NO_THROW(commands().end_single_time_commands(cmd, context().get_compute_queue()));
}

TEST_F(VKCommandManagerTest, ResetPoolSucceedsWhenNothingIsPending)
{
    auto cmd = commands().allocate_command_buffer();
    commands().free_command_buffer(cmd);
    EXPECT_NO_THROW(commands().reset_pool());
}

TEST_F(VKCommandManagerTest, PoolIsExposedAfterInitialize)
{
    EXPECT_TRUE(commands().get_pool());
}

TEST_F(VKCommandManagerTest, DeferredCommandsStartEmpty)
{
    EXPECT_FALSE(commands().has_deferred_commands());
    EXPECT_FALSE(commands().get_deferred_cmd());
}

TEST_F(VKCommandManagerTest, RecordingDeferredCommandsMarksThemPendingAndReusesTheBuffer)
{
    int calls = 0;
    commands().record_deferred_commands([&calls](vk::CommandBuffer) { ++calls; });
    const auto first = commands().get_deferred_cmd();

    EXPECT_TRUE(commands().has_deferred_commands());
    ASSERT_TRUE(first);

    commands().record_deferred_commands([&calls](vk::CommandBuffer) { ++calls; });
    EXPECT_EQ(commands().get_deferred_cmd(), first);
    EXPECT_EQ(calls, 2);

    first.end();
    commands().reset_deferred();
}

TEST_F(VKCommandManagerTest, ResetDeferredClearsPendingState)
{
    commands().record_deferred_commands([](vk::CommandBuffer) { });
    commands().get_deferred_cmd().end();

    commands().reset_deferred();

    EXPECT_FALSE(commands().has_deferred_commands());
    EXPECT_FALSE(commands().get_deferred_cmd());
}

TEST_F(VKCommandManagerTest, StandaloneManagerOwnsItsPoolsUntilCleanup)
{
    Core::VKCommandManager manager;
    ASSERT_TRUE(manager.initialize(context().get_device(), graphics_family(), compute_family()));
    EXPECT_TRUE(manager.get_pool());

    auto cmd = manager.allocate_command_buffer();
    EXPECT_TRUE(cmd);

    manager.cleanup();
    EXPECT_FALSE(manager.get_pool());
}

TEST_F(VKCommandManagerTest, StandaloneCleanupIsIdempotent)
{
    Core::VKCommandManager manager;
    ASSERT_TRUE(manager.initialize(context().get_device(), graphics_family(), compute_family()));
    manager.cleanup();
    EXPECT_NO_THROW(manager.cleanup());
}

TEST_F(VKCommandManagerTest, StandaloneCleanupFreesBuffersStillAllocated)
{
    Core::VKCommandManager manager;
    ASSERT_TRUE(manager.initialize(context().get_device(), graphics_family(), compute_family()));
    (void)manager.allocate_command_buffer();
    (void)manager.begin_single_time_commands_compute();
    EXPECT_NO_THROW(manager.cleanup());
}

TEST(VKCommandManagerUninitializedTest, CleanupWithoutInitializeIsSafe)
{
    Core::VKCommandManager manager;
    EXPECT_NO_THROW(manager.cleanup());
    EXPECT_FALSE(manager.get_pool());
    EXPECT_FALSE(manager.has_deferred_commands());
}

}
