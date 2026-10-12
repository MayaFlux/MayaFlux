#include "gtest/gtest.h"

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDescriptorManager.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::DescriptorSetLayoutConfig;
    using Core::DescriptorUpdateBatch;
    using Core::VKDescriptorManager;

    constexpr uint32_t k_pool_size = 4;

    class VKDescriptorManagerTest : public VulkanDeviceTest {
    protected:
        void SetUp() override
        {
            VulkanDeviceTest::SetUp();
            if (IsSkipped()) {
                return;
            }
            ready = manager.initialize(device(), k_pool_size);
        }

        void TearDown() override
        {
            if (ready) {
                manager.cleanup(device());
            }
        }

        static vk::Device device() { return backend().get_context().get_device(); }

        static DescriptorSetLayoutConfig storage_buffers(uint32_t count)
        {
            DescriptorSetLayoutConfig config;
            for (uint32_t i = 0; i < count; ++i) {
                config.add_storage_buffer(i);
            }
            return config;
        }

        static std::unique_ptr<RawVulkanBuffer> storage_buffer()
        {
            return std::make_unique<RawVulkanBuffer>(backend(), 256,
                vk::BufferUsageFlagBits::eStorageBuffer, true);
        }

        VKDescriptorManager manager;
        bool ready {};
    };

}

TEST(VKDescriptorManagerUninitializedTest, InitializeRejectsNullDevice)
{
    VKDescriptorManager manager;
    EXPECT_FALSE(manager.initialize(vk::Device {}, 4));
}

TEST_F(VKDescriptorManagerTest, InitializeCreatesOnePoolWithNothingAllocated)
{
    ASSERT_TRUE(ready);
    const auto [allocated, capacity] = manager.get_pool_stats();
    EXPECT_EQ(allocated, 0U);
    EXPECT_EQ(capacity, k_pool_size);
}

TEST_F(VKDescriptorManagerTest, IdenticalLayoutConfigsShareOneLayout)
{
    const auto first = manager.create_layout(device(), storage_buffers(2));
    const auto second = manager.create_layout(device(), storage_buffers(2));

    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);
}

TEST_F(VKDescriptorManagerTest, DifferentLayoutConfigsGetDifferentLayouts)
{
    const auto two = manager.create_layout(device(), storage_buffers(2));

    DescriptorSetLayoutConfig mixed;
    mixed.add_uniform_buffer(0);
    mixed.add_storage_buffer(1);
    const auto other = manager.create_layout(device(), mixed);

    ASSERT_TRUE(two);
    ASSERT_TRUE(other);
    EXPECT_NE(two, other);
}

TEST_F(VKDescriptorManagerTest, StageFlagsAndCountChangeTheCachedLayout)
{
    DescriptorSetLayoutConfig compute;
    compute.add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    DescriptorSetLayoutConfig fragment;
    fragment.add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment);
    DescriptorSetLayoutConfig array;
    array.add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 2);

    const auto a = manager.create_layout(device(), compute);
    const auto b = manager.create_layout(device(), fragment);
    const auto c = manager.create_layout(device(), array);

    EXPECT_NE(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(b, c);
}

TEST_F(VKDescriptorManagerTest, EmptyLayoutIsStillCreated)
{
    EXPECT_TRUE(manager.create_layout(device(), DescriptorSetLayoutConfig {}));
}

TEST_F(VKDescriptorManagerTest, AllocatesSetAndCountsIt)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto set = manager.allocate_set(device(), layout);

    EXPECT_TRUE(set);
    EXPECT_EQ(manager.get_pool_stats().first, 1U);
}

TEST_F(VKDescriptorManagerTest, AllocatingWithNullLayoutFails)
{
    EXPECT_FALSE(manager.allocate_set(device(), vk::DescriptorSetLayout {}));
    EXPECT_EQ(manager.get_pool_stats().first, 0U);
}

TEST_F(VKDescriptorManagerTest, ExhaustedPoolGrowsAndKeepsAllocating)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));

    for (uint32_t i = 0; i < k_pool_size + 1; ++i) {
        EXPECT_TRUE(manager.allocate_set(device(), layout)) << "set " << i;
    }

    const auto [allocated, capacity] = manager.get_pool_stats();
    EXPECT_EQ(allocated, k_pool_size + 1);
    EXPECT_EQ(capacity, k_pool_size * 2);
}

TEST_F(VKDescriptorManagerTest, AllocateSetsReturnsOneSetPerLayout)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto sets = manager.allocate_sets(device(), { layout, layout, layout });

    ASSERT_EQ(sets.size(), 3U);
    for (const auto& set : sets) {
        EXPECT_TRUE(set);
    }
    EXPECT_EQ(manager.get_pool_stats().first, 3U);
}

TEST_F(VKDescriptorManagerTest, AllocateSetsGrowsWhenBatchExceedsPool)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    ASSERT_EQ(manager.allocate_sets(device(), { layout, layout, layout }).size(), 3U);

    const auto sets = manager.allocate_sets(device(), { layout, layout });

    EXPECT_EQ(sets.size(), 2U);
    EXPECT_EQ(manager.get_pool_stats().second, k_pool_size * 2);
}

TEST_F(VKDescriptorManagerTest, AllocatingZeroSetsReturnsNothing)
{
    EXPECT_TRUE(manager.allocate_sets(device(), {}).empty());
}

