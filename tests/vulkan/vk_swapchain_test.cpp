#include "gtest/gtest.h"

#include "../support/native_window_test_support.hpp"

#if defined(MAYAFLUX_TEST_NATIVE_WINDOWS)

#include <algorithm>
#include <limits>

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKSwapchain.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::VKSwapchain;
    using Core::Window;

    class VKSwapchainFixture : public PresentingVulkanTest {
    protected:
        void TearDown() override
        {
            swapchains.clear();
            for (auto surface : surfaces) {
                context().destroy_surface(surface);
            }
            surfaces.clear();
            PresentingVulkanTest::TearDown();
        }

        static Core::VKContext& context() { return backend().get_context(); }

        vk::SurfaceKHR surface_for(const std::shared_ptr<Window>& window)
        {
            const auto surface = context().create_surface(window);
            if (surface) {
                surfaces.push_back(surface);
            }
            return surface;
        }

        VKSwapchain* create_swapchain(const std::shared_ptr<Window>& window, const std::vector<uint32_t>& sharing = {})
        {
            const auto surface = surface_for(window);
            if (!surface) {
                return nullptr;
            }
            auto swapchain = std::make_unique<VKSwapchain>();
            if (!swapchain->create(context(), surface, window->get_create_info(), sharing)) {
                return nullptr;
            }
            swapchains.push_back(std::move(swapchain));
            return swapchains.back().get();
        }

        vk::Extent2D expected_extent(vk::SurfaceKHR surface, const std::shared_ptr<Window>& window)
        {
            const auto caps = context().get_physical_device().getSurfaceCapabilitiesKHR(surface);
            if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return caps.currentExtent;
            }
            const auto& info = window->get_create_info();
            return { std::clamp(info.width, caps.minImageExtent.width, caps.maxImageExtent.width),
                std::clamp(info.height, caps.minImageExtent.height, caps.maxImageExtent.height) };
        }

        std::optional<uint32_t> present_one(VKSwapchain& swapchain)
        {
            auto device = context().get_device();
            auto& commands = backend().get_command_manager();
            const vk::Semaphore acquired = device.createSemaphore({});
            const vk::Semaphore finished = device.createSemaphore({});

            std::optional<uint32_t> presented;
            if (auto index = swapchain.acquire_next_image(acquired); index.has_value()) {
                auto cmd = commands.allocate_command_buffer();
                cmd.begin(vk::CommandBufferBeginInfo { vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                vk::ImageMemoryBarrier barrier {};
                barrier.oldLayout = vk::ImageLayout::eUndefined;
                barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = swapchain.get_images()[*index];
                barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
                cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);
                cmd.end();

                const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                vk::SubmitInfo submit {};
                submit.waitSemaphoreCount = 1;
                submit.pWaitSemaphores = &acquired;
                submit.pWaitDstStageMask = &wait_stage;
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &cmd;
                submit.signalSemaphoreCount = 1;
                submit.pSignalSemaphores = &finished;
                (void)context().get_graphics_queue().submit(1, &submit, nullptr);

                if (swapchain.present(*index, finished, context().get_graphics_queue())) {
                    presented = *index;
                }
                device.waitIdle();
                commands.free_command_buffer(cmd);
            }

            device.waitIdle();
            device.destroySemaphore(acquired);
            device.destroySemaphore(finished);
            return presented;
        }

        std::vector<vk::SurfaceKHR> surfaces;
        std::vector<std::unique_ptr<VKSwapchain>> swapchains;
    };

    class VKSwapchainPresentModeTest : public VKSwapchainFixture,
                                       public ::testing::WithParamInterface<Core::GraphicsSurfaceInfo::PresentMode> { };

    class VKSwapchainSurfaceFormatTest : public VKSwapchainFixture,
                                         public ::testing::WithParamInterface<Core::GraphicsSurfaceInfo::SurfaceFormat> { };

}

TEST_F(VKSwapchainFixture, SupportQueryReportsFormatsAndTheMandatoryFifoMode)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    const auto surface = surface_for(window);
    ASSERT_TRUE(surface);

    const auto support = VKSwapchain::query_support(context().get_physical_device(), surface);

    EXPECT_TRUE(support.is_adequate());
    EXPECT_FALSE(support.formats.empty());
    EXPECT_GE(support.capabilities.minImageCount, 1U);
    EXPECT_NE(std::ranges::find(support.present_modes, vk::PresentModeKHR::eFifo), support.present_modes.end());
}

