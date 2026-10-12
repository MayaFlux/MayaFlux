#include "gtest/gtest.h"

#include <cstring>
#include <numeric>

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Test {
namespace {

    using Buffers::VKBuffer;
    using Core::BackendResourceManager;
    using Core::VKImage;
    using Registry::Service::BufferService;

    constexpr vk::BufferUsageFlags k_transfer_usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;

    std::vector<uint8_t> pattern(size_t size, uint8_t seed = 1)
    {
        std::vector<uint8_t> bytes(size);
        std::iota(bytes.begin(), bytes.end(), seed);
        return bytes;
    }

    void* as_handle(vk::Buffer buffer)
    {
        return static_cast<void*>(static_cast<VkBuffer>(buffer));
    }

    class BackendResourceManagerTest : public VulkanDeviceTest {
    protected:
        static BackendResourceManager& resources() { return backend().get_resource_manager(); }
        static BufferService* service() { return Registry::BackendRegistry::instance().get_service<BufferService>(); }

        static std::shared_ptr<VKBuffer> staging_buffer(size_t size = 256)
        {
            auto buffer = std::make_shared<VKBuffer>(size, VKBuffer::Usage::STAGING, Kakshya::DataModality::IMAGE_COLOR);
            resources().initialize_buffer(buffer);
            return buffer;
        }

        static std::shared_ptr<VKImage> make_image(
            uint32_t width, uint32_t height, vk::Format format,
            VKImage::Usage usage = VKImage::Usage::TEXTURE_2D)
        {
            auto image = std::make_shared<VKImage>(width, height, 1, format, usage);
            resources().initialize_image(image);
            return image;
        }
    };

    struct RoundTripCase {
        VKImage::Usage usage;
        vk::Format format;
    };

    class ImageRoundTripTest : public BackendResourceManagerTest,
                               public ::testing::WithParamInterface<RoundTripCase> { };

}

TEST_F(BackendResourceManagerTest, RawHostVisibleBufferIsPersistentlyMapped)
{
    RawVulkanBuffer raw(backend(), 128, k_transfer_usage, true);

    ASSERT_NE(raw.mapped(), nullptr);
    const auto bytes = pattern(128);
    std::memcpy(raw.mapped(), bytes.data(), bytes.size());
    EXPECT_EQ(std::memcmp(raw.mapped(), bytes.data(), bytes.size()), 0);
}

TEST_F(BackendResourceManagerTest, RawDeviceLocalBufferIsNotMapped)
{
    RawVulkanBuffer raw(backend(), 128, k_transfer_usage, false);

    EXPECT_TRUE(raw.buffer());
    EXPECT_EQ(raw.mapped(), nullptr);
}

TEST_F(BackendResourceManagerTest, InitializeBufferAllocatesAndMapsStagingBuffers)
{
    auto buffer = staging_buffer();

    EXPECT_TRUE(buffer->is_initialized());
    EXPECT_NE(buffer->get_mapped_ptr(), nullptr);
    EXPECT_TRUE(buffer->get_buffer());

    resources().cleanup_buffer(buffer);
}

TEST_F(BackendResourceManagerTest, InitializingAnInitializedBufferKeepsItsAllocation)
{
    auto buffer = staging_buffer();
    const auto handle = buffer->get_buffer();

    resources().initialize_buffer(buffer);

    EXPECT_EQ(buffer->get_buffer(), handle);
    resources().cleanup_buffer(buffer);
}

TEST_F(BackendResourceManagerTest, NullBuffersAreRejectedWithoutThrowing)
{
    EXPECT_NO_THROW(resources().initialize_buffer(nullptr));
    EXPECT_NO_THROW(resources().cleanup_buffer(nullptr));
}

TEST_F(BackendResourceManagerTest, CleaningUpAnUnmanagedBufferIsNoOp)
{
    auto buffer = std::make_shared<VKBuffer>(64, VKBuffer::Usage::STAGING, Kakshya::DataModality::IMAGE_COLOR);
    EXPECT_NO_THROW(resources().cleanup_buffer(buffer));
}

TEST_F(BackendResourceManagerTest, CleaningUpTwiceDoesNotFreeTwice)
{
    auto buffer = staging_buffer();
    resources().cleanup_buffer(buffer);
    EXPECT_NO_THROW(resources().cleanup_buffer(buffer));
}

TEST_F(BackendResourceManagerTest, DeviceAddressIsNonZeroForAddressableBuffers)
{
    auto buffer = std::make_shared<VKBuffer>(256, VKBuffer::Usage::STORAGE_BDA, Kakshya::DataModality::IMAGE_COLOR);
    resources().initialize_buffer(buffer);

    EXPECT_NE(resources().get_buffer_device_address(buffer), 0U);
    resources().cleanup_buffer(buffer);
}

TEST_F(BackendResourceManagerTest, DeviceAddressOfUninitializedBufferIsZero)
{
    auto buffer = std::make_shared<VKBuffer>(256, VKBuffer::Usage::STORAGE_BDA, Kakshya::DataModality::IMAGE_COLOR);
    EXPECT_EQ(resources().get_buffer_device_address(buffer), 0U);
    EXPECT_EQ(resources().get_buffer_device_address(nullptr), 0U);
}

TEST_F(BackendResourceManagerTest, FlushPendingOperationsConsumesDirtyRanges)
{
    auto buffer = staging_buffer();
    buffer->mark_dirty_range(0, 64);

    resources().flush_pending_buffer_operations();

    EXPECT_TRUE(buffer->get_and_clear_dirty_ranges().empty());
    resources().cleanup_buffer(buffer);
}

TEST_F(BackendResourceManagerTest, BufferServiceCopiesBytesBetweenBuffers)
{
    ASSERT_NE(service(), nullptr);
    RawVulkanBuffer src(backend(), 128, k_transfer_usage, true);
    RawVulkanBuffer dst(backend(), 128, k_transfer_usage, true);
    const auto bytes = pattern(128);
    std::memcpy(src.mapped(), bytes.data(), bytes.size());
    std::memset(dst.mapped(), 0, 128);

    service()->copy_buffer(as_handle(src.buffer()), as_handle(dst.buffer()), 128, 0, 0);

    EXPECT_EQ(std::memcmp(dst.mapped(), bytes.data(), bytes.size()), 0);
}

TEST_F(BackendResourceManagerTest, BufferServiceCopyHonoursOffsets)
{
    ASSERT_NE(service(), nullptr);
    RawVulkanBuffer src(backend(), 128, k_transfer_usage, true);
    RawVulkanBuffer dst(backend(), 128, k_transfer_usage, true);
    const auto bytes = pattern(128);
    std::memcpy(src.mapped(), bytes.data(), bytes.size());
    std::memset(dst.mapped(), 0, 128);

    service()->copy_buffer(as_handle(src.buffer()), as_handle(dst.buffer()), 16, 32, 64);

    EXPECT_EQ(std::memcmp(static_cast<uint8_t*>(dst.mapped()) + 64, bytes.data() + 32, 16), 0);
    EXPECT_EQ(static_cast<uint8_t*>(dst.mapped())[63], 0);
    EXPECT_EQ(static_cast<uint8_t*>(dst.mapped())[80], 0);
}

TEST_F(BackendResourceManagerTest, FencedCopyCompletesAfterWait)
{
    ASSERT_NE(service(), nullptr);
    RawVulkanBuffer src(backend(), 128, k_transfer_usage, true);
    RawVulkanBuffer dst(backend(), 128, k_transfer_usage, true);
    const auto bytes = pattern(128, 9);
    std::memcpy(src.mapped(), bytes.data(), bytes.size());
    std::memset(dst.mapped(), 0, 128);

    auto handle = service()->copy_buffer_fenced(as_handle(src.buffer()), as_handle(dst.buffer()), 128, 0, 0);
    ASSERT_NE(handle, nullptr);
    service()->wait_fenced(handle);
    service()->release_fenced(handle);

    EXPECT_EQ(std::memcmp(dst.mapped(), bytes.data(), bytes.size()), 0);
}

TEST_F(BackendResourceManagerTest, ExecuteFencedRunsTheRecorderOnce)
{
    ASSERT_NE(service(), nullptr);
    int calls = 0;

    auto handle = service()->execute_fenced([&calls](void* cmd) {
        EXPECT_NE(cmd, nullptr);
        ++calls;
    });

    ASSERT_NE(handle, nullptr);
    service()->wait_fenced(handle);
    service()->release_fenced(handle);
    EXPECT_EQ(calls, 1);
}

TEST_F(BackendResourceManagerTest, FencedHelpersIgnoreNullHandles)
{
    ASSERT_NE(service(), nullptr);
    EXPECT_NO_THROW(service()->wait_fenced(nullptr));
    EXPECT_NO_THROW(service()->release_fenced(nullptr));
}

TEST_F(BackendResourceManagerTest, ReleasingAFencedHandleTwiceIsSafe)
{
    ASSERT_NE(service(), nullptr);
    auto handle = service()->execute_fenced([](void*) { });
    ASSERT_NE(handle, nullptr);
    service()->wait_fenced(handle);
    service()->release_fenced(handle);

    EXPECT_NO_THROW(service()->release_fenced(handle));
}

TEST_F(BackendResourceManagerTest, ExecuteImmediateRunsTheRecorderOnce)
{
    ASSERT_NE(service(), nullptr);
    int calls = 0;
    service()->execute_immediate([&calls](void* cmd) {
        EXPECT_NE(cmd, nullptr);
        ++calls;
    });
    EXPECT_EQ(calls, 1);
}

TEST_F(BackendResourceManagerTest, FlushAndInvalidateAcceptWholeRangeOnCoherentMemory)
{
    ASSERT_NE(service(), nullptr);
    RawVulkanBuffer raw(backend(), 128, k_transfer_usage, true);
    void* memory = reinterpret_cast<void*>(static_cast<VkDeviceMemory>(raw.memory()));

    EXPECT_NO_THROW(service()->flush_range(memory, 0, 0));
    EXPECT_NO_THROW(service()->invalidate_range(memory, 0, 0));
    EXPECT_NO_THROW(service()->flush_range(memory, 0, 64));
}

TEST_F(BackendResourceManagerTest, DeferredCommandsFlushOnceThenReportNothingPending)
{
    int calls = 0;
    resources().record_deferred_commands([&calls](vk::CommandBuffer) { ++calls; });
    EXPECT_EQ(calls, 1);

    const auto semaphore = resources().flush_deferred_commands();
    EXPECT_TRUE(semaphore);
    backend().wait_idle();

    EXPECT_FALSE(resources().flush_deferred_commands());
}

TEST_F(BackendResourceManagerTest, FlushingWithoutDeferredCommandsReturnsNoSemaphore)
{
    EXPECT_FALSE(resources().flush_deferred_commands());
}

TEST_F(BackendResourceManagerTest, ExecuteImmediateCommandsRunsTheRecorder)
{
    int calls = 0;
    resources().execute_immediate_commands([&calls](vk::CommandBuffer cmd) {
        EXPECT_TRUE(cmd);
        ++calls;
    });
    EXPECT_EQ(calls, 1);
}

TEST_F(BackendResourceManagerTest, InitializeImageCreatesImageViewAndMemory)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);

    EXPECT_TRUE(image->is_initialized());
    EXPECT_TRUE(image->get_image());
    EXPECT_TRUE(image->get_image_view());
    EXPECT_TRUE(image->get_memory());

    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, InitializingAnInitializedImageKeepsItsHandle)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);
    const auto handle = image->get_image();

    resources().initialize_image(image);

    EXPECT_EQ(image->get_image(), handle);
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, CleanupImageReleasesHandlesAndIsRepeatable)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);

    resources().cleanup_image(image);

    EXPECT_FALSE(image->is_initialized());
    EXPECT_NO_THROW(resources().cleanup_image(image));
    EXPECT_NO_THROW(resources().cleanup_image(nullptr));
}

