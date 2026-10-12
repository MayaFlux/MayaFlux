#include "gtest/gtest.h"

#include "../support/native_window_test_support.hpp"

#if defined(MAYAFLUX_TEST_NATIVE_WINDOWS)

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::VulkanBackend;
    using Core::Window;
    using Core::WindowManager;
    using Registry::Service::DisplayService;

    int g_window_counter = 0;

    class BackendWindowHandlerTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (!native_display_available()) {
                s_reason = "No display session reachable";
                return;
            }
            const auto config = presenting_graphics_config();
            try {
                auto candidate = std::make_unique<VulkanBackend>();
                if (!candidate->initialize(config)) {
                    s_reason = "VulkanBackend::initialize returned false";
                    return;
                }
                s_backend = std::move(candidate);
            } catch (const std::exception& e) {
                s_reason = e.what();
                return;
            }
            s_manager = std::make_unique<WindowManager>(config);
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
            rendering_buffers.clear();
        }

        static VulkanBackend& backend() { return *s_backend; }
        static WindowManager& manager() { return *s_manager; }
        static DisplayService* service() { return Registry::BackendRegistry::instance().get_service<DisplayService>(); }

        std::shared_ptr<Window> make_window(std::array<float, 4> clear = { 0.0F, 0.0F, 0.0F, 1.0F })
        {
            auto info = native_window_info("window-handler-" + std::to_string(++g_window_counter));
            info.clear_color = clear;
            auto window = manager().create_window(info);
            if (window) {
                windows.push_back(window);
            }
            return window;
        }

        std::shared_ptr<Window> registered_window(std::array<float, 4> clear = { 0.0F, 0.0F, 0.0F, 1.0F })
        {
            auto window = make_window(clear);
            if (!window || !backend().register_window(window)) {
                return nullptr;
            }
            return window;
        }

        bool present_external_frame(const std::shared_ptr<Window>& window, std::array<float, 4> color)
        {
            auto* display = service();
            if (!display) {
                return false;
            }

            const uint64_t image_bits = display->acquire_next_swapchain_image(window);
            if (image_bits == 0) {
                return false;
            }
            const vk::Image image(reinterpret_cast<VkImage>(image_bits));
            auto* view_ptr = static_cast<vk::ImageView*>(display->get_current_image_view(window));
            if (!view_ptr) {
                return false;
            }
            uint32_t width = 0;
            uint32_t height = 0;
            display->get_swapchain_extent(window, width, height);

            auto& commands = backend().get_command_manager();
            auto cmd = commands.allocate_command_buffer();
            cmd.begin(vk::CommandBufferBeginInfo { vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

            vk::ImageMemoryBarrier barrier {};
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

            barrier.oldLayout = vk::ImageLayout::eUndefined;
            barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, barrier);

            vk::RenderingAttachmentInfo attachment {};
            attachment.imageView = *view_ptr;
            attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachment.loadOp = vk::AttachmentLoadOp::eClear;
            attachment.storeOp = vk::AttachmentStoreOp::eStore;
            attachment.clearValue.color = vk::ClearColorValue(color);

            vk::RenderingInfo info {};
            info.renderArea = vk::Rect2D { { 0, 0 }, { width, height } };
            info.layerCount = 1;
            info.colorAttachmentCount = 1;
            info.pColorAttachments = &attachment;
            cmd.beginRendering(info);
            cmd.endRendering();

            barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
            barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
            barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            barrier.dstAccessMask = {};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);
            cmd.end();

            uint64_t cmd_bits = 0;
            std::memcpy(&cmd_bits, &cmd, sizeof(cmd));
            display->submit_and_present(window, cmd_bits);

            backend().wait_idle();
            commands.free_command_buffer(cmd);
            return true;
        }

        std::shared_ptr<Window> capturing_window(bool capture = true)
        {
            auto window = registered_window({ 1.0F, 1.0F, 1.0F, 1.0F });
            if (!window) {
                return nullptr;
            }
            window->set_capture_enabled(capture);
            auto buffer = std::make_shared<Buffers::VKBuffer>(
                64, Buffers::VKBuffer::Usage::STAGING, Kakshya::DataModality::IMAGE_COLOR);
            window->register_rendering_buffer(buffer);
            rendering_buffers.push_back(buffer);
            return window;
        }

        template <typename Predicate>
        bool present_until(const std::shared_ptr<Window>& window, const Predicate& done,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < deadline) {
                if (done()) {
                    return true;
                }
                if (!present_external_frame(window, { 1.0F, 1.0F, 1.0F, 1.0F })) {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return done();
        }

        std::vector<std::shared_ptr<Window>> windows;
        std::vector<std::shared_ptr<Buffers::VKBuffer>> rendering_buffers;

    private:
        static inline std::unique_ptr<VulkanBackend> s_backend;
        static inline std::unique_ptr<WindowManager> s_manager;
        static inline std::string s_reason;
    };

}

