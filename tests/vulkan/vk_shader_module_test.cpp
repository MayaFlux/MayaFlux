#include "gtest/gtest.h"

#include <algorithm>

#include "../support/scoped_working_dir.hpp"
#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKShaderModule.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::ShaderReflection;
    using Core::Stage;
    using Core::VKShaderModule;

    constexpr const char* k_compute_source = R"(#version 450
layout(local_size_x = 8, local_size_y = 4, local_size_z = 2) in;
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

    constexpr const char* k_user_workgroup_source = R"(#version 450
layout(local_size_x = 4, local_size_y = 2, local_size_z = 1) in;
void main() { }
)";

    constexpr const char* k_resources_source = R"(#version 450
layout(local_size_x = 1) in;
layout(set = 0, binding = 0) uniform Block { vec4 color; } block;
layout(set = 0, binding = 1) uniform sampler2D tex;
layout(set = 0, binding = 2, rgba8) uniform writeonly image2D img;
layout(set = 0, binding = 3) buffer Many { float v[]; } many[3];
void main()
{
    vec4 c = textureLod(tex, vec2(0.5), 0.0) * block.color;
    imageStore(img, ivec2(0), c);
    many[0].v[0] = c.x;
    many[1].v[0] = c.y;
    many[2].v[0] = c.z;
}
)";

    constexpr const char* k_specialized_source = R"(#version 450
layout(local_size_x = 1) in;
layout(constant_id = 3) const uint k_factor = 2;
layout(set = 0, binding = 0) buffer Out { uint v[]; } out_buf;
void main() { out_buf.v[0] = k_factor; }
)";

    constexpr const char* k_vertex_source = R"(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
void main() { gl_Position = vec4(position, 1.0) + vec4(uv, 0.0, 0.0); }
)";

    constexpr const char* k_fragment_source = R"(#version 450
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
)";

    constexpr const char* k_assembly = R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%fn = OpTypeFunction %void
%main = OpFunction %void None %fn
%label = OpLabel
OpReturn
OpFunctionEnd
)";

    const ShaderReflection::DescriptorBinding* find_binding(
        const ShaderReflection& reflection, uint32_t set, uint32_t binding)
    {
        auto it = std::ranges::find_if(reflection.bindings, [&](const auto& b) {
            return b.set == set && b.binding == binding;
        });
        return it == reflection.bindings.end() ? nullptr : &*it;
    }

    struct StageCase {
        vk::ShaderStageFlagBits vk_stage;
        const char* source;
        Stage expected;
    };

    class VKShaderModuleStageTest : public VulkanDeviceTest,
                                    public ::testing::WithParamInterface<StageCase> {
    protected:
        void TearDown() override { module.cleanup(backend().get_context().get_device()); }
        VKShaderModule module;
    };

    class VKShaderModuleTest : public VulkanDeviceTest {
    protected:
        void TearDown() override { module.cleanup(device()); }

        static vk::Device device() { return backend().get_context().get_device(); }

        bool compile(const char* source, vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eCompute)
        {
            return module.create_from_glsl(device(), source, stage);
        }

        VKShaderModule module;
    };

}

TEST(VKShaderModuleStageDetectionTest, MapsKnownExtensions)
{
    using S = vk::ShaderStageFlagBits;
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("a/b/shader.comp"), S::eCompute);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.vert"), S::eVertex);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.frag"), S::eFragment);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.geom"), S::eGeometry);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.tesc"), S::eTessellationControl);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.tese"), S::eTessellationEvaluation);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.mesh"), S::eMeshEXT);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.task"), S::eTaskEXT);
}

TEST(VKShaderModuleStageDetectionTest, IgnoresExtensionCase)
{
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.COMP"), vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(VKShaderModule::detect_stage_from_extension("shader.Frag"), vk::ShaderStageFlagBits::eFragment);
}

TEST(VKShaderModuleStageDetectionTest, UnknownOrMissingExtensionHasNoStage)
{
    EXPECT_FALSE(VKShaderModule::detect_stage_from_extension("shader.glsl").has_value());
    EXPECT_FALSE(VKShaderModule::detect_stage_from_extension("shader").has_value());
    EXPECT_FALSE(VKShaderModule::detect_stage_from_extension("").has_value());
}