TEST_F(BackendResourceManagerTest, MipChainAndArrayImagesInitialize)
{
    auto image = std::make_shared<VKImage>(8, 8, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 4, 3);
    resources().initialize_image(image);

    EXPECT_TRUE(image->is_initialized());
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, VolumeImagesInitialize)
{
    auto image = std::make_shared<VKImage>(4, 4, 4, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::STORAGE,
        VKImage::Type::TYPE_3D);
    resources().initialize_image(image);

    EXPECT_TRUE(image->is_initialized());
    resources().cleanup_image(image);
}

TEST_P(ImageRoundTripTest, UploadedBytesReadBackUnchanged)
{
    const auto [usage, format] = GetParam();
    constexpr uint32_t k_extent = 4;
    const size_t size = static_cast<size_t>(k_extent) * k_extent * Core::vk_format_bytes_per_pixel(format);
    auto image = make_image(k_extent, k_extent, format, usage);
    const auto bytes = pattern(size, 3);

    resources().upload_image_data(image, bytes.data(), bytes.size());
    std::vector<uint8_t> out(size, 0);
    resources().download_image_data(image, out.data(), out.size());

    EXPECT_EQ(out, bytes);
    resources().cleanup_image(image);
}

INSTANTIATE_TEST_SUITE_P(UsagesAndFormats, ImageRoundTripTest,
    ::testing::Values(
        RoundTripCase { VKImage::Usage::TEXTURE_2D, vk::Format::eR8G8B8A8Unorm },
        RoundTripCase { VKImage::Usage::STORAGE, vk::Format::eR8G8B8A8Unorm },
        RoundTripCase { VKImage::Usage::RENDER_TARGET, vk::Format::eR8G8B8A8Unorm },
        RoundTripCase { VKImage::Usage::TEXTURE_2D, vk::Format::eR8Unorm },
        RoundTripCase { VKImage::Usage::TEXTURE_2D, vk::Format::eR32G32B32A32Sfloat }));