TEST_F(BackendWindowHandlerTest, RegisterCreatesSwapchainAndMarksTheWindow)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    EXPECT_TRUE(backend().register_window(window));

    EXPECT_TRUE(window->is_graphics_registered());
    EXPECT_TRUE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, RegisteringTheSameWindowTwiceIsRejected)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);

    EXPECT_FALSE(backend().register_window(window));
    EXPECT_TRUE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, UnregisterReleasesTheWindow)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);

    backend().unregister_window(window);

    EXPECT_FALSE(backend().is_window_registered(window));
    EXPECT_FALSE(window->is_graphics_registered());
}

TEST_F(BackendWindowHandlerTest, UnregisteredWindowCanRegisterAgain)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    backend().unregister_window(window);

    EXPECT_TRUE(backend().register_window(window));
}

TEST_F(BackendWindowHandlerTest, UnregisteringAnUnknownWindowIsNoOp)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    EXPECT_NO_THROW(backend().unregister_window(window));
    EXPECT_FALSE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, TwoWindowsHaveIndependentRegistrations)
{
    auto first = registered_window();
    auto second = registered_window();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    backend().unregister_window(first);

    EXPECT_FALSE(backend().is_window_registered(first));
    EXPECT_TRUE(backend().is_window_registered(second));
}

TEST_F(BackendWindowHandlerTest, ServicesReportNothingForAnUnregisteredWindow)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    uint32_t width = 7;
    uint32_t height = 7;
    display->get_swapchain_extent(window, width, height);

    EXPECT_EQ(display->get_swapchain_image_count(window), 0U);
    EXPECT_EQ(display->get_swapchain_format(window), 0);
    EXPECT_EQ(width, 0U);
    EXPECT_EQ(height, 0U);
    EXPECT_EQ(display->acquire_next_swapchain_image(window), 0U);
    EXPECT_EQ(display->get_current_image_view(window), nullptr);
    EXPECT_EQ(display->get_current_swapchain_image(window), 0U);
    EXPECT_EQ(display->get_depth_image_view(window), nullptr);
}

TEST_F(BackendWindowHandlerTest, SwapchainServicesDescribeTheRegisteredWindow)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    uint32_t width = 0;
    uint32_t height = 0;
    display->get_swapchain_extent(window, width, height);

    EXPECT_GE(display->get_swapchain_image_count(window), 2U);
    EXPECT_NE(display->get_swapchain_format(window), 0);
    EXPECT_GT(width, 0U);
    EXPECT_GT(height, 0U);
}

TEST_F(BackendWindowHandlerTest, RenderingAnUnregisteredWindowIsIgnored)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    EXPECT_NO_THROW(backend().render_window(window));
    EXPECT_FALSE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, RenderingAnEmptyWindowCyclesThroughFrameResources)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    const uint32_t frames = display->get_swapchain_image_count(window) * 3;

    for (uint32_t i = 0; i < frames; ++i) {
        EXPECT_NO_THROW(backend().render_window(window)) << "frame " << i;
    }

    backend().wait_idle();
    EXPECT_TRUE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, ExternallyRecordedFramesAcquireAndPresentThroughTheService)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);

    for (int i = 0; i < 6; ++i) {
        ASSERT_TRUE(present_external_frame(window, { 0.0F, 0.0F, 1.0F, 1.0F })) << "frame " << i;
    }
}

TEST_F(BackendWindowHandlerTest, AcquiredImageAndViewAgreeWithTheCurrentSwapchainImage)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    ASSERT_TRUE(present_external_frame(window, { 0.0F, 0.0F, 0.0F, 1.0F }));

    EXPECT_NE(display->get_current_swapchain_image(window), 0U);
    EXPECT_NE(display->get_current_image_view(window), nullptr);
}

