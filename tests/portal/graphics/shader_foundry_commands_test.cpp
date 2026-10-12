#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Test {
namespace {

    using Portal::Graphics::DescriptorBufferWrite;
    using Portal::Graphics::DescriptorImageWrite;
    using Portal::Graphics::INVALID_COMMAND_BUFFER;
    using Portal::Graphics::INVALID_DESCRIPTOR_SET;
    using Portal::Graphics::INVALID_FENCE;
    using Portal::Graphics::INVALID_SEMAPHORE;
    using Portal::Graphics::ShaderFoundry;

    using CommandType = ShaderFoundry::CommandBufferType;

    constexpr uint32_t k_words = 16;
    constexpr size_t k_bytes = k_words * sizeof(uint32_t);
    constexpr uint32_t k_pattern = 0xDEADBEEF;

    ShaderFoundry& foundry()
    {
        return Portal::Graphics::get_shader_foundry();
    }

    class ShaderFoundryDeviceTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (!vulkan_unavailable_reason().empty()) {
                return;
            }
            auto backend = std::make_shared<Core::VulkanBackend>();
            if (backend->initialize(headless_graphics_config())) {
                s_backend = std::move(backend);
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
            foundry().shutdown();
            ASSERT_TRUE(foundry().initialize(s_backend));
        }

        void TearDown() override
        {
            foundry().shutdown();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

        static vk::Device device() { return s_backend->get_context().get_device(); }

        static std::unique_ptr<RawVulkanBuffer> host_buffer()
        {
            return std::make_unique<RawVulkanBuffer>(*s_backend, k_bytes,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc
                    | vk::BufferUsageFlagBits::eStorageBuffer,
                true);
        }

        static std::vector<uint32_t> words(const RawVulkanBuffer& buffer)
        {
            std::vector<uint32_t> out(k_words);
            std::memcpy(out.data(), buffer.mapped(), k_bytes);
            return out;
        }

        static void clear(const RawVulkanBuffer& buffer)
        {
            std::memset(buffer.mapped(), 0, k_bytes);
        }

        class StorageLayout {
        public:
            StorageLayout()
            {
                const vk::DescriptorSetLayoutBinding binding(
                    0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute);
                const vk::DescriptorSetLayoutCreateInfo info({}, 1, &binding);
                m_layout = device().createDescriptorSetLayout(info);
            }

            ~StorageLayout() { device().destroyDescriptorSetLayout(m_layout); }

            StorageLayout(const StorageLayout&) = delete;
            StorageLayout& operator=(const StorageLayout&) = delete;
            StorageLayout(StorageLayout&&) = delete;
            StorageLayout& operator=(StorageLayout&&) = delete;

            [[nodiscard]] vk::DescriptorSetLayout get() const { return m_layout; }

        private:
            vk::DescriptorSetLayout m_layout;
        };

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    // ------------------------------------------------------------------
    // Descriptors
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryDeviceTest, AllocatedDescriptorSetsHaveDistinctIdsAndHandles)
    {
        StorageLayout layout;

        const auto first = foundry().allocate_descriptor_set(layout.get());
        const auto second = foundry().allocate_descriptor_set(layout.get());

        EXPECT_NE(first, INVALID_DESCRIPTOR_SET);
        EXPECT_NE(second, INVALID_DESCRIPTOR_SET);
        EXPECT_NE(first, second);
        EXPECT_TRUE(foundry().get_descriptor_set(first));
        EXPECT_NE(foundry().get_descriptor_set(first), foundry().get_descriptor_set(second));
    }

    TEST_F(ShaderFoundryDeviceTest, UnknownDescriptorSetIdThrows)
    {
        EXPECT_THROW(static_cast<void>(foundry().get_descriptor_set(9999)), std::invalid_argument);
    }

    TEST_F(ShaderFoundryDeviceTest, BufferWritesToAKnownSetAreAcceptedAndUnknownSetsAreSkipped)
    {
        StorageLayout layout;
        const auto set = foundry().allocate_descriptor_set(layout.get());
        const auto buffer = host_buffer();

        EXPECT_NO_THROW(foundry().update_descriptor_buffer(
            set, 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes));
        EXPECT_NO_THROW(foundry().update_descriptor_buffer(
            9999, 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes));

        const std::vector<DescriptorBufferWrite> writes {
            { .descriptor_set_id = 9999, .binding = 0, .type = vk::DescriptorType::eStorageBuffer, .buffer = buffer->buffer(), .offset = 0, .size = k_bytes },
            { .descriptor_set_id = set, .binding = 0, .type = vk::DescriptorType::eStorageBuffer, .buffer = buffer->buffer(), .offset = 0, .size = k_bytes }
        };
        EXPECT_NO_THROW(foundry().update_descriptor_buffers(writes));
        EXPECT_NO_THROW(foundry().update_descriptor_buffers({}));
    }