TEST_F(BackendResourceManagerTest, UploadWithNullArgumentsIsRejected)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);
    const auto bytes = pattern(64);

    EXPECT_NO_THROW(resources().upload_image_data(nullptr, bytes.data(), bytes.size()));
    EXPECT_NO_THROW(resources().upload_image_data(image, nullptr, bytes.size()));
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, UploadThroughProvidedStagingBufferRoundTrips)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);
    auto staging = staging_buffer(64);
    const auto bytes = pattern(64, 5);

    resources().upload_image_data(image, bytes.data(), bytes.size(), staging);
    std::vector<uint8_t> out(64, 0);
    resources().download_image_data(image, out.data(), out.size(), staging);

    EXPECT_EQ(out, bytes);
    resources().cleanup_buffer(staging);
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, LayerUploadReturnsTrueForAnArrayImage)
{
    auto image = std::make_shared<VKImage>(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 1, 2);
    resources().initialize_image(image);
    const auto bytes = pattern(64);

    EXPECT_TRUE(resources().upload_image_layer(image, 1, bytes.data(), bytes.size()));
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, TransitionsBetweenCommonLayoutsComplete)
{
    auto image = make_image(4, 4, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::STORAGE);
    const auto handle = image->get_image();

    EXPECT_NO_THROW(resources().transition_image_layout(handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal));
    EXPECT_NO_THROW(resources().transition_image_layout(handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal));
    EXPECT_NO_THROW(resources().transition_image_layout(handle, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral));
    EXPECT_NO_THROW(resources().transition_image_layout(handle, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal));
    EXPECT_NO_THROW(resources().transition_image_layout(handle, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal));
    resources().cleanup_image(image);
}