TEST_F(BackendWindowHandlerTest, ReadbackReturnsThePresentedClearColor)
{
    auto window = registered_window({ 1.0F, 1.0F, 1.0F, 1.0F });
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    ASSERT_TRUE(present_external_frame(window, { 1.0F, 1.0F, 1.0F, 1.0F }));

    constexpr uint32_t k_side = 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(k_side) * k_side * 4, 0);

    ASSERT_TRUE(display->readback_swapchain_region(window, pixels.data(), 0, 0, k_side, k_side, pixels.size()));

    for (size_t i = 0; i < pixels.size(); ++i) {
        ASSERT_EQ(pixels[i], 255U) << "byte " << i;
    }
}

TEST_F(BackendWindowHandlerTest, ReadbackFailsForAnUnregisteredWindow)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    std::array<uint8_t, 64> pixels {};

    EXPECT_FALSE(display->readback_swapchain_region(window, pixels.data(), 0, 0, 4, 4, pixels.size()));
}

TEST_F(BackendWindowHandlerTest, DepthAttachmentIsCreatedOnDemand)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    ASSERT_EQ(display->get_depth_image_view(window), nullptr);

    display->ensure_depth_attachment(window);

    EXPECT_NE(display->get_depth_image_view(window), nullptr);
    EXPECT_EQ(display->get_depth_format(window), static_cast<uint32_t>(vk::Format::eD32Sfloat));
}

TEST_F(BackendWindowHandlerTest, DepthAttachmentRequestOnUnregisteredWindowIsIgnored)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    EXPECT_NO_THROW(display->ensure_depth_attachment(window));
    EXPECT_EQ(display->get_depth_image_view(window), nullptr);
}

TEST_F(BackendWindowHandlerTest, ResizeRequestKeepsTheWindowRenderable)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    ASSERT_TRUE(present_external_frame(window, { 0.0F, 0.0F, 0.0F, 1.0F }));

    display->resize_surface(window, 200, 150);
    EXPECT_NO_THROW(backend().handle_window_resize());

    uint32_t width = 0;
    uint32_t height = 0;
    display->get_swapchain_extent(window, width, height);
    EXPECT_GT(width, 0U);
    EXPECT_GT(height, 0U);
    EXPECT_TRUE(present_external_frame(window, { 0.0F, 0.0F, 0.0F, 1.0F }));
}

TEST_F(BackendWindowHandlerTest, HandlingResizeWithNothingPendingIsNoOp)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);

    EXPECT_NO_THROW(backend().handle_window_resize());
    EXPECT_TRUE(backend().is_window_registered(window));
}

TEST_F(BackendWindowHandlerTest, WaitIdleServiceCompletes)
{
    auto* display = service();
    ASSERT_NE(display, nullptr);
    EXPECT_NO_THROW(display->wait_idle());
}

struct FrameLog {
    std::atomic<uint32_t> count { 0 };
    std::atomic<uint32_t> width { 0 };
    std::atomic<uint32_t> height { 0 };
    std::atomic<uint32_t> format { 0 };
    std::atomic<size_t> bytes { 0 };
};