    TEST_F(ShaderFoundryDeviceTest, ImageWritesToAnUnknownSetAreSkipped)
    {
        EXPECT_NO_THROW(foundry().update_descriptor_image(
            9999, 0, vk::ImageView {}, vk::Sampler {}, vk::ImageLayout::eShaderReadOnlyOptimal));
        EXPECT_NO_THROW(foundry().update_descriptor_storage_image(
            9999, 0, vk::ImageView {}, vk::ImageLayout::eGeneral));

        const std::vector<DescriptorImageWrite> writes {
            { .descriptor_set_id = 9999, .binding = 0, .type = vk::DescriptorType::eStorageImage, .image_view = vk::ImageView {}, .sampler = vk::Sampler {}, .layout = vk::ImageLayout::eGeneral }
        };
        EXPECT_NO_THROW(foundry().update_descriptor_images(writes));
        EXPECT_NO_THROW(foundry().update_descriptor_images({}));
    }

    TEST_F(ShaderFoundryDeviceTest, ShutdownForgetsEveryDescriptorSet)
    {
        StorageLayout layout;
        const auto set = foundry().allocate_descriptor_set(layout.get());
        ASSERT_TRUE(foundry().get_descriptor_set(set));

        foundry().shutdown();
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_THROW(static_cast<void>(foundry().get_descriptor_set(set)), std::invalid_argument);
    }

    // ------------------------------------------------------------------
    // Command buffers
    // ------------------------------------------------------------------

    struct TypeCase {
        std::string_view name;
        CommandType type;
    };

    class ShaderFoundryCommandTypeTest : public ShaderFoundryDeviceTest, public ::testing::WithParamInterface<TypeCase> { };

    TEST_P(ShaderFoundryCommandTypeTest, BeginCommandsGivesADistinctBufferThatCanBeSubmitted)
    {
        const auto first = foundry().begin_commands(GetParam().type);
        const auto second = foundry().begin_commands(GetParam().type);

        EXPECT_NE(first, INVALID_COMMAND_BUFFER);
        EXPECT_NE(second, INVALID_COMMAND_BUFFER);
        EXPECT_NE(first, second);
        EXPECT_TRUE(foundry().get_command_buffer(first));
        EXPECT_NE(foundry().get_command_buffer(first), foundry().get_command_buffer(second));

        foundry().submit_and_wait(first);
        foundry().submit_and_wait(second);
        EXPECT_FALSE(foundry().get_command_buffer(first));
    }

    INSTANTIATE_TEST_SUITE_P(Types, ShaderFoundryCommandTypeTest,
        ::testing::Values(
            TypeCase { "Graphics", CommandType::GRAPHICS },
            TypeCase { "Compute", CommandType::COMPUTE }),
        [](const ::testing::TestParamInfo<TypeCase>& info) { return std::string(info.param.name); });

    TEST_F(ShaderFoundryDeviceTest, EndCommandsSucceedsOnceAndFailsForUnknownOrFinishedBuffers)
    {
        const auto id = foundry().begin_commands(CommandType::COMPUTE);

        EXPECT_TRUE(foundry().end_commands(id));
        EXPECT_FALSE(foundry().end_commands(id));
        EXPECT_FALSE(foundry().end_commands(9999));
        EXPECT_FALSE(foundry().get_command_buffer(9999));
    }

    TEST_F(ShaderFoundryDeviceTest, SubmitAndWaitRunsTheRecordedWorkAndForgetsTheBuffer)
    {
        const auto buffer = host_buffer();
        clear(*buffer);
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        foundry().get_command_buffer(id).fillBuffer(buffer->buffer(), 0, k_bytes, k_pattern);

        foundry().submit_and_wait(id);

        EXPECT_EQ(words(*buffer), std::vector<uint32_t>(k_words, k_pattern));
        EXPECT_FALSE(foundry().get_command_buffer(id));
    }