TEST_F(BackendResourceManagerTest, SameFormatBlitIsSupportedAndUndefinedIsNot)
{
    EXPECT_TRUE(resources().supports_blit(vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm));
    EXPECT_FALSE(resources().supports_blit(vk::Format::eUndefined, vk::Format::eUndefined));
}

TEST_F(BackendResourceManagerTest, FullExtentBlitCopiesPixels)
{
    auto src = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);
    auto dst = make_image(4, 4, vk::Format::eR8G8B8A8Unorm);
    const auto bytes = pattern(64, 7);
    resources().upload_image_data(src, bytes.data(), bytes.size());

    vk::ImageBlit region {};
    region.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    region.srcOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D { 0, 0, 0 }, vk::Offset3D { 4, 4, 1 } };
    region.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    region.dstOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D { 0, 0, 0 }, vk::Offset3D { 4, 4, 1 } };

    ASSERT_TRUE(resources().blit_image(src, dst, region, vk::Filter::eNearest));
    std::vector<uint8_t> out(64, 0);
    resources().download_image_data(dst, out.data(), out.size());

    EXPECT_EQ(out, bytes);
    resources().cleanup_image(src);
    resources().cleanup_image(dst);
}

TEST_F(BackendResourceManagerTest, SamplersWithTheSameParametersAreShared)
{
    const auto a = resources().create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat, 0.0F);
    const auto b = resources().create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat, 0.0F);

    ASSERT_TRUE(a);
    EXPECT_EQ(a, b);
}