TEST_F(VKDescriptorManagerTest, ResetPoolsReleasesEverySetAndKeepsCapacity)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    for (uint32_t i = 0; i < k_pool_size; ++i) {
        ASSERT_TRUE(manager.allocate_set(device(), layout));
    }

    manager.reset_pools(device());

    const auto [allocated, capacity] = manager.get_pool_stats();
    EXPECT_EQ(allocated, 0U);
    EXPECT_EQ(capacity, k_pool_size);
    for (uint32_t i = 0; i < k_pool_size; ++i) {
        EXPECT_TRUE(manager.allocate_set(device(), layout)) << "set " << i;
    }
    EXPECT_EQ(manager.get_pool_stats().second, k_pool_size);
}

TEST_F(VKDescriptorManagerTest, UpdateBufferAcceptsValidBinding)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto set = manager.allocate_set(device(), layout);
    auto buffer = storage_buffer();

    EXPECT_NO_THROW(manager.update_buffer(device(), set, 0, buffer->buffer(), 0, VK_WHOLE_SIZE));
}

TEST_F(VKDescriptorManagerTest, UpdatesWithNullHandlesAreRejectedWithoutThrowing)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto set = manager.allocate_set(device(), layout);
    auto buffer = storage_buffer();

    EXPECT_NO_THROW(manager.update_buffer(device(), vk::DescriptorSet {}, 0, buffer->buffer()));
    EXPECT_NO_THROW(manager.update_buffer(device(), set, 0, vk::Buffer {}));
    EXPECT_NO_THROW(manager.update_image(device(), set, 0, vk::ImageView {}, vk::Sampler {}, vk::ImageLayout::eGeneral));
    EXPECT_NO_THROW(manager.update_sampler(device(), set, 0, vk::Sampler {}));
    EXPECT_NO_THROW(manager.update_combined_image_sampler(device(), set, 0, vk::ImageView {}, vk::Sampler {}, vk::ImageLayout::eGeneral));
    EXPECT_NO_THROW(manager.update_input_attachment(device(), set, 0, vk::ImageView {}, vk::ImageLayout::eGeneral));
}

TEST_F(VKDescriptorManagerTest, CopyDescriptorSetRejectsNullSets)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto set = manager.allocate_set(device(), layout);

    EXPECT_NO_THROW(manager.copy_descriptor_set(device(), vk::DescriptorSet {}, set, 1));
    EXPECT_NO_THROW(manager.copy_descriptor_set(device(), set, vk::DescriptorSet {}, 1));
}

TEST_F(VKDescriptorManagerTest, CopyDescriptorSetCopiesAWrittenBinding)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto src = manager.allocate_set(device(), layout);
    const auto dst = manager.allocate_set(device(), layout);
    auto buffer = storage_buffer();
    manager.update_buffer(device(), src, 0, buffer->buffer());

    EXPECT_NO_THROW(manager.copy_descriptor_set(device(), src, dst, 1));
}

TEST_F(VKDescriptorManagerTest, BatchUpdateAppliesEveryWrite)
{
    const auto layout = manager.create_layout(device(), storage_buffers(2));
    const auto set = manager.allocate_set(device(), layout);
    auto first = storage_buffer();
    auto second = storage_buffer();

    vk::DescriptorBufferInfo first_info { first->buffer(), 0, VK_WHOLE_SIZE };
    vk::DescriptorBufferInfo second_info { second->buffer(), 0, VK_WHOLE_SIZE };
    vk::WriteDescriptorSet a { set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &first_info };
    vk::WriteDescriptorSet b { set, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &second_info };

    EXPECT_NO_THROW(manager.batch_update(device(), { a, b }));
    EXPECT_NO_THROW(manager.batch_update(device(), {}));
}

TEST_F(VKDescriptorManagerTest, UpdateBatchKeepsEveryBufferWriteValidAcrossManyBindings)
{
    constexpr uint32_t k_bindings = 9;
    const auto layout = manager.create_layout(device(), storage_buffers(k_bindings));
    const auto set = manager.allocate_set(device(), layout);

    std::vector<std::unique_ptr<RawVulkanBuffer>> buffers;
    DescriptorUpdateBatch batch(device(), set);
    for (uint32_t i = 0; i < k_bindings; ++i) {
        buffers.push_back(storage_buffer());
        batch.buffer(i, buffers.back()->buffer());
    }

    EXPECT_NO_THROW(batch.submit());
}

TEST_F(VKDescriptorManagerTest, SetWithSamplerBindingCanBeAllocated)
{
    DescriptorSetLayoutConfig config;
    config.add_binding(0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute);
    const auto layout = manager.create_layout(device(), config);
    ASSERT_TRUE(layout);

    EXPECT_TRUE(manager.allocate_set(device(), layout));
}

TEST_F(VKDescriptorManagerTest, EmptyUpdateBatchSubmitIsNoOp)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    const auto set = manager.allocate_set(device(), layout);

    DescriptorUpdateBatch batch(device(), set);
    EXPECT_NO_THROW(batch.submit());
}

TEST_F(VKDescriptorManagerTest, MoveTransfersPoolsAndLayouts)
{
    const auto layout = manager.create_layout(device(), storage_buffers(1));
    ASSERT_TRUE(layout);

    VKDescriptorManager moved(std::move(manager));
    EXPECT_EQ(moved.get_pool_stats().second, k_pool_size);
    EXPECT_EQ(moved.create_layout(device(), storage_buffers(1)), layout);
    EXPECT_TRUE(moved.allocate_set(device(), layout));

    moved.cleanup(device());
}

TEST_F(VKDescriptorManagerTest, CleanupIsSafeToRepeat)
{
    manager.cleanup(device());
    EXPECT_NO_THROW(manager.cleanup(device()));
    EXPECT_EQ(manager.get_pool_stats().second, 0U);
    EXPECT_FALSE(manager.allocate_set(device(), vk::DescriptorSetLayout {}));
}

}