TEST_F(VKSwapchainFixture, CreateBuildsImagesViewsAndAnAdvertisedFormat)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);
    const auto support = VKSwapchain::query_support(context().get_physical_device(), surfaces.back());

    EXPECT_TRUE(swapchain->get_swapchain());
    EXPECT_GE(swapchain->get_image_count(), support.capabilities.minImageCount);
    if (support.capabilities.maxImageCount > 0) {
        EXPECT_LE(swapchain->get_image_count(), support.capabilities.maxImageCount);
    }
    EXPECT_EQ(swapchain->get_image_views().size(), swapchain->get_images().size());
    for (const auto& view : swapchain->get_image_views()) {
        EXPECT_TRUE(view);
    }
    EXPECT_NE(std::ranges::find_if(support.formats, [&](const auto& f) { return f.format == swapchain->get_image_format(); }),
        support.formats.end());
}

TEST_F(VKSwapchainFixture, ExtentFollowsTheSurfaceOrTheClampedWindowSize)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);

    const auto expected = expected_extent(surfaces.back(), window);

    EXPECT_EQ(swapchain->get_extent().width, expected.width);
    EXPECT_EQ(swapchain->get_extent().height, expected.height);
}

TEST_F(VKSwapchainFixture, RecreateRebuildsTheSwapchainAtTheCurrentWindowSize)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);
    const auto before = swapchain->get_swapchain();

    window->set_size(200, 150);
    ASSERT_TRUE(swapchain->recreate(200, 150));

    const auto expected = expected_extent(surfaces.back(), window);
    EXPECT_TRUE(swapchain->get_swapchain());
    EXPECT_NE(swapchain->get_swapchain(), before);
    EXPECT_EQ(swapchain->get_extent().width, expected.width);
    EXPECT_EQ(swapchain->get_extent().height, expected.height);
    EXPECT_GT(swapchain->get_image_count(), 0U);
    EXPECT_EQ(swapchain->get_image_views().size(), swapchain->get_images().size());
}

TEST_F(VKSwapchainFixture, RecreatedSwapchainStillPresents)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);
    ASSERT_TRUE(present_one(*swapchain).has_value());

    ASSERT_TRUE(swapchain->recreate(0, 0));

    EXPECT_TRUE(present_one(*swapchain).has_value());
}

TEST_P(VKSwapchainPresentModeTest, EveryRequestedPresentModeYieldsAWorkingSwapchain)
{
    auto info = native_window_info("swapchain-present-mode");
    info.present_mode = GetParam();
    auto window = make_window(info);
    ASSERT_NE(window, nullptr);

    auto* swapchain = create_swapchain(window);

    ASSERT_NE(swapchain, nullptr);
    EXPECT_TRUE(present_one(*swapchain).has_value());
}

INSTANTIATE_TEST_SUITE_P(Modes, VKSwapchainPresentModeTest,
    ::testing::Values(
        Core::GraphicsSurfaceInfo::PresentMode::IMMEDIATE,
        Core::GraphicsSurfaceInfo::PresentMode::MAILBOX,
        Core::GraphicsSurfaceInfo::PresentMode::FIFO,
        Core::GraphicsSurfaceInfo::PresentMode::FIFO_RELAXED));

TEST_P(VKSwapchainSurfaceFormatTest, RequestedFormatIsUsedWhenAdvertisedAndSomethingValidOtherwise)
{
    auto info = native_window_info("swapchain-surface-format");
    info.surface_format = GetParam();
    auto window = make_window(info);
    ASSERT_NE(window, nullptr);

    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);
    const auto support = VKSwapchain::query_support(context().get_physical_device(), surfaces.back());
    const auto wanted = Core::to_vk_format(GetParam());
    const bool advertised = std::ranges::any_of(support.formats, [&](const auto& f) { return f.format == wanted; });

    if (advertised) {
        EXPECT_EQ(swapchain->get_image_format(), wanted);
    } else {
        EXPECT_TRUE(std::ranges::any_of(support.formats, [&](const auto& f) { return f.format == swapchain->get_image_format(); }));
    }
}

INSTANTIATE_TEST_SUITE_P(Formats, VKSwapchainSurfaceFormatTest,
    ::testing::Values(
        Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB,
        Core::GraphicsSurfaceInfo::SurfaceFormat::R8G8B8A8_SRGB,
        Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_UNORM,
        Core::GraphicsSurfaceInfo::SurfaceFormat::R8G8B8A8_UNORM,
        Core::GraphicsSurfaceInfo::SurfaceFormat::R16G16B16A16_SFLOAT,
        Core::GraphicsSurfaceInfo::SurfaceFormat::A2B10G10R10_UNORM,
        Core::GraphicsSurfaceInfo::SurfaceFormat::R32G32B32A32_SFLOAT));

