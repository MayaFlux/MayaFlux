#include "gtest/gtest.h"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDescriptorManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::DescriptorSetLayoutConfig;
    using Core::GraphicsPipelineConfig;

}

TEST(GraphicsPipelineConfigTest, DefaultsAreTriangleListWithDynamicViewport)
{
    GraphicsPipelineConfig config;

    EXPECT_EQ(config.topology, vk::PrimitiveTopology::eTriangleList);
    EXPECT_EQ(config.polygon_mode, vk::PolygonMode::eFill);
    EXPECT_TRUE(config.dynamic_viewport);
    EXPECT_TRUE(config.dynamic_scissor);
    ASSERT_EQ(config.dynamic_states.size(), 2U);
    EXPECT_EQ(config.dynamic_states[0], vk::DynamicState::eViewport);
    EXPECT_EQ(config.dynamic_states[1], vk::DynamicState::eScissor);
    EXPECT_EQ(config.rasterization_samples, vk::SampleCountFlagBits::e1);
    EXPECT_TRUE(config.color_blend_attachments.empty());
    EXPECT_EQ(config.vertex_shader, nullptr);
}

TEST(GraphicsPipelineConfigTest, Default3dCullsBackFacesAndDepthTests)
{
    const auto config = GraphicsPipelineConfig::default_3d();

    EXPECT_EQ(config.cull_mode, vk::CullModeFlagBits::eBack);
    EXPECT_EQ(config.front_face, vk::FrontFace::eCounterClockwise);
    EXPECT_TRUE(config.depth_test_enable);
    EXPECT_TRUE(config.depth_write_enable);
    EXPECT_EQ(config.depth_compare_op, vk::CompareOp::eLess);
    ASSERT_EQ(config.color_blend_attachments.size(), 1U);
    EXPECT_FALSE(config.color_blend_attachments[0].blend_enable);
}

TEST(GraphicsPipelineConfigTest, Default2dHasNoCullingAndNoDepth)
{
    const auto config = GraphicsPipelineConfig::default_2d();

    EXPECT_EQ(config.cull_mode, vk::CullModeFlagBits::eNone);
    EXPECT_FALSE(config.depth_test_enable);
    EXPECT_FALSE(config.depth_write_enable);
    ASSERT_EQ(config.color_blend_attachments.size(), 1U);
    EXPECT_FALSE(config.color_blend_attachments[0].blend_enable);
}

TEST(GraphicsPipelineConfigTest, AlphaBlendedKeepsDepthAndBlendsSourceAlpha)
{
    const auto config = GraphicsPipelineConfig::alpha_blended();

    EXPECT_TRUE(config.depth_test_enable);
    ASSERT_EQ(config.color_blend_attachments.size(), 1U);
    const auto& attachment = config.color_blend_attachments[0];
    EXPECT_TRUE(attachment.blend_enable);
    EXPECT_EQ(attachment.src_color_blend_factor, vk::BlendFactor::eSrcAlpha);
    EXPECT_EQ(attachment.dst_color_blend_factor, vk::BlendFactor::eOneMinusSrcAlpha);
    EXPECT_EQ(attachment.color_blend_op, vk::BlendOp::eAdd);
    EXPECT_EQ(attachment.src_alpha_blend_factor, vk::BlendFactor::eOne);
    EXPECT_EQ(attachment.dst_alpha_blend_factor, vk::BlendFactor::eZero);
}

TEST(GraphicsPipelineConfigTest, AdditiveBlendedAccumulatesDestination)
{
    const auto config = GraphicsPipelineConfig::additive_blended();

    ASSERT_EQ(config.color_blend_attachments.size(), 1U);
    const auto& attachment = config.color_blend_attachments[0];
    EXPECT_TRUE(attachment.blend_enable);
    EXPECT_EQ(attachment.src_color_blend_factor, vk::BlendFactor::eSrcAlpha);
    EXPECT_EQ(attachment.dst_color_blend_factor, vk::BlendFactor::eOne);
}

TEST(GraphicsPipelineConfigTest, DepthOnlyHasNoColorAttachments)
{
    const auto config = GraphicsPipelineConfig::depth_only();

    EXPECT_TRUE(config.color_blend_attachments.empty());
    EXPECT_TRUE(config.depth_test_enable);
    EXPECT_TRUE(config.depth_write_enable);
    EXPECT_FALSE(config.rasterizer_discard_enable);
}

TEST(GraphicsPipelineConfigTest, EnableBlendingCreatesAttachmentWhenNoneExist)
{
    GraphicsPipelineConfig config;
    config.enable_alpha_blending();
    ASSERT_EQ(config.color_blend_attachments.size(), 1U);
    EXPECT_TRUE(config.color_blend_attachments[0].blend_enable);

    GraphicsPipelineConfig additive;
    additive.enable_additive_blending();
    ASSERT_EQ(additive.color_blend_attachments.size(), 1U);
    EXPECT_TRUE(additive.color_blend_attachments[0].blend_enable);
}

