#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Test {
namespace {

    using Kakshya::DataModality;
    using Kakshya::GpuDataFormat;
    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelOp;
    using Portal::Graphics::KernelSource;
    using Portal::Graphics::KernelTemplate;
    using Portal::Graphics::ShaderSpec;

    // ------------------------------------------------------------------
    // ShaderSpec::Assemble
    // ------------------------------------------------------------------

    TEST(ShaderSpecAssembleTest, DefaultsDescribeAnElementwiseScaleOverBlocksOf256)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}.build();

        EXPECT_EQ(spec.tmpl, KernelTemplate::Elementwise);
        EXPECT_EQ(spec.op, KernelOp::Scale);
        EXPECT_TRUE(spec.bindings.empty());
        EXPECT_TRUE(spec.pc_fields.empty());
        EXPECT_TRUE(spec.functions.empty());
        EXPECT_FALSE(spec.kernel.has_value());
        EXPECT_EQ(spec.push_constant_bytes, 0U);
        EXPECT_EQ(spec.workgroup_size, (std::array<uint32_t, 3> { 256, 1, 1 }));
    }

    TEST(ShaderSpecAssembleTest, TemplateOpAndWorkgroupAreCarriedIntoTheSpec)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .tmpl(KernelTemplate::Reduction)
                                    .op(KernelOp::Sum)
                                    .workgroup(64, 2, 4)
                                    .build();

        EXPECT_EQ(spec.tmpl, KernelTemplate::Reduction);
        EXPECT_EQ(spec.op, KernelOp::Sum);
        EXPECT_EQ(spec.workgroup_size, (std::array<uint32_t, 3> { 64, 2, 4 }));
    }

    TEST(ShaderSpecAssembleTest, WorkgroupYAndZDefaultToOne)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}.workgroup(128).build();

        EXPECT_EQ(spec.workgroup_size, (std::array<uint32_t, 3> { 128, 1, 1 }));
    }

    TEST(ShaderSpecAssembleTest, BindingsGetSequentialIndicesInDeclarationOrder)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("a", BindingDirection::Input, GpuDataFormat::FLOAT32)
                                    .storage_image("img")
                                    .texture("tex")
                                    .ssbo("b", BindingDirection::Output, GpuDataFormat::UINT32)
                                    .build();

        ASSERT_EQ(spec.bindings.size(), 4U);
        for (uint32_t i = 0; i < spec.bindings.size(); ++i) {
            EXPECT_EQ(spec.bindings[i].binding_index, i) << spec.bindings[i].name;
        }
        EXPECT_EQ(spec.bindings[0].name, "a");
        EXPECT_EQ(spec.bindings[1].name, "img");
        EXPECT_EQ(spec.bindings[2].name, "tex");
        EXPECT_EQ(spec.bindings[3].name, "b");
    }

    TEST(ShaderSpecAssembleTest, StartBindingAndStartSetOffsetLaterSsbos)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .start_set(2)
                                    .start_binding(5)
                                    .ssbo("a", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .ssbo("b", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .build();

        ASSERT_EQ(spec.bindings.size(), 2U);
        EXPECT_EQ(spec.bindings[0].set, 2U);
        EXPECT_EQ(spec.bindings[0].binding_index, 5U);
        EXPECT_EQ(spec.bindings[1].set, 2U);
        EXPECT_EQ(spec.bindings[1].binding_index, 6U);
    }

    TEST(ShaderSpecAssembleTest, SsboRecordsDirectionFormatAndModality)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("pos", BindingDirection::Output, GpuDataFormat::VEC4_F32, DataModality::VERTEX_POSITIONS_3D)
                                    .build();

        ASSERT_EQ(spec.bindings.size(), 1U);
        EXPECT_EQ(spec.bindings[0].direction, BindingDirection::Output);
        EXPECT_EQ(spec.bindings[0].format, GpuDataFormat::VEC4_F32);
        EXPECT_EQ(spec.bindings[0].modality, DataModality::VERTEX_POSITIONS_3D);
    }

    TEST(ShaderSpecAssembleTest, SsboModalityDefaultsToScalarFloat)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .build();

        EXPECT_EQ(spec.bindings.at(0).modality, DataModality::SCALAR_F32);
    }

    TEST(ShaderSpecAssembleTest, TextureIsAnInputSampledImageOfFourFloatChannels)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}.texture("src").build();

        ASSERT_EQ(spec.bindings.size(), 1U);
        EXPECT_EQ(spec.bindings[0].modality, DataModality::TEXTURE_2D);
        EXPECT_EQ(spec.bindings[0].direction, BindingDirection::Input);
        EXPECT_EQ(spec.bindings[0].format, GpuDataFormat::VEC4_F32);
    }

    TEST(ShaderSpecAssembleTest, StorageImageDefaultsToOutputAndHonoursAnExplicitDirection)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("dst")
                                    .storage_image("src", BindingDirection::Input)
                                    .build();

        ASSERT_EQ(spec.bindings.size(), 2U);
        EXPECT_EQ(spec.bindings[0].modality, DataModality::IMAGE_2D);
        EXPECT_EQ(spec.bindings[0].direction, BindingDirection::Output);
        EXPECT_EQ(spec.bindings[1].modality, DataModality::IMAGE_2D);
        EXPECT_EQ(spec.bindings[1].direction, BindingDirection::Input);
        EXPECT_EQ(spec.bindings[1].format, GpuDataFormat::VEC4_F32);
    }

    TEST(ShaderSpecAssembleTest, PushConstantFieldsKeepDeclarationOrderAndDefaultToFloat)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .pc("gain")
                                    .pc("count", GpuDataFormat::UINT32)
                                    .pc("offset")
                                    .build();

        ASSERT_EQ(spec.pc_fields.size(), 3U);
        EXPECT_EQ(spec.pc_fields[0].name, "gain");
        EXPECT_EQ(spec.pc_fields[0].format, GpuDataFormat::FLOAT32);
        EXPECT_EQ(spec.pc_fields[1].name, "count");
        EXPECT_EQ(spec.pc_fields[1].format, GpuDataFormat::UINT32);
        EXPECT_EQ(spec.pc_fields[2].name, "offset");
    }

    TEST(ShaderSpecAssembleTest, PushConstantBytesSumTheFieldFormats)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .pc("gain")
                                    .pc("count", GpuDataFormat::UINT32)
                                    .pc("tint", GpuDataFormat::VEC4_F32)
                                    .build();

        const auto expected = static_cast<uint32_t>(
            Kakshya::gpu_data_format_bytes(GpuDataFormat::FLOAT32)
            + Kakshya::gpu_data_format_bytes(GpuDataFormat::UINT32)
            + Kakshya::gpu_data_format_bytes(GpuDataFormat::VEC4_F32));
        EXPECT_EQ(spec.push_constant_bytes, expected);
        EXPECT_GT(spec.push_constant_bytes, 0U);
    }

    TEST(ShaderSpecAssembleTest, FunctionsKeepDeclarationOrderAndAllFourParts)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .function("float", "first", "float x", "    return x;")
                                    .function("vec3", "second", "vec3 p, float k", "    return p * k;")
                                    .build();

        ASSERT_EQ(spec.functions.size(), 2U);
        EXPECT_EQ(spec.functions[0].return_type, "float");
        EXPECT_EQ(spec.functions[0].name, "first");
        EXPECT_EQ(spec.functions[0].params, "float x");
        EXPECT_EQ(spec.functions[0].body, "    return x;");
        EXPECT_EQ(spec.functions[1].name, "second");
        EXPECT_EQ(spec.functions[1].params, "vec3 p, float k");
    }

    TEST(ShaderSpecAssembleTest, KernelWithoutParamNamesTakesBindingsThenPushFieldsThenIndex)
    {
        KernelSource source;
        source.body = " sig[i] *= gain; ";

        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .ssbo("aux", BindingDirection::Input, GpuDataFormat::FLOAT32)
                                    .pc("gain")
                                    .pc("bias")
                                    .kernel(source)
                                    .build();

        ASSERT_TRUE(spec.kernel.has_value());
        EXPECT_EQ(spec.kernel->param_names,
            (std::vector<std::string> { "sig", "aux", "gain", "bias", "i" }));
        EXPECT_EQ(spec.kernel->body, " sig[i] *= gain; ");
    }

    TEST(ShaderSpecAssembleTest, KernelWithParamNamesKeepsThemUntouched)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .pc("gain")
                                    .kernel(MF_KERNEL([](float* data, float amount) { data[0] *= amount; }))
                                    .build();

        ASSERT_TRUE(spec.kernel.has_value());
        EXPECT_EQ(spec.kernel->param_names, (std::vector<std::string> { "data", "amount" }));
    }

    // ------------------------------------------------------------------
    // KernelSource::parse and MF_KERNEL
    // ------------------------------------------------------------------

    TEST(KernelSourceTest, ParameterNamesDropTypesAndPointerStars)
    {
        const KernelSource ks = KernelSource::parse(
            "[](float* sig, const float* ref, float gain, uint32_t i) { sig[i] = gain; }");

        EXPECT_EQ(ks.param_names, (std::vector<std::string> { "sig", "ref", "gain", "i" }));
    }

    TEST(KernelSourceTest, BodyIsTheTextBetweenTheOutermostBracesVerbatim)
    {
        const KernelSource ks = KernelSource::parse(
            "[](float* sig, uint32_t i) {\n    sig[i] = 1.0f;\n}");

        EXPECT_EQ(ks.body, "\n    sig[i] = 1.0f;\n");
    }

    TEST(KernelSourceTest, NestedBracesStayInsideTheBody)
    {
        const KernelSource ks = KernelSource::parse(
            "[](float* sig, uint32_t i) { if (sig[i] > 0.0f) { sig[i] = 0.0f; } sig[i] += 1.0f; }");

        EXPECT_EQ(ks.body, " if (sig[i] > 0.0f) { sig[i] = 0.0f; } sig[i] += 1.0f; ");
    }

    TEST(KernelSourceTest, CommasInsideTemplateArgumentsDoNotSplitAParameter)
    {
        const KernelSource ks = KernelSource::parse(
            "[](std::array<float, 2> pair, float gain) { }");

        EXPECT_EQ(ks.param_names, (std::vector<std::string> { "pair", "gain" }));
    }

    TEST(KernelSourceTest, CaptureListDoesNotDisturbParameterExtraction)
    {
        const KernelSource ks = KernelSource::parse("[&](float* sig, uint32_t i) { sig[i] = 0.0f; }");

        EXPECT_EQ(ks.param_names, (std::vector<std::string> { "sig", "i" }));
        EXPECT_EQ(ks.body, " sig[i] = 0.0f; ");
    }

    TEST(KernelSourceTest, EmptyParameterListAndBodyAreAccepted)
    {
        const KernelSource ks = KernelSource::parse("[]() { }");

        EXPECT_TRUE(ks.param_names.empty());
        EXPECT_EQ(ks.body, " ");
    }

    TEST(KernelSourceTest, RawKeepsTheStringifiedLambdaUnchanged)
    {
        const std::string text = "[](float* sig, uint32_t i) { sig[i] = 0.0f; }";

        EXPECT_EQ(KernelSource::parse(text).raw, text);
    }

    TEST(KernelSourceTest, TextWithoutAParameterListYieldsNoParametersAndNoBody)
    {
        const KernelSource ks = KernelSource::parse("not a lambda");

        EXPECT_TRUE(ks.param_names.empty());
        EXPECT_TRUE(ks.body.empty());
        EXPECT_EQ(ks.raw, "not a lambda");
    }

    TEST(KernelSourceTest, ParameterListWithoutABodyYieldsNamesOnly)
    {
        const KernelSource ks = KernelSource::parse("[](float* sig, uint32_t i)");

        EXPECT_EQ(ks.param_names, (std::vector<std::string> { "sig", "i" }));
        EXPECT_TRUE(ks.body.empty());
    }

    TEST(KernelSourceTest, MfKernelParsesTheLambdaWrittenAtTheCallSite)
    {
        const KernelSource ks = MF_KERNEL([](float* sig, float gain, uint32_t i) {
            sig[i] = sig[i] * gain;
        });

        EXPECT_EQ(ks.param_names, (std::vector<std::string> { "sig", "gain", "i" }));
        EXPECT_NE(ks.body.find("sig[i] = sig[i] * gain;"), std::string::npos);
    }

} // namespace
} // namespace MayaFlux::Test
