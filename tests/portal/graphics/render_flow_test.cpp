#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Test {
namespace {

    using Kakshya::ImageFormat;
    using Portal::Graphics::CullMode;
    using Portal::Graphics::DescriptorBindingInfo;
    using Portal::Graphics::INVALID_RENDER_PIPELINE;
    using Portal::Graphics::INVALID_SHADER;
    using Portal::Graphics::RenderFlow;
    using Portal::Graphics::RenderPipelineConfig;
    using Portal::Graphics::ShaderID;
    using Portal::Graphics::ShaderStage;
    using Portal::Graphics::TextureLoom;

    constexpr vk::Format k_color_format = vk::Format::eR8G8B8A8Unorm;
    constexpr uint32_t k_extent = 8;

    constexpr std::string_view k_vertex_source = R"(#version 450
void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

    constexpr std::string_view k_fragment_source = R"(#version 450
layout(location = 0) out vec4 color;
layout(push_constant) uniform Params { vec4 value; } params;
void main() { color = params.value; }
)";

    RenderFlow& flow()
    {
        return Portal::Graphics::get_render_flow();
    }

    Portal::Graphics::ShaderFoundry& foundry()
    {
        return Portal::Graphics::get_shader_foundry();
    }

    TextureLoom& loom()
    {
        return Portal::Graphics::get_texture_manager();
    }

    class RenderFlowTest : public ::testing::Test {
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
            teardown_portal();
            ASSERT_TRUE(foundry().initialize(s_backend));
            ASSERT_TRUE(flow().initialize());
        }

        void TearDown() override
        {
            teardown_portal();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

        static void teardown_portal()
        {
            flow().shutdown();
            loom().shutdown();
            foundry().shutdown();
        }

        static ShaderID vertex_shader() { return foundry().load_shader(std::string(k_vertex_source), ShaderStage::VERTEX); }
        static ShaderID fragment_shader() { return foundry().load_shader(std::string(k_fragment_source), ShaderStage::FRAGMENT); }

        static RenderPipelineConfig basic_config()
        {
            RenderPipelineConfig config;
            config.vertex_shader = vertex_shader();
            config.fragment_shader = fragment_shader();
            config.rasterization.cull_mode = CullMode::NONE;
            config.depth_stencil.depth_test_enable = false;
            config.depth_stencil.depth_write_enable = false;
            config.blend_attachments.emplace_back();
            config.push_constant_size = sizeof(float) * 4;
            return config;
        }

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST_F(RenderFlowTest, InitializeNeedsAnInitializedShaderFoundry)
    {
        flow().shutdown();
        foundry().shutdown();

        EXPECT_FALSE(flow().initialize());
        EXPECT_FALSE(flow().is_initialized());
    }

    TEST_F(RenderFlowTest, InitializingAgainReportsSuccess)
    {
        EXPECT_TRUE(flow().initialize());
        EXPECT_TRUE(flow().is_initialized());
    }

    TEST_F(RenderFlowTest, ShutdownIsRepeatableAndTheFlowInitializesAgain)
    {
        flow().shutdown();

        EXPECT_NO_THROW(flow().shutdown());
        EXPECT_FALSE(flow().is_initialized());
        EXPECT_TRUE(flow().initialize());
        EXPECT_NE(flow().create_pipeline(basic_config(), { k_color_format }), INVALID_RENDER_PIPELINE);
    }

    TEST_F(RenderFlowTest, UninitializedFlowCreatesNoPipelines)
    {
        const auto config = basic_config();
        flow().shutdown();

        EXPECT_EQ(flow().create_pipeline(config, { k_color_format }), INVALID_RENDER_PIPELINE);
    }

    // ------------------------------------------------------------------
    // Pipelines
    // ------------------------------------------------------------------

    TEST_F(RenderFlowTest, PipelineNeedsAVertexOrMeshShader)
    {
        RenderPipelineConfig config = basic_config();
        config.vertex_shader = INVALID_SHADER;

        EXPECT_EQ(flow().create_pipeline(config, { k_color_format }), INVALID_RENDER_PIPELINE);
    }

    TEST_F(RenderFlowTest, PipelineNeedsAtLeastOneColorFormat)
    {
        EXPECT_EQ(flow().create_pipeline(basic_config(), {}), INVALID_RENDER_PIPELINE);
    }

    TEST_F(RenderFlowTest, ValidConfigCreatesPipelinesWithDistinctIds)
    {
        const auto first = flow().create_pipeline(basic_config(), { k_color_format });
        const auto second = flow().create_pipeline(basic_config(), { k_color_format });

        EXPECT_NE(first, INVALID_RENDER_PIPELINE);
        EXPECT_NE(second, INVALID_RENDER_PIPELINE);
        EXPECT_NE(first, second);
    }

    TEST_F(RenderFlowTest, ViewTransformLayoutExistsForAPipelineAndNotForAnUnknownId)
    {
        const auto id = flow().create_pipeline(basic_config(), { k_color_format });
        ASSERT_NE(id, INVALID_RENDER_PIPELINE);

        EXPECT_TRUE(flow().get_view_transform_layout(id));
        EXPECT_FALSE(flow().get_view_transform_layout(9999));
    }

    TEST_F(RenderFlowTest, DestroyPipelineForgetsItAndIgnoresUnknownIds)
    {
        const auto id = flow().create_pipeline(basic_config(), { k_color_format });
        ASSERT_NE(id, INVALID_RENDER_PIPELINE);

        flow().destroy_pipeline(id);

        EXPECT_FALSE(flow().get_view_transform_layout(id));
        EXPECT_NO_THROW(flow().destroy_pipeline(id));
        EXPECT_NO_THROW(flow().destroy_pipeline(9999));
    }

    // ------------------------------------------------------------------
    // Descriptors
    // ------------------------------------------------------------------

    TEST_F(RenderFlowTest, ADescriptorSetIsAllocatedForTheReservedViewTransformLayout)
    {
        const auto id = flow().create_pipeline(basic_config(), { k_color_format });
        ASSERT_NE(id, INVALID_RENDER_PIPELINE);

        const auto sets = flow().allocate_pipeline_descriptors(id);

        ASSERT_EQ(sets.size(), 1U);
        EXPECT_TRUE(foundry().get_descriptor_set(sets[0]));
    }

    TEST_F(RenderFlowTest, EachExtraDescriptorSetAddsALayoutAndFirstIndexSkipsLayouts)
    {
        RenderPipelineConfig config = basic_config();
        config.descriptor_sets = { { DescriptorBindingInfo { .set = 1, .binding = 0, .type = vk::DescriptorType::eUniformBuffer } } };
        const auto id = flow().create_pipeline(config, { k_color_format });
        ASSERT_NE(id, INVALID_RENDER_PIPELINE);

        EXPECT_EQ(flow().allocate_pipeline_descriptors(id).size(), 2U);
        EXPECT_EQ(flow().allocate_pipeline_descriptors(id, 1).size(), 1U);
        EXPECT_TRUE(flow().allocate_pipeline_descriptors(id, 2).empty());
    }

    TEST_F(RenderFlowTest, AllocatingForAnUnknownPipelineGivesNothing)
    {
        EXPECT_TRUE(flow().allocate_pipeline_descriptors(9999).empty());
    }

    // ------------------------------------------------------------------
    // Recording
    // ------------------------------------------------------------------

    TEST_F(RenderFlowTest, RecordingWithUnknownIdsIsIgnored)
    {
        const auto pipeline = flow().create_pipeline(basic_config(), { k_color_format });
        ASSERT_NE(pipeline, INVALID_RENDER_PIPELINE);
        const auto cmd = foundry().begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);
        const std::array<float, 4> color { 1.0F, 0.0F, 0.0F, 1.0F };

        EXPECT_NO_THROW(flow().bind_pipeline(cmd, 9999));
        EXPECT_NO_THROW(flow().bind_pipeline(9999, pipeline));
        EXPECT_NO_THROW(flow().bind_descriptor_sets(cmd, 9999, {}));
        EXPECT_NO_THROW(flow().push_constants(cmd, 9999, color.data(), sizeof(color)));
        EXPECT_NO_THROW(flow().push_constants(9999, pipeline, color.data(), sizeof(color)));
        EXPECT_NO_THROW(flow().bind_vertex_buffers(9999, {}));
        EXPECT_NO_THROW(flow().bind_index_buffer(9999, nullptr));
        EXPECT_NO_THROW(flow().draw(9999, 3));
        EXPECT_NO_THROW(flow().draw_indexed(9999, 3));
        EXPECT_NO_THROW(flow().draw_mesh_tasks(9999, 1, 1, 1));
        EXPECT_NO_THROW(flow().draw_mesh_tasks_indirect(9999, nullptr, 0, 1));

        foundry().submit_and_wait(cmd);
    }

    TEST_F(RenderFlowTest, BindPushAndDrawFillATargetWithThePushedColor)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const auto target = loom().create_render_target(k_extent, k_extent, ImageFormat::RGBA8);
        ASSERT_NE(target, nullptr);
        const auto pipeline = flow().create_pipeline(basic_config(), { k_color_format });
        ASSERT_NE(pipeline, INVALID_RENDER_PIPELINE);
        const std::array<float, 4> green { 0.0F, 1.0F, 0.0F, 1.0F };

        const auto id = foundry().begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);
        auto cmd = foundry().get_command_buffer(id);

        vk::RenderingAttachmentInfo attachment {};
        attachment.imageView = target->get_image_view();
        attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachment.loadOp = vk::AttachmentLoadOp::eClear;
        attachment.storeOp = vk::AttachmentStoreOp::eStore;
        attachment.clearValue.color = vk::ClearColorValue { std::array<float, 4> { 0.0F, 0.0F, 0.0F, 0.0F } };

        vk::RenderingInfo info {};
        info.renderArea = vk::Rect2D { { 0, 0 }, { k_extent, k_extent } };
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = &attachment;

        cmd.beginRendering(info);
        flow().bind_pipeline(id, pipeline);
        cmd.setViewport(0, vk::Viewport { 0.0F, 0.0F, static_cast<float>(k_extent), static_cast<float>(k_extent), 0.0F, 1.0F });
        cmd.setScissor(0, vk::Rect2D { { 0, 0 }, { k_extent, k_extent } });
        flow().push_constants(id, pipeline, green.data(), sizeof(green));
        flow().draw(id, 3);
        cmd.endRendering();
        foundry().submit_and_wait(id);

        const auto downloaded = loom().download_image(target);
        ASSERT_TRUE(downloaded.has_value());
        ASSERT_EQ(downloaded->byte_size(), static_cast<size_t>(k_extent) * k_extent * sizeof(uint32_t));
        std::vector<uint32_t> pixels(static_cast<size_t>(k_extent) * k_extent);
        std::memcpy(pixels.data(), downloaded->data(), downloaded->byte_size());
        EXPECT_EQ(pixels, std::vector<uint32_t>(pixels.size(), 0xFF00FF00U));
    }

    // ------------------------------------------------------------------
    // Window registration without a window
    // ------------------------------------------------------------------

    TEST_F(RenderFlowTest, NullWindowsAreNeverRegistered)
    {
        EXPECT_NO_THROW(flow().register_window_for_rendering(nullptr));
        EXPECT_NO_THROW(flow().unregister_window(nullptr));
        EXPECT_FALSE(flow().is_window_registered(nullptr));
        EXPECT_TRUE(flow().get_registered_windows().empty());
    }

} // namespace
} // namespace MayaFlux::Test