TEST(VKShaderModuleUninitializedTest, DefaultModuleIsInvalid)
{
    VKShaderModule module;
    EXPECT_FALSE(module.is_valid());
    EXPECT_FALSE(module.get());
    EXPECT_EQ(module.get_entry_point(), "main");
    EXPECT_TRUE(module.get_spirv().empty());
    EXPECT_TRUE(module.get_reflection().bindings.empty());
}

TEST(VKShaderModuleUninitializedTest, StageCreateInfoOfInvalidModuleIsEmpty)
{
    VKShaderModule module;
    const auto info = module.get_stage_create_info();
    EXPECT_FALSE(info.module);
}

TEST_F(VKShaderModuleTest, CompilesComputeGlslIntoValidModule)
{
    ASSERT_TRUE(compile(k_compute_source));

    EXPECT_TRUE(module.is_valid());
    EXPECT_EQ(module.get_stage(), vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(module.get_stage_type(), Stage::COMPUTE);
    EXPECT_EQ(module.get_entry_point(), "main");
}

TEST_F(VKShaderModuleTest, InvalidGlslFailsAndLeavesModuleInvalid)
{
    EXPECT_FALSE(compile("#version 450\nvoid main() { this is not glsl }\n"));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, UnsupportedStageForGlslFails)
{
    EXPECT_FALSE(compile(k_compute_source, vk::ShaderStageFlagBits::eRaygenKHR));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, DefinesReachTheCompiler)
{
    const char* source = R"(#version 450
layout(local_size_x = 1) in;
layout(set = 0, binding = 0) buffer Out { uint v[]; } o;
void main() { o.v[0] = FACTOR; }
)";
    EXPECT_FALSE(module.create_from_glsl(device(), source, vk::ShaderStageFlagBits::eCompute));
    EXPECT_TRUE(module.create_from_glsl(device(), source, vk::ShaderStageFlagBits::eCompute,
        "main", true, {}, { { "FACTOR", "7" } }));
}

TEST_F(VKShaderModuleTest, EmptySpirvIsRejected)
{
    EXPECT_FALSE(module.create_from_spirv(device(), {}, vk::ShaderStageFlagBits::eCompute));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, SpirvWithWrongMagicNumberIsRejected)
{
    const std::vector<uint32_t> bogus { 0xDEADBEEF, 0, 0, 0, 0 };
    EXPECT_FALSE(module.create_from_spirv(device(), bogus, vk::ShaderStageFlagBits::eCompute));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, MissingSpirvFileFails)
{
    EXPECT_FALSE(module.create_from_spirv_file(device(), "no/such/shader.spv", vk::ShaderStageFlagBits::eCompute));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, MissingGlslFileFails)
{
    EXPECT_FALSE(module.create_from_glsl_file(device(), "no/such/shader.comp"));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, UserShaderDirectoryWinsOverALibraryShaderOfTheSameName)
{
    ScopedWorkingDir cwd;
    cwd.write("shaders/dilate.comp", k_user_workgroup_source);

    ASSERT_TRUE(module.create_from_glsl_file(device(), "dilate.comp"));

    const auto workgroup = module.get_workgroup_size();
    ASSERT_TRUE(workgroup.has_value());
    EXPECT_EQ(*workgroup, (std::array<uint32_t, 3> { 4, 2, 1 }));
}

TEST_F(VKShaderModuleTest, DataShadersDirectoryAlsoWinsOverALibraryShaderOfTheSameName)
{
    ScopedWorkingDir cwd;
    cwd.write("data/shaders/dilate.comp", k_user_workgroup_source);

    ASSERT_TRUE(module.create_from_glsl_file(device(), "dilate.comp"));

    const auto workgroup = module.get_workgroup_size();
    ASSERT_TRUE(workgroup.has_value());
    EXPECT_EQ(*workgroup, (std::array<uint32_t, 3> { 4, 2, 1 }));
}

TEST_F(VKShaderModuleTest, LibraryShaderIsUsedWhenNoUserCopyExists)
{
    ScopedWorkingDir cwd;

    ASSERT_TRUE(module.create_from_glsl_file(device(), "element_shape.comp"));

    const auto workgroup = module.get_workgroup_size();
    ASSERT_TRUE(workgroup.has_value());
    EXPECT_EQ(*workgroup, (std::array<uint32_t, 3> { 256, 1, 1 }));
}

