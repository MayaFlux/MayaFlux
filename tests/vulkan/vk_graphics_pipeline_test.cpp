#include "gtest/gtest.h"

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::GraphicsPipelineConfig;
    using Core::VKGraphicsPipeline;
    using Core::VKImage;
    using Core::VKShaderModule;

    constexpr vk::Format k_color_format = vk::Format::eR8G8B8A8Unorm;
    constexpr uint32_t k_extent = 8;

    constexpr const char* k_triangle_vertex = R"(#version 450
void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

    constexpr const char* k_color_fragment = R"(#version 450
layout(location = 0) out vec4 color;
layout(push_constant) uniform Params { vec4 value; } params;
void main() { color = params.value; }
)";

    constexpr const char* k_compute_source = R"(#version 450
layout(local_size_x = 1) in;
void main() { }
)";

    class VKGraphicsPipelineTest : public VulkanDeviceTest {
    protected:
        void TearDown() override
        {
            if (IsSkipped()) {
                return;
            }
            for (auto& pipeline : pipelines) {
                pipeline->cleanup(device());
            }
            for (auto& shader : shaders) {
                shader->cleanup(device());
            }
        }

        static vk::Device device() { return backend().get_context().get_device(); }

        std::shared_ptr<VKShaderModule> compile(const char* source, vk::ShaderStageFlagBits stage)
        {
            auto module = std::make_shared<VKShaderModule>();
            if (!module->create_from_glsl(device(), source, stage)) {
                return nullptr;
            }
            shaders.push_back(module);
            return module;
        }

        GraphicsPipelineConfig color_config()
        {
            auto config = GraphicsPipelineConfig::default_2d();
            config.vertex_shader = compile(k_triangle_vertex, vk::ShaderStageFlagBits::eVertex);
            config.fragment_shader = compile(k_color_fragment, vk::ShaderStageFlagBits::eFragment);
            config.color_attachment_formats = { k_color_format };
            config.push_constant_ranges = { { vk::ShaderStageFlagBits::eFragment, 0, 16 } };
            return config;
        }

        std::shared_ptr<VKGraphicsPipeline> create(const GraphicsPipelineConfig& config, bool expect_success = true)
        {
            auto pipeline = std::make_shared<VKGraphicsPipeline>();
            pipelines.push_back(pipeline);
            const bool created = pipeline->create(device(), config);
            EXPECT_EQ(created, expect_success);
            return pipeline;
        }

        std::vector<std::shared_ptr<VKShaderModule>> shaders;
        std::vector<std::shared_ptr<VKGraphicsPipeline>> pipelines;
    };

}

TEST(VKGraphicsPipelineUninitializedTest, DefaultPipelineIsInvalidAndIgnoresCommands)
{
    VKGraphicsPipeline pipeline;

    EXPECT_FALSE(pipeline.is_valid());
    EXPECT_FALSE(pipeline.get());
    EXPECT_FALSE(pipeline.get_layout());
    EXPECT_NO_THROW(pipeline.bind(vk::CommandBuffer {}));
    EXPECT_NO_THROW(pipeline.draw(vk::CommandBuffer {}, 3));
}

TEST(VKGraphicsPipelineUninitializedTest, CreateWithNullDeviceFails)
{
    VKGraphicsPipeline pipeline;
    EXPECT_FALSE(pipeline.create(vk::Device {}, GraphicsPipelineConfig::default_2d()));
}

TEST_F(VKGraphicsPipelineTest, RejectsAConfigWithoutAVertexShader)
{
    auto config = color_config();
    config.vertex_shader = nullptr;

    auto pipeline = create(config, false);
    EXPECT_FALSE(pipeline->is_valid());
}

TEST_F(VKGraphicsPipelineTest, RejectsAnInvalidVertexShaderModule)
{
    auto config = color_config();
    config.vertex_shader = std::make_shared<VKShaderModule>();

    create(config, false);
}

TEST_F(VKGraphicsPipelineTest, RejectsShadersInTheWrongStageSlots)
{
    auto wrong_vertex = color_config();
    wrong_vertex.vertex_shader = compile(k_compute_source, vk::ShaderStageFlagBits::eCompute);
    create(wrong_vertex, false);

    auto wrong_fragment = color_config();
    wrong_fragment.fragment_shader = compile(k_compute_source, vk::ShaderStageFlagBits::eCompute);
    create(wrong_fragment, false);
}

TEST_F(VKGraphicsPipelineTest, MeshPipelineRequiresMeshStageAndExcludesVertexShaders)
{
    GraphicsPipelineConfig wrong_stage;
    wrong_stage.mesh_shader = compile(k_compute_source, vk::ShaderStageFlagBits::eCompute);
    create(wrong_stage, false);

    auto mixed = color_config();
    mixed.mesh_shader = compile(k_compute_source, vk::ShaderStageFlagBits::eCompute);
    create(mixed, false);
}