TEST(GraphicsPipelineConfigTest, EnableBlendingAppliesToEveryAttachment)
{
    GraphicsPipelineConfig config;
    config.color_blend_attachments.resize(3);

    config.enable_alpha_blending();

    ASSERT_EQ(config.color_blend_attachments.size(), 3U);
    for (const auto& attachment : config.color_blend_attachments) {
        EXPECT_TRUE(attachment.blend_enable);
        EXPECT_EQ(attachment.dst_color_blend_factor, vk::BlendFactor::eOneMinusSrcAlpha);
    }
}

TEST(GraphicsPipelineConfigTest, SwitchingBlendModeOverwritesDestinationFactor)
{
    auto config = GraphicsPipelineConfig::additive_blended();
    config.enable_alpha_blending();

    EXPECT_EQ(config.color_blend_attachments[0].dst_color_blend_factor,
        vk::BlendFactor::eOneMinusSrcAlpha);
}

TEST(GraphicsPipelineConfigTest, DisableDepthTestDisablesWritesToo)
{
    auto config = GraphicsPipelineConfig::default_3d();
    config.disable_depth_test();

    EXPECT_FALSE(config.depth_test_enable);
    EXPECT_FALSE(config.depth_write_enable);
}

TEST(GraphicsPipelineConfigTest, RasterizationToggles)
{
    GraphicsPipelineConfig config;

    config.enable_wireframe();
    EXPECT_EQ(config.polygon_mode, vk::PolygonMode::eLine);

    config.disable_culling();
    EXPECT_EQ(config.cull_mode, vk::CullModeFlagBits::eNone);

    config.enable_back_face_culling();
    EXPECT_EQ(config.cull_mode, vk::CullModeFlagBits::eBack);
}

TEST(DescriptorSetLayoutConfigTest, StartsEmpty)
{
    DescriptorSetLayoutConfig config;
    EXPECT_TRUE(config.bindings.empty());
}

TEST(DescriptorSetLayoutConfigTest, AddBindingRecordsEveryField)
{
    DescriptorSetLayoutConfig config;
    config.add_binding(3, vk::DescriptorType::eUniformBuffer,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 4);

    ASSERT_EQ(config.bindings.size(), 1U);
    EXPECT_EQ(config.bindings[0].binding, 3U);
    EXPECT_EQ(config.bindings[0].type, vk::DescriptorType::eUniformBuffer);
    EXPECT_EQ(config.bindings[0].count, 4U);
    EXPECT_EQ(config.bindings[0].stage_flags,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
}

TEST(DescriptorSetLayoutConfigTest, AddBindingDefaultsToSingleDescriptor)
{
    DescriptorSetLayoutConfig config;
    config.add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(config.bindings[0].count, 1U);
}

TEST(DescriptorSetLayoutConfigTest, ConvenienceHelpersMapToDescriptorTypesAndComputeStage)
{
    DescriptorSetLayoutConfig config;
    config.add_storage_buffer(0);
    config.add_uniform_buffer(1);
    config.add_storage_image(2);
    config.add_sampled_image(3);

    ASSERT_EQ(config.bindings.size(), 4U);
    EXPECT_EQ(config.bindings[0].type, vk::DescriptorType::eStorageBuffer);
    EXPECT_EQ(config.bindings[1].type, vk::DescriptorType::eUniformBuffer);
    EXPECT_EQ(config.bindings[2].type, vk::DescriptorType::eStorageImage);
    EXPECT_EQ(config.bindings[3].type, vk::DescriptorType::eCombinedImageSampler);

    for (uint32_t i = 0; i < config.bindings.size(); ++i) {
        EXPECT_EQ(config.bindings[i].binding, i);
        EXPECT_EQ(config.bindings[i].stage_flags, vk::ShaderStageFlagBits::eCompute);
    }
}

TEST(DescriptorSetLayoutConfigTest, HelpersAcceptExplicitStages)
{
    DescriptorSetLayoutConfig config;
    config.add_uniform_buffer(0, vk::ShaderStageFlagBits::eFragment);
    EXPECT_EQ(config.bindings[0].stage_flags, vk::ShaderStageFlagBits::eFragment);
}

TEST(DescriptorSetLayoutConfigTest, BindingsKeepInsertionOrder)
{
    DescriptorSetLayoutConfig config;
    config.add_storage_buffer(5);
    config.add_storage_buffer(1);
    config.add_storage_buffer(3);

    ASSERT_EQ(config.bindings.size(), 3U);
    EXPECT_EQ(config.bindings[0].binding, 5U);
    EXPECT_EQ(config.bindings[1].binding, 1U);
    EXPECT_EQ(config.bindings[2].binding, 3U);
}

}