TEST_F(VKSwapchainFixture, DistinctSharingFamiliesCreateAConcurrentSwapchain)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    const auto& families = context().get_queue_families();

    auto* swapchain = create_swapchain(window,
        { families.graphics_family.value(), families.compute_family.value() });

    EXPECT_NE(swapchain, nullptr);
}

TEST_F(VKSwapchainFixture, DuplicateSharingFamiliesCreateAnExclusiveSwapchain)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    const uint32_t family = context().get_queue_families().graphics_family.value();

    EXPECT_NE(create_swapchain(window, { family, family }), nullptr);
}

TEST_F(VKSwapchainFixture, AcquireAndPresentCycleThroughEveryImage)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);

    std::vector<bool> seen(swapchain->get_image_count(), false);
    for (uint32_t i = 0; i < swapchain->get_image_count() * 3; ++i) {
        const auto index = present_one(*swapchain);
        ASSERT_TRUE(index.has_value()) << "frame " << i;
        ASSERT_LT(*index, swapchain->get_image_count());
        seen[*index] = true;
    }

    EXPECT_GE(std::ranges::count(seen, true), 1);
}

TEST_F(VKSwapchainFixture, CleanupReleasesImagesAndDisablesFurtherUse)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* swapchain = create_swapchain(window);
    ASSERT_NE(swapchain, nullptr);

    swapchain->cleanup();

    EXPECT_EQ(swapchain->get_image_count(), 0U);
    EXPECT_TRUE(swapchain->get_image_views().empty());
    EXPECT_FALSE(swapchain->get_swapchain());
    EXPECT_FALSE(swapchain->acquire_next_image(vk::Semaphore {}).has_value());
    EXPECT_FALSE(swapchain->present(0, vk::Semaphore {}));
    EXPECT_FALSE(swapchain->recreate(100, 100));
    EXPECT_NO_THROW(swapchain->cleanup());
}

TEST_F(VKSwapchainFixture, DestroyingTheSwapchainBeforeItsSurfaceIsClean)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    ASSERT_NE(create_swapchain(window), nullptr);

    EXPECT_NO_THROW(swapchains.clear());
}

TEST(VKSwapchainUninitializedTest, DefaultSwapchainHasNothingAndRejectsUse)
{
    VKSwapchain swapchain;

    EXPECT_EQ(swapchain.get_image_count(), 0U);
    EXPECT_FALSE(swapchain.get_swapchain());
    EXPECT_FALSE(swapchain.acquire_next_image(vk::Semaphore {}).has_value());
    EXPECT_FALSE(swapchain.present(0, vk::Semaphore {}));
    EXPECT_FALSE(swapchain.recreate(1, 1));
    EXPECT_NO_THROW(swapchain.cleanup());
}

TEST(VKSwapchainHdrTest, HdrRequestUsesAnHdrFormatWhenOneIsAdvertisedAndFallsBackOtherwise)
{
    if (!native_display_available()) {
        GTEST_SKIP() << "No display session reachable";
    }
    auto config = presenting_graphics_config();
    config.surface_info.enable_hdr = true;

    Core::VulkanBackend backend;
    try {
        if (!backend.initialize(config)) {
            GTEST_SKIP() << "Presenting Vulkan backend unavailable";
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Presenting Vulkan backend unavailable: " << e.what();
    }
    Core::WindowManager manager(config);
    auto window = manager.create_window(native_window_info("swapchain-hdr"));
    ASSERT_NE(window, nullptr);
    const auto surface = backend.get_context().create_surface(window);
    ASSERT_TRUE(surface);

    VKSwapchain swapchain;
    ASSERT_TRUE(swapchain.create(backend.get_context(), surface, window->get_create_info()));
    const auto support = VKSwapchain::query_support(backend.get_context().get_physical_device(), surface);

    const std::vector<std::pair<vk::Format, vk::ColorSpaceKHR>> hdr_candidates {
        { vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eHdr10St2084EXT },
        { vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eExtendedSrgbLinearEXT },
        { vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eHdr10St2084EXT },
        { vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eExtendedSrgbLinearEXT },
    };
    std::optional<vk::Format> expected;
    for (const auto& [format, space] : hdr_candidates) {
        if (std::ranges::any_of(support.formats, [&](const auto& f) { return f.format == format && f.colorSpace == space; })) {
            expected = format;
            break;
        }
    }

    if (expected.has_value()) {
        EXPECT_EQ(swapchain.get_image_format(), *expected);
    } else {
        EXPECT_TRUE(std::ranges::any_of(support.formats, [&](const auto& f) { return f.format == swapchain.get_image_format(); }));
    }

    swapchain.cleanup();
    backend.get_context().destroy_surface(surface);
}

}

#endif