TEST_F(VKGraphicsPipelineTest, CreatesAColorPipelineForDynamicRendering)
{
    auto config = color_config();
    auto pipeline = create(config);

    EXPECT_TRUE(pipeline->is_valid());
    EXPECT_TRUE(pipeline->get());
    EXPECT_TRUE(pipeline->get_layout());
    EXPECT_EQ(pipeline->get_config().color_attachment_formats, config.color_attachment_formats);
}

TEST_F(VKGraphicsPipelineTest, CreatesAVertexOnlyDepthPipeline)
{
    auto config = GraphicsPipelineConfig::depth_only();
    config.vertex_shader = compile(k_triangle_vertex, vk::ShaderStageFlagBits::eVertex);
    config.depth_attachment_format = vk::Format::eD32Sfloat;

    auto pipeline = create(config);
    EXPECT_TRUE(pipeline->is_valid());
}

TEST_F(VKGraphicsPipelineTest, AcceptsExplicitVertexBindingsAndAttributes)
{
    const char* source = R"(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
void main() { gl_Position = vec4(position, 1.0) + vec4(uv, 0.0, 0.0); }
)";
    auto config = color_config();
    config.vertex_shader = compile(source, vk::ShaderStageFlagBits::eVertex);
    config.vertex_bindings = { { 0, 20 } };
    config.vertex_attributes = {
        { 0, 0, vk::Format::eR32G32B32Sfloat, 0 },
        { 1, 0, vk::Format::eR32G32Sfloat, 12 }
    };

    auto pipeline = create(config);
    EXPECT_TRUE(pipeline->is_valid());
}

TEST_F(VKGraphicsPipelineTest, CleanupInvalidatesThePipelineAndIsRepeatable)
{
    auto pipeline = create(color_config());

    pipeline->cleanup(device());

    EXPECT_FALSE(pipeline->is_valid());
    EXPECT_NO_THROW(pipeline->cleanup(device()));
}

TEST_F(VKGraphicsPipelineTest, MovedPipelineKeepsItsHandles)
{
    auto source = create(color_config());
    const auto handle = source->get();

    VKGraphicsPipeline moved(std::move(*source));

    EXPECT_EQ(moved.get(), handle);
    EXPECT_FALSE(source->is_valid());
    moved.cleanup(device());
}

TEST_F(VKGraphicsPipelineTest, DrawsAFullTriangleIntoAnOffscreenImage)
{
    auto pipeline = create(color_config());
    ASSERT_TRUE(pipeline->is_valid());

    auto& resources = backend().get_resource_manager();
    auto image = std::make_shared<VKImage>(k_extent, k_extent, 1, k_color_format, VKImage::Usage::RENDER_TARGET);
    resources.initialize_image(image);
    resources.transition_image_layout(image->get_image(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    const std::array<float, 4> color { 1.0F, 0.0F, 0.5F, 1.0F };
    resources.execute_immediate_commands([&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo attachment {};
        attachment.imageView = image->get_image_view();
        attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachment.loadOp = vk::AttachmentLoadOp::eClear;
        attachment.storeOp = vk::AttachmentStoreOp::eStore;
        attachment.clearValue.color = vk::ClearColorValue { std::array<float, 4> { 0.0F, 0.0F, 0.0F, 1.0F } };

        vk::RenderingInfo info {};
        info.renderArea = vk::Rect2D { { 0, 0 }, { k_extent, k_extent } };
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = &attachment;

        cmd.beginRendering(info);
        pipeline->bind(cmd);
        pipeline->set_viewport(cmd, vk::Viewport { 0.0F, 0.0F, static_cast<float>(k_extent), static_cast<float>(k_extent), 0.0F, 1.0F });
        pipeline->set_scissor(cmd, vk::Rect2D { { 0, 0 }, { k_extent, k_extent } });
        pipeline->push_constants(cmd, vk::ShaderStageFlagBits::eFragment, 0, sizeof(color), color.data());
        pipeline->draw(cmd, 3);
        cmd.endRendering();
    });
    image->set_current_layout(vk::ImageLayout::eColorAttachmentOptimal);

    std::vector<uint8_t> pixels(static_cast<size_t>(k_extent) * k_extent * 4, 0);
    resources.download_image_data(image, pixels.data(), pixels.size());

    for (size_t i = 0; i < pixels.size(); i += 4) {
        ASSERT_EQ(pixels[i], 255U) << "pixel " << i / 4;
        ASSERT_EQ(pixels[i + 1], 0U) << "pixel " << i / 4;
        ASSERT_NEAR(pixels[i + 2], 128, 1) << "pixel " << i / 4;
        ASSERT_EQ(pixels[i + 3], 255U) << "pixel " << i / 4;
    }
    resources.cleanup_image(image);
}

}
