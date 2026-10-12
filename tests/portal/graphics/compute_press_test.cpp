#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Portal/Graphics/ComputePress.hpp"

namespace MayaFlux::Test {
namespace {

    using Portal::Graphics::ComputeStage;
    using Portal::Graphics::DescriptorBindingInfo;
    using Portal::Graphics::GpuBufferBinding;
    using Portal::Graphics::HazardResource;
    using Portal::Graphics::INVALID_COMPUTE_PIPELINE;
    using Portal::Graphics::INVALID_SHADER;
    using Portal::Graphics::ShaderID;
    using Portal::Graphics::ShaderStage;

    using CommandType = Portal::Graphics::ShaderFoundry::CommandBufferType;

    constexpr uint32_t k_words = 16;
    constexpr size_t k_bytes = k_words * sizeof(uint32_t);

    constexpr std::string_view k_compute_source = R"(#version 450
layout(local_size_x = 16) in;
layout(set = 0, binding = 0, std430) buffer Data { uint values[]; };
layout(push_constant) uniform PC { uint factor; } pc;
void main()
{
    values[gl_GlobalInvocationID.x] *= pc.factor;
}
)";

    constexpr std::string_view k_affine_source = R"(#version 450
layout(local_size_x = 16) in;
layout(set = 0, binding = 0, std430) buffer Data { uint values[]; };
layout(push_constant) uniform PC { uint scale; uint bias; } pc;
void main()
{
    values[gl_GlobalInvocationID.x] = values[gl_GlobalInvocationID.x] * pc.scale + pc.bias;
}
)";

    constexpr std::string_view k_vertex_source = R"(#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
)";

    Portal::Graphics::ComputePress& press()
    {
        return Portal::Graphics::get_compute_press();
    }

    Portal::Graphics::ShaderFoundry& foundry()
    {
        return Portal::Graphics::get_shader_foundry();
    }

    class ComputePressTest : public ::testing::Test {
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
            ASSERT_TRUE(press().initialize());
        }

        void TearDown() override
        {
            teardown_portal();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

        static void teardown_portal()
        {
            press().shutdown();
            foundry().shutdown();
        }

        static ShaderID compute_shader() { return foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE); }

        static std::vector<std::vector<DescriptorBindingInfo>> one_storage_binding()
        {
            return { { DescriptorBindingInfo { .set = 0, .binding = 0, .type = vk::DescriptorType::eStorageBuffer } } };
        }

        static std::unique_ptr<RawVulkanBuffer> storage_buffer()
        {
            return std::make_unique<RawVulkanBuffer>(*s_backend, k_bytes,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer
                    | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
                true);
        }

        static void fill_ascending(const RawVulkanBuffer& buffer)
        {
            std::vector<uint32_t> words(k_words);
            std::iota(words.begin(), words.end(), 1U);
            std::memcpy(buffer.mapped(), words.data(), k_bytes);
        }

        static std::vector<uint32_t> words(const RawVulkanBuffer& buffer)
        {
            std::vector<uint32_t> out(k_words);
            std::memcpy(out.data(), buffer.mapped(), k_bytes);
            return out;
        }

        static std::vector<uint32_t> ascending_times(uint32_t factor)
        {
            std::vector<uint32_t> expected(k_words);
            std::iota(expected.begin(), expected.end(), 1U);
            for (auto& value : expected) {
                value *= factor;
            }
            return expected;
        }

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST_F(ComputePressTest, InitializeNeedsAnInitializedShaderFoundry)
    {
        press().shutdown();
        foundry().shutdown();

        EXPECT_FALSE(press().initialize());
        EXPECT_FALSE(press().is_initialized());
        EXPECT_EQ(press().create_pipeline(1), INVALID_COMPUTE_PIPELINE);
    }

    TEST_F(ComputePressTest, InitializingAgainReportsSuccess)
    {
        EXPECT_TRUE(press().initialize());
        EXPECT_TRUE(press().is_initialized());
    }

    TEST_F(ComputePressTest, ShutdownIsRepeatableAndThePressInitializesAgain)
    {
        press().shutdown();

        EXPECT_NO_THROW(press().shutdown());
        EXPECT_FALSE(press().is_initialized());
        EXPECT_TRUE(press().initialize());
        EXPECT_NE(press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t)), INVALID_COMPUTE_PIPELINE);
    }

    // ------------------------------------------------------------------
    // Pipelines
    // ------------------------------------------------------------------

    TEST_F(ComputePressTest, UnknownShaderIdsAndNonComputeShadersAreRefused)
    {
        const ShaderID vertex = foundry().load_shader(std::string(k_vertex_source), ShaderStage::VERTEX);
        ASSERT_NE(vertex, INVALID_SHADER);

        EXPECT_EQ(press().create_pipeline(9999), INVALID_COMPUTE_PIPELINE);
        EXPECT_EQ(press().create_pipeline(vertex), INVALID_COMPUTE_PIPELINE);
        EXPECT_EQ(press().create_pipeline_auto(9999), INVALID_COMPUTE_PIPELINE);
    }

    TEST_F(ComputePressTest, ComputeShaderCreatesPipelinesWithDistinctIds)
    {
        const ShaderID shader = compute_shader();
        ASSERT_NE(shader, INVALID_SHADER);

        const auto first = press().create_pipeline(shader, one_storage_binding(), sizeof(uint32_t));
        const auto second = press().create_pipeline(shader, one_storage_binding(), sizeof(uint32_t));

        EXPECT_NE(first, INVALID_COMPUTE_PIPELINE);
        EXPECT_NE(second, INVALID_COMPUTE_PIPELINE);
        EXPECT_NE(first, second);
    }

    TEST_F(ComputePressTest, AutoPipelineTakesItsLayoutAndPushConstantsFromReflection)
    {
        const auto id = press().create_pipeline_auto(compute_shader());
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);

        const auto sets = press().allocate_pipeline_descriptors(id);

        ASSERT_EQ(sets.size(), 1U);
        EXPECT_TRUE(foundry().get_descriptor_set(sets[0]));
    }

    TEST_F(ComputePressTest, OneSetIsAllocatedPerLayoutAndAnUnknownPipelineGivesNothing)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);

        EXPECT_EQ(press().allocate_pipeline_descriptors(id).size(), 1U);
        EXPECT_TRUE(press().allocate_pipeline_descriptors(9999).empty());
    }

    TEST_F(ComputePressTest, DestroyPipelineIgnoresUnknownIdsAndRepeats)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);

        press().destroy_pipeline(id);

        EXPECT_TRUE(press().allocate_pipeline_descriptors(id).empty());
        EXPECT_NO_THROW(press().destroy_pipeline(id));
        EXPECT_NO_THROW(press().destroy_pipeline(9999));
    }

    // ------------------------------------------------------------------
    // Recording and dispatch
    // ------------------------------------------------------------------

    TEST_F(ComputePressTest, BindAllAndDispatchRunTheKernelOverTheBoundBuffer)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto sets = press().allocate_pipeline_descriptors(id);
        ASSERT_EQ(sets.size(), 1U);
        const auto buffer = storage_buffer();
        fill_ascending(*buffer);
        foundry().update_descriptor_buffer(sets[0], 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes);
        const uint32_t factor = 3;

        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        press().bind_all(cmd, id, sets, &factor, sizeof(factor));
        press().dispatch(cmd, 1, 1, 1);
        foundry().submit_and_wait(cmd);

        EXPECT_EQ(words(*buffer), ascending_times(3));
    }

    TEST_F(ComputePressTest, IndirectDispatchReadsItsGroupCountFromABuffer)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto sets = press().allocate_pipeline_descriptors(id);
        const auto data = storage_buffer();
        fill_ascending(*data);
        foundry().update_descriptor_buffer(sets.at(0), 0, vk::DescriptorType::eStorageBuffer, data->buffer(), 0, k_bytes);
        const auto indirect = storage_buffer();
        const std::array<uint32_t, 3> groups { 1, 1, 1 };
        std::memcpy(indirect->mapped(), groups.data(), sizeof(groups));
        const uint32_t factor = 5;

        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        press().bind_all(cmd, id, sets, &factor, sizeof(factor));
        press().dispatch_indirect(cmd, indirect->buffer(), 0);
        foundry().submit_and_wait(cmd);

        EXPECT_EQ(words(*data), ascending_times(5));
    }

    TEST_F(ComputePressTest, RecordSequenceRunsEveryStageInOrderWithBarriersBetweenThem)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto sets = press().allocate_pipeline_descriptors(id);
        const auto buffer = storage_buffer();
        fill_ascending(*buffer);
        foundry().update_descriptor_buffer(sets.at(0), 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes);

        HazardResource hazard;
        hazard.binding.direction = GpuBufferBinding::Direction::INPUT_OUTPUT;
        hazard.binding.element_type = GpuBufferBinding::ElementType::UINT32;
        hazard.buffer = buffer->buffer();

        auto stage_with = [&](uint32_t factor) {
            ComputeStage stage {};
            stage.pipeline_id = id;
            stage.descriptor_set_ids = sets;
            stage.groups = { 1, 1, 1 };
            stage.push_constant_data.resize(sizeof(factor));
            std::memcpy(stage.push_constant_data.data(), &factor, sizeof(factor));
            stage.hazard_resources.push_back(hazard);
            return stage;
        };

        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        press().record_sequence(cmd, { stage_with(2), stage_with(3) });
        foundry().submit_and_wait(cmd);

        EXPECT_EQ(words(*buffer), ascending_times(6));
    }

    TEST_F(ComputePressTest, PushingAtAnOffsetReplacesOnlyThatPartOfTheBlock)
    {
        const ShaderID shader = foundry().load_shader(std::string(k_affine_source), ShaderStage::COMPUTE);
        ASSERT_NE(shader, INVALID_SHADER);
        const auto id = press().create_pipeline(shader, one_storage_binding(), 2 * sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto sets = press().allocate_pipeline_descriptors(id);
        ASSERT_EQ(sets.size(), 1U);
        const auto buffer = storage_buffer();
        fill_ascending(*buffer);
        foundry().update_descriptor_buffer(sets[0], 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes);
        const std::array<uint32_t, 2> block { 2U, 100U };
        const uint32_t bias = 7;

        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        press().bind_all(cmd, id, sets, block.data(), sizeof(block));
        press().push_constants(cmd, id, &bias, sizeof(bias), sizeof(uint32_t));
        press().dispatch(cmd, 1, 1, 1);
        foundry().submit_and_wait(cmd);

        std::vector<uint32_t> expected = ascending_times(2);
        for (auto& value : expected) {
            value += 7U;
        }
        EXPECT_EQ(words(*buffer), expected);
    }

    TEST_F(ComputePressTest, PushingBeyondTheDeclaredBlockIsRefused)
    {
        const ShaderID shader = foundry().load_shader(std::string(k_affine_source), ShaderStage::COMPUTE);
        ASSERT_NE(shader, INVALID_SHADER);
        const auto id = press().create_pipeline(shader, one_storage_binding(), 2 * sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto sets = press().allocate_pipeline_descriptors(id);
        ASSERT_EQ(sets.size(), 1U);
        const auto buffer = storage_buffer();
        fill_ascending(*buffer);
        foundry().update_descriptor_buffer(sets[0], 0, vk::DescriptorType::eStorageBuffer, buffer->buffer(), 0, k_bytes);
        const std::array<uint32_t, 2> block { 2U, 100U };
        const uint32_t stray = 9;

        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        press().bind_all(cmd, id, sets, block.data(), sizeof(block));
        press().push_constants(cmd, id, &stray, sizeof(stray), 2 * sizeof(uint32_t));
        press().push_constants(cmd, id, block.data(), sizeof(block), sizeof(uint32_t));
        press().push_constants(9999, id, &stray, sizeof(stray), 0);
        press().dispatch(cmd, 1, 1, 1);
        foundry().submit_and_wait(cmd);

        std::vector<uint32_t> expected = ascending_times(2);
        for (auto& value : expected) {
            value += 100U;
        }
        EXPECT_EQ(words(*buffer), expected);
    }

    TEST_F(ComputePressTest, PushingAtAnOffsetOnAnUnknownPipelineIsIgnored)
    {
        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        const uint32_t value = 1;

        EXPECT_NO_THROW(press().push_constants(cmd, 9999, &value, sizeof(value), 0));

        foundry().submit_and_wait(cmd);
    }

    TEST_F(ComputePressTest, RecordingWithUnknownIdsIsIgnored)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);
        const uint32_t factor = 1;

        EXPECT_NO_THROW(press().bind_pipeline(cmd, 9999));
        EXPECT_NO_THROW(press().bind_pipeline(9999, id));
        EXPECT_NO_THROW(press().bind_descriptor_sets(cmd, 9999, {}));
        EXPECT_NO_THROW(press().push_constants(cmd, 9999, &factor, sizeof(factor)));
        EXPECT_NO_THROW(press().push_constants(9999, id, &factor, sizeof(factor)));
        EXPECT_NO_THROW(press().dispatch(9999, 1, 1, 1));
        EXPECT_NO_THROW(press().dispatch_indirect(9999, vk::Buffer {}, 0));

        foundry().submit_and_wait(cmd);
    }

    TEST_F(ComputePressTest, BindingAnUnknownDescriptorSetThrowsFromTheFoundry)
    {
        const auto id = press().create_pipeline(compute_shader(), one_storage_binding(), sizeof(uint32_t));
        ASSERT_NE(id, INVALID_COMPUTE_PIPELINE);
        const auto cmd = foundry().begin_commands(CommandType::COMPUTE);

        EXPECT_THROW(press().bind_descriptor_sets(cmd, id, { 9999 }), std::invalid_argument);

        foundry().submit_and_wait(cmd);
    }

} // namespace
} // namespace MayaFlux::Test