TEST_F(VKShaderModuleTest, GlslFileWithUndetectableStageFailsWithoutExplicitStage)
{
    EXPECT_FALSE(module.create_from_glsl_file(device(), "no/such/shader.glsl"));
}

TEST_F(VKShaderModuleTest, AssemblesAndCreatesModuleFromSpirvAssembly)
{
    ASSERT_TRUE(module.create_from_spirv_asm(device(), k_assembly, vk::ShaderStageFlagBits::eCompute));
    EXPECT_TRUE(module.is_valid());
}

TEST_F(VKShaderModuleTest, MalformedSpirvAssemblyFails)
{
    EXPECT_FALSE(module.create_from_spirv_asm(device(), "OpNotAnInstruction %x", vk::ShaderStageFlagBits::eCompute));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, SpirvAssemblyThatFailsValidationIsRejected)
{
    EXPECT_FALSE(module.create_from_spirv_asm(device(),
        "OpCapability Shader\nOpMemoryModel Logical GLSL450\n", vk::ShaderStageFlagBits::eCompute));
    EXPECT_FALSE(module.is_valid());
}

TEST_F(VKShaderModuleTest, SpirvIsDiscardedUnlessPreservationIsRequested)
{
    ASSERT_TRUE(compile(k_compute_source));
    EXPECT_TRUE(module.get_spirv().empty());
}

TEST_F(VKShaderModuleTest, PreservedSpirvKeepsTheMagicNumber)
{
    module.set_preserve_spirv(true);
    ASSERT_TRUE(compile(k_compute_source));

    ASSERT_FALSE(module.get_spirv().empty());
    EXPECT_EQ(module.get_spirv().front(), 0x07230203U);
}

TEST_F(VKShaderModuleTest, CleanupInvalidatesModuleAndClearsReflection)
{
    ASSERT_TRUE(compile(k_compute_source));
    module.cleanup(device());

    EXPECT_FALSE(module.is_valid());
    EXPECT_TRUE(module.get_reflection().bindings.empty());
    EXPECT_NO_THROW(module.cleanup(device()));
}

TEST_F(VKShaderModuleTest, ReflectsStorageBufferBindingsWithSetAndBinding)
{
    ASSERT_TRUE(compile(k_compute_source));
    const auto& reflection = module.get_reflection();

    ASSERT_EQ(reflection.bindings.size(), 2U);
    const auto* in = find_binding(reflection, 0, 0);
    const auto* out = find_binding(reflection, 0, 1);
    ASSERT_NE(in, nullptr);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(in->type, vk::DescriptorType::eStorageBuffer);
    EXPECT_EQ(out->type, vk::DescriptorType::eStorageBuffer);
    EXPECT_EQ(in->count, 1U);
    EXPECT_EQ(in->stage, vk::ShaderStageFlagBits::eCompute);
}

TEST_F(VKShaderModuleTest, ReflectsWorkgroupSize)
{
    ASSERT_TRUE(compile(k_compute_source));

    const auto size = module.get_workgroup_size();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ((*size)[0], 8U);
    EXPECT_EQ((*size)[1], 4U);
    EXPECT_EQ((*size)[2], 2U);
}

TEST_F(VKShaderModuleTest, ReflectsPushConstantBlockSize)
{
    ASSERT_TRUE(compile(k_compute_source));
    const auto& reflection = module.get_reflection();

    ASSERT_EQ(reflection.push_constants.size(), 1U);
    EXPECT_EQ(reflection.push_constants[0].size, 8U);
    EXPECT_EQ(reflection.push_constants[0].stage, vk::ShaderStageFlagBits::eCompute);
}

TEST_F(VKShaderModuleTest, ReflectsEveryDescriptorTypeAndArrayCount)
{
    ASSERT_TRUE(compile(k_resources_source));
    const auto& reflection = module.get_reflection();

    const auto* uniform = find_binding(reflection, 0, 0);
    const auto* sampled = find_binding(reflection, 0, 1);
    const auto* storage_image = find_binding(reflection, 0, 2);
    const auto* array = find_binding(reflection, 0, 3);
    ASSERT_NE(uniform, nullptr);
    ASSERT_NE(sampled, nullptr);
    ASSERT_NE(storage_image, nullptr);
    ASSERT_NE(array, nullptr);

    EXPECT_EQ(uniform->type, vk::DescriptorType::eUniformBuffer);
    EXPECT_EQ(sampled->type, vk::DescriptorType::eCombinedImageSampler);
    EXPECT_EQ(storage_image->type, vk::DescriptorType::eStorageImage);
    EXPECT_EQ(array->type, vk::DescriptorType::eStorageBuffer);
    EXPECT_EQ(array->count, 3U);
}