TEST_F(BackendResourceManagerTest, SamplersDifferByFilterAddressModeAndAnisotropy)
{
    const auto base = resources().create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat, 0.0F);
    const auto nearest = resources().create_sampler(vk::Filter::eNearest, vk::SamplerAddressMode::eRepeat, 0.0F);
    const auto clamped = resources().create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eClampToEdge, 0.0F);
    const auto aniso = resources().create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat, 4.0F);

    EXPECT_NE(base, nearest);
    EXPECT_NE(base, clamped);
    EXPECT_NE(base, aniso);
}

TEST_F(BackendResourceManagerTest, DestroyedSamplerIsRecreatedOnNextRequest)
{
    const auto first = resources().create_sampler(vk::Filter::eNearest, vk::SamplerAddressMode::eMirroredRepeat, 0.0F);
    resources().destroy_sampler(first);

    const auto second = resources().create_sampler(vk::Filter::eNearest, vk::SamplerAddressMode::eMirroredRepeat, 0.0F);

    EXPECT_TRUE(second);
    resources().destroy_sampler(second);
}

TEST_F(BackendResourceManagerTest, DestroyingNullSamplerIsNoOp)
{
    EXPECT_NO_THROW(resources().destroy_sampler(vk::Sampler {}));
}

TEST_F(BackendResourceManagerTest, FindMemoryTypeHonoursRequestedProperties)
{
    const auto index = resources().find_memory_type(0xFFFFFFFFU,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    const auto properties = backend().get_context().get_physical_device().getMemoryProperties();
    ASSERT_LT(index, properties.memoryTypeCount);
    EXPECT_TRUE(properties.memoryTypes[index].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);
}

TEST_F(BackendResourceManagerTest, FindMemoryTypeThrowsWhenNothingMatches)
{
    EXPECT_THROW(resources().find_memory_type(0U, vk::MemoryPropertyFlagBits::eHostVisible), std::runtime_error);
}

TEST_F(BackendResourceManagerTest, StandaloneManagerCleanupReleasesItsBuffersAndSamplers)
{
    BackendResourceManager manager(backend().get_context(), backend().get_command_manager());
    auto buffer = std::make_shared<VKBuffer>(64, VKBuffer::Usage::STAGING, Kakshya::DataModality::IMAGE_COLOR);
    manager.initialize_buffer(buffer);
    ASSERT_TRUE(manager.create_sampler(vk::Filter::eLinear, vk::SamplerAddressMode::eClampToBorder, 0.0F));

    EXPECT_NO_THROW(manager.cleanup());
    EXPECT_NO_THROW(manager.cleanup());
}

}
