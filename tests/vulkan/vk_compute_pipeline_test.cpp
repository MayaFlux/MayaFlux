#include "gtest/gtest.h"

#include <cstring>

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKComputePipeline.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::ComputePipelineConfig;
    using Core::VKComputePipeline;
    using Core::VKDescriptorManager;
    using Core::VKShaderModule;

    constexpr const char* k_scale_source = R"(#version 450
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) buffer InBuf { float values[]; } in_buf;
layout(set = 0, binding = 1) buffer OutBuf { float values[]; } out_buf;
layout(push_constant) uniform Params { uint count; float scale; } params;
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i < params.count) {
        out_buf.values[i] = in_buf.values[i] * params.scale;
    }
}
)";

    constexpr const char* k_specialized_source = R"(#version 450
layout(local_size_x = 1) in;
layout(constant_id = 7) const uint k_value = 3;
layout(set = 0, binding = 0) buffer Out { uint v[]; } out_buf;
void main() { out_buf.v[0] = k_value; }
)";

    constexpr const char* k_vertex_source = R"(#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
)";

    struct ScaleParams {
        uint32_t count;
        float scale;
    };

    class VKComputePipelineTest : public VulkanDeviceTest {
    protected:
        void SetUp() override
        {
            VulkanDeviceTest::SetUp();
            if (IsSkipped()) {
                return;
            }
            ready = descriptors.initialize(device(), 8);
        }

        void TearDown() override
        {
            if (!ready) {
                return;
            }
            for (auto& pipeline : pipelines) {
                pipeline->cleanup(device());
            }
            for (auto& shader : shaders) {
                shader->cleanup(device());
            }
            descriptors.cleanup(device());
        }

        static vk::Device device() { return backend().get_context().get_device(); }

        std::shared_ptr<VKShaderModule> compile(const char* source,
            vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eCompute)
        {
            auto module = std::make_shared<VKShaderModule>();
            if (!module->create_from_glsl(device(), source, stage)) {
                return nullptr;
            }
            shaders.push_back(module);
            return module;
        }

        std::shared_ptr<VKComputePipeline> make_pipeline(
            const std::shared_ptr<VKShaderModule>& shader,
            vk::DescriptorSetLayout layout,
            uint32_t push_size = 0)
        {
            ComputePipelineConfig config;
            config.shader = shader;
            config.add_descriptor_set_layout(layout);
            if (push_size > 0) {
                config.add_push_constant(vk::ShaderStageFlagBits::eCompute, push_size);
            }

            auto pipeline = std::make_shared<VKComputePipeline>();
            pipelines.push_back(pipeline);
            return pipeline->create(device(), config) ? pipeline : nullptr;
        }

        vk::DescriptorSetLayout storage_layout(uint32_t bindings)
        {
            Core::DescriptorSetLayoutConfig config;
            for (uint32_t i = 0; i < bindings; ++i) {
                config.add_storage_buffer(i);
            }
            return descriptors.create_layout(device(), config);
        }

        vk::DescriptorSet bind_buffers(vk::DescriptorSetLayout layout,
            std::initializer_list<vk::Buffer> buffers)
        {
            const auto set = descriptors.allocate_set(device(), layout);
            uint32_t binding = 0;
            for (auto buffer : buffers) {
                descriptors.update_buffer(device(), set, binding++, buffer);
            }
            return set;
        }

        template <typename Recorder>
        void run(VKComputePipeline& pipeline, vk::DescriptorSet set, const Recorder& record)
        {
            auto& commands = backend().get_command_manager();
            auto cmd = commands.begin_single_time_commands();
            pipeline.bind(cmd);
            pipeline.bind_descriptor_sets(cmd, { set });
            record(cmd);
            commands.end_single_time_commands(cmd, backend().get_context().get_graphics_queue());
        }

        static std::unique_ptr<RawVulkanBuffer> storage_buffer(size_t bytes)
        {
            return std::make_unique<RawVulkanBuffer>(backend(), bytes,
                vk::BufferUsageFlagBits::eStorageBuffer, true);
        }

        VKDescriptorManager descriptors;
        std::vector<std::shared_ptr<VKShaderModule>> shaders;
        std::vector<std::shared_ptr<VKComputePipeline>> pipelines;
        bool ready {};
    };

}