    TEST_F(ShaderFoundryDeviceTest, SubmitAndWaitOfAnUnknownBufferIsIgnored)
    {
        EXPECT_NO_THROW(foundry().submit_and_wait(9999));
    }

    TEST_F(ShaderFoundryDeviceTest, AsyncSubmitSignalsItsFenceAndReleaseFreesTheBuffer)
    {
        const auto buffer = host_buffer();
        clear(*buffer);
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        foundry().get_command_buffer(id).fillBuffer(buffer->buffer(), 0, k_bytes, k_pattern);

        const auto fence = foundry().submit_async(id);
        ASSERT_NE(fence, INVALID_FENCE);
        foundry().wait_for_fence(fence);

        EXPECT_TRUE(foundry().is_fence_signaled(fence));
        EXPECT_EQ(words(*buffer), std::vector<uint32_t>(k_words, k_pattern));
        EXPECT_TRUE(foundry().get_command_buffer(id));

        foundry().release_fence(fence);

        EXPECT_FALSE(foundry().get_command_buffer(id));
        EXPECT_FALSE(foundry().is_fence_signaled(fence));
        EXPECT_NO_THROW(foundry().release_fence(fence));
    }

    TEST_F(ShaderFoundryDeviceTest, WaitForFencesWaitsForEveryFence)
    {
        const auto a = host_buffer();
        const auto b = host_buffer();
        clear(*a);
        clear(*b);
        const auto cmd_a = foundry().begin_commands(CommandType::COMPUTE);
        foundry().get_command_buffer(cmd_a).fillBuffer(a->buffer(), 0, k_bytes, 1U);
        const auto cmd_b = foundry().begin_commands(CommandType::COMPUTE);
        foundry().get_command_buffer(cmd_b).fillBuffer(b->buffer(), 0, k_bytes, 2U);
        const auto fence_a = foundry().submit_async(cmd_a);
        const auto fence_b = foundry().submit_async(cmd_b);
        ASSERT_NE(fence_a, INVALID_FENCE);
        ASSERT_NE(fence_b, INVALID_FENCE);

        foundry().wait_for_fences({ fence_a, fence_b });

        EXPECT_TRUE(foundry().is_fence_signaled(fence_a));
        EXPECT_TRUE(foundry().is_fence_signaled(fence_b));
        EXPECT_EQ(words(*a), std::vector<uint32_t>(k_words, 1U));
        EXPECT_EQ(words(*b), std::vector<uint32_t>(k_words, 2U));
        foundry().release_fence(fence_a);
        foundry().release_fence(fence_b);
    }

    TEST_F(ShaderFoundryDeviceTest, SubmittingAnUnknownBufferGivesNoFenceOrSemaphore)
    {
        EXPECT_EQ(foundry().submit_async(9999), INVALID_FENCE);
        EXPECT_EQ(foundry().submit_with_signal(9999), INVALID_SEMAPHORE);
        EXPECT_FALSE(foundry().is_fence_signaled(9999));
        EXPECT_NO_THROW(foundry().wait_for_fence(9999));
        EXPECT_NO_THROW(foundry().wait_for_fences({ 9999 }));
    }

    TEST_F(ShaderFoundryDeviceTest, SubmitWithSignalGivesASemaphoreWithAHandle)
    {
        const auto id = foundry().begin_commands(CommandType::COMPUTE);

        const auto semaphore = foundry().submit_with_signal(id);

        ASSERT_NE(semaphore, INVALID_SEMAPHORE);
        EXPECT_TRUE(foundry().get_semaphore_handle(semaphore));
        EXPECT_FALSE(foundry().get_semaphore_handle(9999));
        device().waitIdle();
    }

    TEST_F(ShaderFoundryDeviceTest, BeginCommandsWithWaitRefusesAnUnknownSemaphoreAndAcceptsAKnownOne)
    {
        const auto producer = foundry().begin_commands(CommandType::COMPUTE);
        const auto semaphore = foundry().submit_with_signal(producer);
        ASSERT_NE(semaphore, INVALID_SEMAPHORE);

        EXPECT_EQ(foundry().begin_commands_with_wait(CommandType::COMPUTE, 9999, vk::PipelineStageFlagBits::eTransfer),
            INVALID_COMMAND_BUFFER);

        const auto consumer = foundry().begin_commands_with_wait(
            CommandType::COMPUTE, semaphore, vk::PipelineStageFlagBits::eTransfer);
        EXPECT_NE(consumer, INVALID_COMMAND_BUFFER);
        foundry().submit_and_wait(consumer);
        device().waitIdle();
    }