TEST_F(BackendWindowHandlerTest, FrameObserverIsRefusedWhenCaptureIsDisabled)
{
    auto window = capturing_window(false);
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    const uint32_t id = display->register_frame_observer(window,
        [](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { });

    EXPECT_EQ(id, 0U);
}

TEST_F(BackendWindowHandlerTest, FrameObserverCallsOnAnUnregisteredWindowAreIgnored)
{
    auto window = make_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    EXPECT_EQ(display->register_frame_observer(window,
                  [](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { }),
        0U);
    EXPECT_NO_THROW(display->unregister_frame_observer(window, 1));
    EXPECT_EQ(display->get_last_frame(window), nullptr);
}

TEST_F(BackendWindowHandlerTest, UnregisteringAnObserverBeforeCaptureStartsIsIgnored)
{
    auto window = capturing_window(false);
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    EXPECT_NO_THROW(display->unregister_frame_observer(window, 1));
}

TEST_F(BackendWindowHandlerTest, CapturedFramesReachObserversWithTheSwapchainGeometry)
{
    auto window = capturing_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    auto log = std::make_shared<FrameLog>();
    const uint32_t id = display->register_frame_observer(window,
        [log](const std::shared_ptr<std::vector<uint8_t>>& frame, uint32_t width, uint32_t height, uint32_t format) {
            log->width = width;
            log->height = height;
            log->format = format;
            log->bytes = frame ? frame->size() : 0;
            ++log->count;
        });
    ASSERT_NE(id, 0U);

    ASSERT_TRUE(present_until(window, [log] { return log->count.load() > 0; }));

    uint32_t width = 0;
    uint32_t height = 0;
    display->get_swapchain_extent(window, width, height);
    EXPECT_EQ(log->width.load(), width);
    EXPECT_EQ(log->height.load(), height);
    EXPECT_EQ(log->format.load(), static_cast<uint32_t>(display->get_swapchain_format(window)));
    EXPECT_EQ(log->bytes.load(), static_cast<size_t>(width) * height * 4);
}

TEST_F(BackendWindowHandlerTest, LastFrameIsAvailableOnceACaptureCompletes)
{
    auto window = capturing_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    auto log = std::make_shared<FrameLog>();
    ASSERT_NE(display->register_frame_observer(window,
                  [log](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { ++log->count; }),
        0U);
    EXPECT_EQ(display->get_last_frame(window), nullptr);

    ASSERT_TRUE(present_until(window, [log] { return log->count.load() > 0; }));

    uint32_t width = 0;
    uint32_t height = 0;
    display->get_swapchain_extent(window, width, height);
    auto frame = display->get_last_frame(window);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->size(), static_cast<size_t>(width) * height * 4);
}

TEST_F(BackendWindowHandlerTest, ObserverIdsAreDistinctAndUnregisteringStopsDelivery)
{
    auto window = capturing_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    auto kept = std::make_shared<FrameLog>();
    auto removed = std::make_shared<FrameLog>();
    const uint32_t kept_id = display->register_frame_observer(window,
        [kept](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { ++kept->count; });
    const uint32_t removed_id = display->register_frame_observer(window,
        [removed](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { ++removed->count; });
    ASSERT_NE(kept_id, 0U);
    ASSERT_NE(removed_id, 0U);
    EXPECT_NE(kept_id, removed_id);

    ASSERT_TRUE(present_until(window, [&] { return kept->count.load() > 0 && removed->count.load() > 0; }));

    display->unregister_frame_observer(window, removed_id);
    backend().wait_idle();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const uint32_t removed_before = removed->count.load();
    const uint32_t kept_before = kept->count.load();

    ASSERT_TRUE(present_until(window, [&] { return kept->count.load() > kept_before; }));

    EXPECT_EQ(removed->count.load(), removed_before);
}

TEST_F(BackendWindowHandlerTest, CaptureDisabledNeverProducesAFrame)
{
    auto window = capturing_window(false);
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(present_external_frame(window, { 1.0F, 1.0F, 1.0F, 1.0F }));
    }

    EXPECT_EQ(display->get_last_frame(window), nullptr);
}

TEST_F(BackendWindowHandlerTest, CaptureWithoutRenderingBuffersProducesNothing)
{
    auto window = registered_window();
    ASSERT_NE(window, nullptr);
    window->set_capture_enabled(true);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    auto log = std::make_shared<FrameLog>();
    ASSERT_NE(display->register_frame_observer(window,
                  [log](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { ++log->count; }),
        0U);

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(present_external_frame(window, { 0.0F, 0.0F, 0.0F, 1.0F }));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(log->count.load(), 0U);
}

TEST_F(BackendWindowHandlerTest, UnregisteringACapturingWindowStopsItsReadbackThread)
{
    auto window = capturing_window();
    ASSERT_NE(window, nullptr);
    auto* display = service();
    ASSERT_NE(display, nullptr);
    auto log = std::make_shared<FrameLog>();
    ASSERT_NE(display->register_frame_observer(window,
                  [log](const std::shared_ptr<std::vector<uint8_t>>&, uint32_t, uint32_t, uint32_t) { ++log->count; }),
        0U);
    ASSERT_TRUE(present_until(window, [log] { return log->count.load() > 0; }));

    backend().unregister_window(window);

    EXPECT_FALSE(backend().is_window_registered(window));
    EXPECT_FALSE(window->is_graphics_registered());
    EXPECT_EQ(display->get_last_frame(window), nullptr);
}

TEST(BackendWindowHandlerCleanupTest, BackendCleanupReleasesWindowsStillRegistered)
{
    if (!native_display_available()) {
        GTEST_SKIP() << "No display session reachable";
    }
    const auto config = presenting_graphics_config();
    auto backend = std::make_unique<VulkanBackend>();
    try {
        if (!backend->initialize(config)) {
            GTEST_SKIP() << "Presenting Vulkan backend unavailable";
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Presenting Vulkan backend unavailable: " << e.what();
    }

    WindowManager manager(config);
    auto window = manager.create_window(native_window_info("window-handler-cleanup"));
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(backend->register_window(window));

    backend->cleanup();

    EXPECT_FALSE(window->is_graphics_registered());
}

}

#endif