TEST(VKComputePipelineDispatchMathTest, DispatchSizesRoundUpToWholeWorkgroups)
{
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_1d(1000, 64), (std::array<uint32_t, 3> { 16, 1, 1 }));
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_1d(1024, 64), (std::array<uint32_t, 3> { 16, 1, 1 }));
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_1d(1025, 64), (std::array<uint32_t, 3> { 17, 1, 1 }));
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_2d(100, 50, 16, 16), (std::array<uint32_t, 3> { 7, 4, 1 }));
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_3d(10, 10, 10, 4, 4, 4), (std::array<uint32_t, 3> { 3, 3, 3 }));
}

TEST(VKComputePipelineDispatchMathTest, EmptyWorkOrZeroWorkgroupSizeYieldsNoGroups)
{
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_1d(0, 64)[0], 0U);
    EXPECT_EQ(VKComputePipeline::calculate_dispatch_1d(10, 0)[0], 0U);
}

TEST(VKComputePipelineUninitializedTest, DefaultPipelineIsInvalidAndIgnoresCommands)
{
    VKComputePipeline pipeline;

    EXPECT_FALSE(pipeline.is_valid());
    EXPECT_FALSE(pipeline.get());
    EXPECT_FALSE(pipeline.get_layout());
    EXPECT_FALSE(pipeline.get_workgroup_size().has_value());
    EXPECT_NO_THROW(pipeline.bind(vk::CommandBuffer {}));
    EXPECT_NO_THROW(pipeline.dispatch(vk::CommandBuffer {}, 1, 1, 1));
    EXPECT_NO_THROW(pipeline.dispatch_indirect(vk::CommandBuffer {}, vk::Buffer {}));
}

TEST(VKComputePipelineUninitializedTest, CreateWithoutShaderFails)
{
    VKComputePipeline pipeline;
    EXPECT_FALSE(pipeline.create(vk::Device {}, ComputePipelineConfig {}));
    EXPECT_FALSE(pipeline.is_valid());
}

TEST(VKComputePipelineUninitializedTest, CreateSpecializedWithoutShaderFails)
{
    VKComputePipeline pipeline;
    EXPECT_FALSE(pipeline.create_specialized(vk::Device {}, ComputePipelineConfig {}, { { 0U, 1U } }));
}

TEST_F(VKComputePipelineTest, CreateRejectsAnInvalidShaderModule)
{
    ComputePipelineConfig config;
    config.shader = std::make_shared<VKShaderModule>();

    auto pipeline = std::make_shared<VKComputePipeline>();
    EXPECT_FALSE(pipeline->create(device(), config));
    EXPECT_FALSE(pipeline->is_valid());
}

TEST_F(VKComputePipelineTest, CreateRejectsANonComputeShader)
{
    auto vertex = compile(k_vertex_source, vk::ShaderStageFlagBits::eVertex);
    ASSERT_NE(vertex, nullptr);

    ComputePipelineConfig config;
    config.shader = vertex;
    auto pipeline = std::make_shared<VKComputePipeline>();

    EXPECT_FALSE(pipeline->create(device(), config));
}

TEST_F(VKComputePipelineTest, CreatedPipelineExposesHandlesAndWorkgroupSize)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    auto pipeline = make_pipeline(shader, storage_layout(2), sizeof(ScaleParams));

    ASSERT_NE(pipeline, nullptr);
    EXPECT_TRUE(pipeline->is_valid());
    EXPECT_TRUE(pipeline->get());
    EXPECT_TRUE(pipeline->get_layout());
    ASSERT_TRUE(pipeline->get_workgroup_size().has_value());
    EXPECT_EQ((*pipeline->get_workgroup_size())[0], 64U);
    EXPECT_EQ((*pipeline->get_workgroup_size())[1], 1U);
}

TEST_F(VKComputePipelineTest, ReflectionOfTheAttachedShaderIsAvailable)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    auto pipeline = make_pipeline(shader, storage_layout(2), sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    EXPECT_EQ(pipeline->get_shader_reflection().bindings.size(), 2U);
}