TEST_F(VKShaderModuleTest, ReflectionDisabledLeavesNoBindings)
{
    ASSERT_TRUE(module.create_from_glsl(device(), k_compute_source, vk::ShaderStageFlagBits::eCompute,
        "main", false));
    EXPECT_TRUE(module.get_reflection().bindings.empty());
    EXPECT_FALSE(module.get_workgroup_size().has_value());
}

TEST_F(VKShaderModuleTest, ReflectsSpecializationConstantIds)
{
    ASSERT_TRUE(compile(k_specialized_source));
    const auto& reflection = module.get_reflection();

    ASSERT_EQ(reflection.specialization_constants.size(), 1U);
    EXPECT_EQ(reflection.specialization_constants[0].constant_id, 3U);
    EXPECT_EQ(reflection.specialization_constants[0].size, sizeof(uint32_t));
}

TEST_F(VKShaderModuleTest, StageCreateInfoCarriesStageModuleAndEntryPoint)
{
    ASSERT_TRUE(compile(k_compute_source));
    const auto info = module.get_stage_create_info();

    EXPECT_EQ(info.stage, vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(info.module, module.get());
    EXPECT_STREQ(info.pName, "main");
    EXPECT_EQ(info.pSpecializationInfo, nullptr);
}

TEST_F(VKShaderModuleTest, SpecializationConstantsReachTheStageCreateInfo)
{
    ASSERT_TRUE(compile(k_specialized_source));
    module.set_specialization_constants({ { 3U, 42U } });

    const auto info = module.get_stage_create_info();

    ASSERT_NE(info.pSpecializationInfo, nullptr);
    ASSERT_EQ(info.pSpecializationInfo->mapEntryCount, 1U);
    EXPECT_EQ(info.pSpecializationInfo->pMapEntries[0].constantID, 3U);
    EXPECT_EQ(info.pSpecializationInfo->pMapEntries[0].size, sizeof(uint32_t));
    ASSERT_EQ(info.pSpecializationInfo->dataSize, sizeof(uint32_t));
    EXPECT_EQ(*static_cast<const uint32_t*>(info.pSpecializationInfo->pData), 42U);
}

TEST_F(VKShaderModuleTest, ReflectsVertexInputLocationsAndFormats)
{
    ASSERT_TRUE(compile(k_vertex_source, vk::ShaderStageFlagBits::eVertex));
    EXPECT_EQ(module.get_stage_type(), Stage::VERTEX);

    const auto& attributes = module.get_reflection().vertex_attributes;
    ASSERT_EQ(attributes.size(), 2U);

    auto format_at = [&](uint32_t location) {
        auto it = std::ranges::find_if(attributes, [location](const auto& a) { return a.location == location; });
        return it == attributes.end() ? vk::Format::eUndefined : it->format;
    };
    EXPECT_EQ(format_at(0), vk::Format::eR32G32B32Sfloat);
    EXPECT_EQ(format_at(1), vk::Format::eR32G32Sfloat);
}

TEST_P(VKShaderModuleStageTest, StageTypeFollowsCompiledStage)
{
    const auto& param = GetParam();
    ASSERT_TRUE(module.create_from_glsl(backend().get_context().get_device(), param.source, param.vk_stage));

    EXPECT_EQ(module.get_stage(), param.vk_stage);
    EXPECT_EQ(module.get_stage_type(), param.expected);
}

INSTANTIATE_TEST_SUITE_P(Stages, VKShaderModuleStageTest,
    ::testing::Values(
        StageCase { vk::ShaderStageFlagBits::eCompute, k_compute_source, Stage::COMPUTE },
        StageCase { vk::ShaderStageFlagBits::eVertex, k_vertex_source, Stage::VERTEX },
        StageCase { vk::ShaderStageFlagBits::eFragment, k_fragment_source, Stage::FRAGMENT }));

}