    TEST_F(ShaderFoundryDeviceTest, EveryQueueKindHasAQueue)
    {
        EXPECT_TRUE(foundry().get_graphics_queue());
        EXPECT_TRUE(foundry().get_compute_queue());
        EXPECT_TRUE(foundry().get_transfer_queue());
    }

    TEST_F(ShaderFoundryDeviceTest, ShutdownFreesBuffersThatWereNeverSubmitted)
    {
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        ASSERT_TRUE(foundry().get_command_buffer(id));

        foundry().shutdown();
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_FALSE(foundry().get_command_buffer(id));
    }

    TEST_F(ShaderFoundryDeviceTest, StopFreesRecordingBuffersButKeepsTheFoundryInitialized)
    {
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        ASSERT_TRUE(foundry().get_command_buffer(id));

        foundry().stop();

        EXPECT_FALSE(foundry().get_command_buffer(id));
        EXPECT_TRUE(foundry().is_initialized());
    }

    // ------------------------------------------------------------------
    // Barriers and timestamps
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryDeviceTest, BufferBarrierOrdersAFillBeforeACopyOfTheSameBuffer)
    {
        const auto source = host_buffer();
        const auto target = host_buffer();
        clear(*source);
        clear(*target);
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        auto cmd = foundry().get_command_buffer(id);
        cmd.fillBuffer(source->buffer(), 0, k_bytes, k_pattern);

        foundry().buffer_barrier(id, source->buffer(),
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);

        const vk::BufferCopy region(0, 0, k_bytes);
        cmd.copyBuffer(source->buffer(), target->buffer(), region);
        foundry().submit_and_wait(id);

        EXPECT_EQ(words(*target), std::vector<uint32_t>(k_words, k_pattern));
    }

    TEST_F(ShaderFoundryDeviceTest, BarriersOnAnUnknownCommandBufferAreIgnored)
    {
        const auto buffer = host_buffer();

        EXPECT_NO_THROW(foundry().buffer_barrier(9999, buffer->buffer(),
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer));
        EXPECT_NO_THROW(foundry().image_barrier(9999, vk::Image {},
            vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
            vk::AccessFlags {}, vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader));
    }

    TEST_F(ShaderFoundryDeviceTest, TimestampsOfASubmittedBufferGiveAValidDuration)
    {
        const auto buffer = host_buffer();
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        foundry().begin_timestamp(id, "fill");
        foundry().get_command_buffer(id).fillBuffer(buffer->buffer(), 0, k_bytes, k_pattern);
        foundry().end_timestamp(id, "fill");

        const auto fence = foundry().submit_async(id);
        ASSERT_NE(fence, INVALID_FENCE);
        foundry().wait_for_fence(fence);
        const auto result = foundry().get_timestamp_result(id, "fill");

        EXPECT_TRUE(result.valid);
        EXPECT_EQ(result.label, "fill");
        foundry().release_fence(fence);
    }

    TEST_F(ShaderFoundryDeviceTest, TimestampsAreReadableAfterSubmitAndWait)
    {
        const auto buffer = host_buffer();
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        foundry().begin_timestamp(id, "fill");
        foundry().get_command_buffer(id).fillBuffer(buffer->buffer(), 0, k_bytes, k_pattern);
        foundry().end_timestamp(id, "fill");

        foundry().submit_and_wait(id);

        EXPECT_TRUE(foundry().get_timestamp_result(id, "fill").valid);
    }

    TEST_F(ShaderFoundryDeviceTest, TimestampResultsForUnknownBuffersOrLabelsAreInvalid)
    {
        const auto id = foundry().begin_commands(CommandType::COMPUTE);
        foundry().begin_timestamp(id, "known");

        EXPECT_FALSE(foundry().get_timestamp_result(9999, "known").valid);
        EXPECT_FALSE(foundry().get_timestamp_result(id, "other").valid);
        EXPECT_NO_THROW(foundry().begin_timestamp(9999, "x"));
        EXPECT_NO_THROW(foundry().end_timestamp(9999, "x"));
        EXPECT_NO_THROW(foundry().end_timestamp(id, "never_begun"));
        foundry().submit_and_wait(id);
    }

} // namespace
} // namespace MayaFlux::Test