TEST_F(VKComputePipelineTest, DispatchWritesScaledValuesThroughStorageBuffersAndPushConstants)
{
    constexpr uint32_t k_count = 1000;
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    const auto layout = storage_layout(2);
    auto pipeline = make_pipeline(shader, layout, sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    auto in = storage_buffer(k_count * sizeof(float));
    auto out = storage_buffer(k_count * sizeof(float));
    auto* in_values = static_cast<float*>(in->mapped());
    auto* out_values = static_cast<float*>(out->mapped());
    for (uint32_t i = 0; i < k_count; ++i) {
        in_values[i] = static_cast<float>(i);
        out_values[i] = -1.0F;
    }
    const auto set = bind_buffers(layout, { in->buffer(), out->buffer() });
    const ScaleParams params { k_count, 2.5F };

    run(*pipeline, set, [&](vk::CommandBuffer cmd) {
        pipeline->push_constants(cmd, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params);
        pipeline->dispatch_1d(cmd, k_count, 64);
    });

    for (uint32_t i = 0; i < k_count; ++i) {
        ASSERT_FLOAT_EQ(out_values[i], static_cast<float>(i) * 2.5F) << "element " << i;
    }
}

TEST_F(VKComputePipelineTest, ThreadsPastTheCountDoNotWrite)
{
    constexpr uint32_t k_count = 70;
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    const auto layout = storage_layout(2);
    auto pipeline = make_pipeline(shader, layout, sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    auto in = storage_buffer(128 * sizeof(float));
    auto out = storage_buffer(128 * sizeof(float));
    auto* in_values = static_cast<float*>(in->mapped());
    auto* out_values = static_cast<float*>(out->mapped());
    for (uint32_t i = 0; i < 128; ++i) {
        in_values[i] = 1.0F;
        out_values[i] = -1.0F;
    }
    const auto set = bind_buffers(layout, { in->buffer(), out->buffer() });
    const ScaleParams params { k_count, 3.0F };

    run(*pipeline, set, [&](vk::CommandBuffer cmd) {
        pipeline->push_constants(cmd, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params);
        pipeline->dispatch(cmd, 2, 1, 1);
    });

    EXPECT_FLOAT_EQ(out_values[k_count - 1], 3.0F);
    EXPECT_FLOAT_EQ(out_values[k_count], -1.0F);
    EXPECT_FLOAT_EQ(out_values[127], -1.0F);
}

TEST_F(VKComputePipelineTest, ZeroGroupDispatchLeavesOutputUntouched)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    const auto layout = storage_layout(2);
    auto pipeline = make_pipeline(shader, layout, sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    auto in = storage_buffer(64 * sizeof(float));
    auto out = storage_buffer(64 * sizeof(float));
    auto* out_values = static_cast<float*>(out->mapped());
    std::memset(in->mapped(), 0, 64 * sizeof(float));
    for (uint32_t i = 0; i < 64; ++i) {
        out_values[i] = -1.0F;
    }
    const auto set = bind_buffers(layout, { in->buffer(), out->buffer() });
    const ScaleParams params { 64, 5.0F };

    run(*pipeline, set, [&](vk::CommandBuffer cmd) {
        pipeline->push_constants(cmd, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params);
        pipeline->dispatch(cmd, 0, 1, 1);
        pipeline->dispatch_1d(cmd, 64, 0);
    });

    EXPECT_FLOAT_EQ(out_values[0], -1.0F);
}

TEST_F(VKComputePipelineTest, RejectedBindingAndPushCallsDoNotTouchTheCommandBuffer)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    auto pipeline = make_pipeline(shader, storage_layout(2), sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    EXPECT_NO_THROW(pipeline->bind_descriptor_sets(vk::CommandBuffer {}, {}));
    EXPECT_NO_THROW(pipeline->bind_descriptor_sets(vk::CommandBuffer {}, { vk::DescriptorSet {} }));
    EXPECT_NO_THROW(pipeline->push_constants(vk::CommandBuffer {}, vk::ShaderStageFlagBits::eCompute, 0, 8, nullptr));
    const ScaleParams params { 1, 1.0F };
    EXPECT_NO_THROW(pipeline->push_constants(vk::CommandBuffer {}, vk::ShaderStageFlagBits::eCompute, 0, 0, &params));
}

TEST_F(VKComputePipelineTest, IndirectDispatchReadsGroupCountsFromABuffer)
{
    constexpr uint32_t k_count = 1000;
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    const auto layout = storage_layout(2);
    auto pipeline = make_pipeline(shader, layout, sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    auto in = storage_buffer(k_count * sizeof(float));
    auto out = storage_buffer(k_count * sizeof(float));
    RawVulkanBuffer indirect(backend(), sizeof(vk::DispatchIndirectCommand),
        vk::BufferUsageFlagBits::eIndirectBuffer, true);
    auto* in_values = static_cast<float*>(in->mapped());
    auto* out_values = static_cast<float*>(out->mapped());
    for (uint32_t i = 0; i < k_count; ++i) {
        in_values[i] = 4.0F;
        out_values[i] = -1.0F;
    }
    const vk::DispatchIndirectCommand groups { 16, 1, 1 };
    std::memcpy(indirect.mapped(), &groups, sizeof(groups));
    const auto set = bind_buffers(layout, { in->buffer(), out->buffer() });
    const ScaleParams params { k_count, 0.5F };

    run(*pipeline, set, [&](vk::CommandBuffer cmd) {
        pipeline->push_constants(cmd, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params);
        pipeline->dispatch_indirect(cmd, indirect.buffer(), 0);
    });

    EXPECT_FLOAT_EQ(out_values[0], 2.0F);
    EXPECT_FLOAT_EQ(out_values[k_count - 1], 2.0F);
}

TEST_F(VKComputePipelineTest, SpecializationConstantsChangeWhatTheShaderComputes)
{
    const auto layout = storage_layout(1);
    auto defaulted = compile(k_specialized_source);
    auto specialized = compile(k_specialized_source);
    ASSERT_NE(defaulted, nullptr);
    ASSERT_NE(specialized, nullptr);

    auto default_pipeline = make_pipeline(defaulted, layout);
    ASSERT_NE(default_pipeline, nullptr);

    ComputePipelineConfig config;
    config.shader = specialized;
    config.add_descriptor_set_layout(layout);
    auto specialized_pipeline = std::make_shared<VKComputePipeline>();
    pipelines.push_back(specialized_pipeline);
    ASSERT_TRUE(specialized_pipeline->create_specialized(device(), config, { { 7U, 42U } }));

    auto out_default = storage_buffer(sizeof(uint32_t));
    auto out_specialized = storage_buffer(sizeof(uint32_t));
    std::memset(out_default->mapped(), 0, sizeof(uint32_t));
    std::memset(out_specialized->mapped(), 0, sizeof(uint32_t));

    run(*default_pipeline, bind_buffers(layout, { out_default->buffer() }),
        [&](vk::CommandBuffer cmd) { default_pipeline->dispatch(cmd, 1, 1, 1); });
    run(*specialized_pipeline, bind_buffers(layout, { out_specialized->buffer() }),
        [&](vk::CommandBuffer cmd) { specialized_pipeline->dispatch(cmd, 1, 1, 1); });

    EXPECT_EQ(*static_cast<uint32_t*>(out_default->mapped()), 3U);
    EXPECT_EQ(*static_cast<uint32_t*>(out_specialized->mapped()), 42U);
}

TEST_F(VKComputePipelineTest, MovedPipelineKeepsHandlesWorkgroupSizeAndReflection)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    auto source = make_pipeline(shader, storage_layout(2), sizeof(ScaleParams));
    ASSERT_NE(source, nullptr);

    VKComputePipeline moved(std::move(*source));

    EXPECT_TRUE(moved.is_valid());
    EXPECT_FALSE(source->is_valid());
    ASSERT_TRUE(moved.get_workgroup_size().has_value());
    EXPECT_EQ(moved.get_shader_reflection().bindings.size(), 2U);
    moved.cleanup(device());
}

TEST_F(VKComputePipelineTest, CleanupInvalidatesThePipelineAndIsRepeatable)
{
    auto shader = compile(k_scale_source);
    ASSERT_NE(shader, nullptr);
    auto pipeline = make_pipeline(shader, storage_layout(2), sizeof(ScaleParams));
    ASSERT_NE(pipeline, nullptr);

    pipeline->cleanup(device());

    EXPECT_FALSE(pipeline->is_valid());
    EXPECT_FALSE(pipeline->get_workgroup_size().has_value());
    EXPECT_NO_THROW(pipeline->cleanup(device()));
}

}
