#include "gtest/gtest.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Test {
namespace {

    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::INVALID_SHADER;
    using Portal::Graphics::KernelOp;
    using Portal::Graphics::ShaderCompilerConfig;
    using Portal::Graphics::ShaderFoundry;
    using Portal::Graphics::ShaderID;
    using Portal::Graphics::ShaderSpec;
    using Portal::Graphics::ShaderStage;

    ShaderFoundry& foundry()
    {
        return Portal::Graphics::get_shader_foundry();
    }

    constexpr std::string_view k_compute_source = R"(#version 460
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) buffer Data { float values[]; };
layout(push_constant) uniform PC { float gain; } pc;
void main() {
    values[gl_GlobalInvocationID.x] *= pc.gain;
}
)";

    class ShaderFoundryTest : public ::testing::Test {
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
        }

        void TearDown() override
        {
            foundry().shutdown();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    // ------------------------------------------------------------------
    // Stage helpers, no device
    // ------------------------------------------------------------------

    struct StageCase {
        std::string_view name;
        ShaderStage stage;
        vk::ShaderStageFlagBits vulkan;
        std::string_view file;
    };

    class ShaderStageTableTest : public ::testing::TestWithParam<StageCase> { };

    TEST_P(ShaderStageTableTest, MapsToItsVulkanStage)
    {
        EXPECT_EQ(ShaderFoundry::to_vulkan_stage(GetParam().stage), GetParam().vulkan);
    }

    TEST_P(ShaderStageTableTest, IsDetectedFromItsFileExtension)
    {
        EXPECT_EQ(ShaderFoundry::detect_stage_from_extension(std::string(GetParam().file)), GetParam().stage);
    }

    INSTANTIATE_TEST_SUITE_P(Stages, ShaderStageTableTest,
        ::testing::Values(
            StageCase { "Compute", ShaderStage::COMPUTE, vk::ShaderStageFlagBits::eCompute, "a.comp" },
            StageCase { "Vertex", ShaderStage::VERTEX, vk::ShaderStageFlagBits::eVertex, "a.vert" },
            StageCase { "Fragment", ShaderStage::FRAGMENT, vk::ShaderStageFlagBits::eFragment, "a.frag" },
            StageCase { "Geometry", ShaderStage::GEOMETRY, vk::ShaderStageFlagBits::eGeometry, "a.geom" },
            StageCase { "TessControl", ShaderStage::TESS_CONTROL, vk::ShaderStageFlagBits::eTessellationControl, "a.tesc" },
            StageCase { "TessEvaluation", ShaderStage::TESS_EVALUATION, vk::ShaderStageFlagBits::eTessellationEvaluation, "a.tese" },
            StageCase { "Mesh", ShaderStage::MESH, vk::ShaderStageFlagBits::eMeshEXT, "a.mesh" },
            StageCase { "Task", ShaderStage::TASK, vk::ShaderStageFlagBits::eTaskEXT, "a.task" }),
        [](const ::testing::TestParamInfo<StageCase>& info) { return std::string(info.param.name); });

    TEST(ShaderStageTest, UnknownExtensionHasNoStage)
    {
        EXPECT_FALSE(ShaderFoundry::detect_stage_from_extension("notes.txt").has_value());
        EXPECT_FALSE(ShaderFoundry::detect_stage_from_extension("noextension").has_value());
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryTest, UninitializedFoundryLoadsNothing)
    {
        EXPECT_FALSE(foundry().is_initialized());
        EXPECT_EQ(foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, InitializeRejectsANullBackend)
    {
        EXPECT_FALSE(foundry().initialize(nullptr));
        EXPECT_FALSE(foundry().is_initialized());
    }

    TEST_F(ShaderFoundryTest, InitializingAgainReportsSuccess)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_TRUE(foundry().initialize(backend()));
        EXPECT_TRUE(foundry().is_initialized());
    }

    TEST_F(ShaderFoundryTest, ShutdownIsRepeatableAndTheFoundryLoadsAgainAfterReinitialize)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        ASSERT_NE(foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE), INVALID_SHADER);

        foundry().shutdown();
        EXPECT_NO_THROW(foundry().shutdown());
        EXPECT_FALSE(foundry().is_initialized());

        ASSERT_TRUE(foundry().initialize(backend()));
        EXPECT_NE(foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE), INVALID_SHADER);
    }

    // ------------------------------------------------------------------
    // Compiling and loading
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryTest, GlslSourceCompilesToAShaderIdAndTheSameSourceReturnsTheSameId)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        const ShaderID first = foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE);
        const ShaderID again = foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE);

        ASSERT_NE(first, INVALID_SHADER);
        EXPECT_EQ(again, first);
    }

    TEST_F(ShaderFoundryTest, SourceWithoutAStageIsRefused)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_EQ(foundry().load_shader(std::string(k_compute_source)), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, BrokenGlslIsRefused)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_EQ(foundry().load_shader("#version 460\nvoid main() { not valid }\n", ShaderStage::COMPUTE), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, SuppliedShaderFileLoadsWithItsStageDetectedAndIsCachedByName)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        const ShaderID id = foundry().load_shader("dilate.comp");

        ASSERT_NE(id, INVALID_SHADER);
        EXPECT_EQ(foundry().get_shader_stage(id), ShaderStage::COMPUTE);
        EXPECT_EQ(foundry().load_shader("dilate.comp"), id);
        EXPECT_TRUE(foundry().is_cached("dilate.comp"));
    }

    TEST_F(ShaderFoundryTest, UnknownFileNameIsRefused)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_EQ(foundry().load_shader("no_such_shader_file.comp"), INVALID_SHADER);
        EXPECT_EQ(foundry().load_shader("no_such_shader_file.comp", ShaderStage::COMPUTE), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, GeneratedAssemblyShaderReturnsTheSameIdForTheSameSpec)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const auto spec = ShaderSpec::Assemble {}
                              .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
                              .pc("gain")
                              .op(KernelOp::Scale)
                              .build();

        const ShaderID first = foundry().load_shader(spec);
        const ShaderID again = foundry().load_shader(spec);

        ASSERT_NE(first, INVALID_SHADER);
        EXPECT_EQ(again, first);
    }

    TEST_F(ShaderFoundryTest, GeneratedKernelShaderReturnsTheSameIdForTheSameSpec)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const auto spec = ShaderSpec::Assemble {}
                              .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
                              .pc("gain")
                              .kernel(MF_KERNEL([](float* sig, float gain, uint32_t i) { sig[i] = sig[i] * gain; }))
                              .build();

        const ShaderID first = foundry().load_shader(spec);
        const ShaderID again = foundry().load_shader(spec);

        ASSERT_NE(first, INVALID_SHADER);
        EXPECT_EQ(again, first);
    }

    // ------------------------------------------------------------------
    // Registry, reflection and cache control
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryTest, ReflectionDescribesWorkgroupBindingsAndPushConstants)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const ShaderID id = foundry().load_shader(std::string(k_compute_source), ShaderStage::COMPUTE);
        ASSERT_NE(id, INVALID_SHADER);

        const auto info = foundry().get_shader_reflection(id);

        EXPECT_EQ(info.stage, ShaderStage::COMPUTE);
        EXPECT_EQ(info.entry_point, "main");
        ASSERT_TRUE(info.workgroup_size.has_value());
        EXPECT_EQ(*info.workgroup_size, (std::array<uint32_t, 3> { 8, 4, 1 }));
        ASSERT_EQ(info.descriptor_bindings.size(), 1U);
        EXPECT_EQ(info.descriptor_bindings[0].set, 0U);
        EXPECT_EQ(info.descriptor_bindings[0].binding, 0U);
        EXPECT_EQ(info.descriptor_bindings[0].type, vk::DescriptorType::eStorageBuffer);
        ASSERT_EQ(info.push_constant_ranges.size(), 1U);
        EXPECT_EQ(info.push_constant_ranges[0].size, 4U);
    }

    TEST_F(ShaderFoundryTest, UnknownShaderIdHasDefaultStageEntryPointAndEmptyReflection)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_EQ(foundry().get_shader_stage(9999), ShaderStage::COMPUTE);
        EXPECT_EQ(foundry().get_shader_entry_point(9999), "main");
        EXPECT_TRUE(foundry().get_shader_reflection(9999).descriptor_bindings.empty());
    }

    TEST_F(ShaderFoundryTest, DestroyShaderDropsItsCacheEntryAndTheShaderLoadsAgain)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const ShaderID id = foundry().load_shader("dilate.comp");
        ASSERT_NE(id, INVALID_SHADER);

        foundry().destroy_shader(id);

        EXPECT_FALSE(foundry().is_cached("dilate.comp"));
        EXPECT_NE(foundry().load_shader("dilate.comp"), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, DestroyingAnUnknownShaderIsIgnored)
    {
        ASSERT_TRUE(foundry().initialize(backend()));

        EXPECT_NO_THROW(foundry().destroy_shader(9999));
    }

    TEST_F(ShaderFoundryTest, ReloadShaderReturnsAFreshId)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const ShaderID original = foundry().load_shader("dilate.comp");
        ASSERT_NE(original, INVALID_SHADER);

        const ShaderID reloaded = foundry().reload_shader("dilate.comp");

        EXPECT_NE(reloaded, INVALID_SHADER);
        EXPECT_NE(reloaded, original);
    }

    TEST_F(ShaderFoundryTest, CacheKeysAreListedAndCanBeInvalidatedOrCleared)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        ASSERT_NE(foundry().load_shader("dilate.comp"), INVALID_SHADER);
        ASSERT_NE(foundry().load_shader("erode.comp"), INVALID_SHADER);

        const auto keys = foundry().get_cached_keys();
        EXPECT_NE(std::ranges::find(keys, "dilate.comp"), keys.end());
        EXPECT_NE(std::ranges::find(keys, "erode.comp"), keys.end());

        foundry().invalidate_cache("dilate.comp");
        EXPECT_FALSE(foundry().is_cached("dilate.comp"));
        EXPECT_TRUE(foundry().is_cached("erode.comp"));

        foundry().clear_cache();
        EXPECT_TRUE(foundry().get_cached_keys().empty());
    }

    TEST_F(ShaderFoundryTest, HotReloadCompilesTheFileAgainAndKeepsItCached)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        ASSERT_NE(foundry().load_shader("dilate.comp"), INVALID_SHADER);

        const auto reloaded = foundry().hot_reload("dilate.comp");

        EXPECT_NE(reloaded, nullptr);
        EXPECT_TRUE(foundry().is_cached("dilate.comp"));
    }

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    TEST_F(ShaderFoundryTest, DefinesReachTheCompiler)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        const std::string source = "#version 460\n#ifndef MF_TEST_FLAG\n#error flag missing\n#endif\n"
                                   "layout(local_size_x = 1) in;\nvoid main() { }\n";
        ASSERT_EQ(foundry().load_shader(source, ShaderStage::COMPUTE), INVALID_SHADER);

        foundry().add_define("MF_TEST_FLAG", "1");

        EXPECT_NE(foundry().load_shader(source, ShaderStage::COMPUTE), INVALID_SHADER);
    }

    TEST_F(ShaderFoundryTest, DefaultIncludeDirectoriesSurviveSetConfig)
    {
        ASSERT_TRUE(foundry().initialize(backend()));
        ASSERT_NE(foundry().load_shader("dilate.comp"), INVALID_SHADER);

        foundry().set_config(ShaderCompilerConfig {});

        EXPECT_NE(foundry().hot_reload("dilate.comp"), nullptr);
    }

} // namespace
} // namespace MayaFlux::Test
