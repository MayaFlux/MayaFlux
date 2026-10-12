#include "gtest/gtest.h"

#include <initializer_list>
#include <string>
#include <string_view>

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Test {
namespace {

    using Kakshya::GpuDataFormat;
    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelOp;
    using Portal::Graphics::KernelTemplate;
    using Portal::Graphics::ShaderSpec;
    using Portal::Graphics::detail::emit_glsl_kernel;
    using Portal::Graphics::detail::emit_spirv_asm;

    struct Expectation {
        std::string_view needle;
        std::string_view reason;
    };

    ::testing::AssertionResult contains_all(const std::string& text, std::initializer_list<Expectation> expected)
    {
        std::string missing;
        for (const auto& e : expected) {
            if (text.find(e.needle) == std::string::npos) {
                missing += "  missing: " + std::string(e.needle) + "   (" + std::string(e.reason) + ")\n";
            }
        }
        if (missing.empty()) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "\n"
                                             << missing << "---- emitted ----\n"
                                             << text << "-----------------\n";
    }

    ::testing::AssertionResult contains_none(const std::string& text, std::initializer_list<Expectation> forbidden)
    {
        std::string present;
        for (const auto& e : forbidden) {
            if (text.find(e.needle) != std::string::npos) {
                present += "  unexpected: " + std::string(e.needle) + "   (" + std::string(e.reason) + ")\n";
            }
        }
        if (present.empty()) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "\n"
                                             << present << "---- emitted ----\n"
                                             << text << "-----------------\n";
    }

    ShaderSpec ssbo_spec(KernelOp op, std::size_t pc_count, GpuDataFormat format = GpuDataFormat::FLOAT32)
    {
        ShaderSpec::Assemble assemble;
        assemble.ssbo("sig", BindingDirection::InOut, format);
        if (pc_count > 0) {
            assemble.pc("a");
        }
        if (pc_count > 1) {
            assemble.pc("b");
        }
        return assemble.op(op).build();
    }

    ShaderSpec two_input_ssbo_spec(KernelOp op, std::size_t pc_count)
    {
        ShaderSpec::Assemble assemble;
        assemble.ssbo("a", BindingDirection::Input, GpuDataFormat::FLOAT32)
            .ssbo("b", BindingDirection::Input, GpuDataFormat::FLOAT32)
            .ssbo("out", BindingDirection::Output, GpuDataFormat::FLOAT32);
        if (pc_count > 0) {
            assemble.pc("p");
        }
        return assemble.op(op).build();
    }

    // ------------------------------------------------------------------
    // Module header and layout
    // ------------------------------------------------------------------

    TEST(AsmGeneratorTest, EntryPointListsTheInterfaceAndTheWorkgroupSizeIsEmitted)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .pc("gain")
                                    .op(KernelOp::Scale)
                                    .workgroup(64, 2, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "OpCapability Shader", "base capability" },
                { "OpMemoryModel Logical GLSL450", "memory model" },
                { "OpEntryPoint GLCompute %main \"main\" %gid_var %buf_sig %pc", "interface names the buffer and the push constants" },
                { "OpExecutionMode %main LocalSize 64 2 1", "workgroup size from the spec" } }));
    }

    TEST(AsmGeneratorTest, InterfaceOmitsPushConstantsWhenNoneAreDeclared)
    {
        const std::string text = emit_spirv_asm(ssbo_spec(KernelOp::Abs, 0));

        EXPECT_TRUE(contains_all(text, { { "OpEntryPoint GLCompute %main \"main\" %gid_var %buf_sig\n", "interface ends at the buffer" } }));
        EXPECT_TRUE(contains_none(text, { { "%pc ", "no push constant variable referenced" } }));
    }

    TEST(AsmGeneratorTest, ReductionAndScanAddTheLocalInvocationIdToTheInterface)
    {
        const ShaderSpec reduction = ShaderSpec::Assemble {}
                                         .ssbo("data", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                         .tmpl(KernelTemplate::Reduction)
                                         .op(KernelOp::Sum)
                                         .workgroup(256, 1, 1)
                                         .build();
        const ShaderSpec scan = ShaderSpec::Assemble {}
                                    .ssbo("data", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .tmpl(KernelTemplate::Scan)
                                    .op(KernelOp::ScanSum)
                                    .workgroup(256, 1, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(reduction), { { "%buf_data %lid_var", "local id in the interface" } }));
        EXPECT_TRUE(contains_all(emit_spirv_asm(scan), { { "%buf_data %lid_var", "local id in the interface" } }));
    }

    TEST(AsmGeneratorTest, ImageDirectionsDecoratePerAccessAndKeepTheirBindingIndices)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("dst", BindingDirection::Output)
                                    .op(KernelOp::Abs)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "OpDecorate %img_src Binding 0", "first binding" },
                { "OpDecorate %img_dst Binding 1", "second binding" },
                { "OpDecorate %img_src NonWritable", "input image is read only" },
                { "OpDecorate %img_dst NonReadable", "output image is write only" } }));
    }

    TEST(AsmGeneratorTest, SameSpecEmitsIdenticalTextAndDifferentOpsDoNot)
    {
        const std::string first = emit_spirv_asm(ssbo_spec(KernelOp::Scale, 1));
        const std::string second = emit_spirv_asm(ssbo_spec(KernelOp::Scale, 1));
        const std::string other = emit_spirv_asm(ssbo_spec(KernelOp::Offset, 1));

        EXPECT_EQ(first, second);
        EXPECT_NE(first, other);
    }

    // ------------------------------------------------------------------
    // SSBO element types
    // ------------------------------------------------------------------

    TEST(AsmGeneratorTest, ScalarSsboUsesFloatElementsOfStrideFour)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .pc("gain")
                                    .op(KernelOp::Scale)
                                    .build();

        const std::string text = emit_spirv_asm(spec);

        EXPECT_TRUE(contains_all(text,
            { { "OpDecorate %rta_sig ArrayStride 4", "scalar stride" },
                { "%rta_sig = OpTypeRuntimeArray %f32", "scalar element type" },
                { "%pelem_sig = OpTypePointer StorageBuffer %f32", "element pointer" },
                { "OpFMul %f32", "scalar multiply" } }));
        EXPECT_TRUE(contains_none(text,
            { { "OpVectorTimesScalar", "vector op in a scalar shader" },
                { "%v4f32 = OpTypeVector", "vector type in a scalar shader" } }));
    }

    TEST(AsmGeneratorTest, Vec4SsboUsesVectorElementsOfStrideSixteen)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::VEC4_F32)
                                    .pc("gain")
                                    .op(KernelOp::Scale)
                                    .build();

        const std::string text = emit_spirv_asm(spec);

        EXPECT_TRUE(contains_all(text,
            { { "OpDecorate %rta_sig ArrayStride 16", "vec4 stride" },
                { "%v4f32 = OpTypeVector", "vector type declared" },
                { "%rta_sig = OpTypeRuntimeArray %v4f32", "vector element type" },
                { "%pelem_sig = OpTypePointer StorageBuffer %v4f32", "vector element pointer" },
                { "OpVectorTimesScalar %v4f32", "scale of a vector by a scalar" } }));
        EXPECT_TRUE(contains_none(text, { { "%pf32_sig", "pointer name of the old scalar-only layout" } }));
    }

    TEST(AsmGeneratorTest, EachSsboGetsItsOwnBlockAndBinding)
    {
        const std::string text = emit_spirv_asm(two_input_ssbo_spec(KernelOp::Add, 0));

        EXPECT_TRUE(contains_all(text,
            { { "OpDecorate %buf_a Binding 0", "first binding" },
                { "OpDecorate %buf_b Binding 1", "second binding" },
                { "OpDecorate %buf_out Binding 2", "third binding" },
                { "OpDecorate %buf_a DescriptorSet 0", "set of the first binding" } }));
    }

    // ------------------------------------------------------------------
    // Elementwise lowering
    // ------------------------------------------------------------------

    struct OpCase {
        std::string_view name;
        KernelOp op;
        std::size_t pc_count;
        std::string_view expected;
    };

    std::string case_name(const ::testing::TestParamInfo<OpCase>& info)
    {
        return std::string(info.param.name);
    }

    class AsmUnaryOpTest : public ::testing::TestWithParam<OpCase> { };

    TEST_P(AsmUnaryOpTest, LowersToItsInstructionOverTheLoadedElement)
    {
        const auto& param = GetParam();

        const std::string text = emit_spirv_asm(ssbo_spec(param.op, param.pc_count));

        EXPECT_TRUE(contains_all(text,
            { { param.expected, "instruction for the op" },
                { "OpFunctionEnd", "module is complete" } }));
    }

    INSTANTIATE_TEST_SUITE_P(Arithmetic, AsmUnaryOpTest,
        ::testing::Values(
            OpCase { "Scale", KernelOp::Scale, 1, "%res = OpFMul %f32 %val_sig %pc_a" },
            OpCase { "ScaleOffset", KernelOp::ScaleOffset, 2, "%res = OpFAdd %f32 %mul %pc_b" },
            OpCase { "Offset", KernelOp::Offset, 1, "%res = OpFAdd %f32 %val_sig %pc_a" },
            OpCase { "Clip", KernelOp::Clip, 2, "OpExtInst %f32 %glsl FClamp %val_sig %pc_a %pc_b" },
            OpCase { "Abs", KernelOp::Abs, 0, "OpExtInst %f32 %glsl FAbs %val_sig" },
            OpCase { "Negate", KernelOp::Negate, 0, "%res = OpFNegate %f32 %val_sig" },
            OpCase { "Fma", KernelOp::Fma, 2, "OpExtInst %f32 %glsl Fma %val_sig %pc_a %pc_b" },
            OpCase { "Floor", KernelOp::Floor, 0, "OpExtInst %f32 %glsl Floor %val_sig" },
            OpCase { "Ceil", KernelOp::Ceil, 0, "OpExtInst %f32 %glsl Ceil %val_sig" },
            OpCase { "Round", KernelOp::Round, 0, "OpExtInst %f32 %glsl Round %val_sig" },
            OpCase { "Trunc", KernelOp::Trunc, 0, "OpExtInst %f32 %glsl Trunc %val_sig" },
            OpCase { "Fract", KernelOp::Fract, 0, "OpExtInst %f32 %glsl Fract %val_sig" },
            OpCase { "Sqrt", KernelOp::Sqrt, 0, "OpExtInst %f32 %glsl Sqrt %val_sig" },
            OpCase { "InverseSqrt", KernelOp::InverseSqrt, 0, "OpExtInst %f32 %glsl InverseSqrt %val_sig" },
            OpCase { "IndexScale", KernelOp::IndexScale, 0, "%res = OpFMul %f32 %val_sig %i_f32" }),
        case_name);

    INSTANTIATE_TEST_SUITE_P(Transcendental, AsmUnaryOpTest,
        ::testing::Values(
            OpCase { "Sin", KernelOp::Sin, 0, "OpExtInst %f32 %glsl Sin %val_sig" },
            OpCase { "Cos", KernelOp::Cos, 0, "OpExtInst %f32 %glsl Cos %val_sig" },
            OpCase { "Tan", KernelOp::Tan, 0, "OpExtInst %f32 %glsl Tan %val_sig" },
            OpCase { "Asin", KernelOp::Asin, 0, "OpExtInst %f32 %glsl Asin %val_sig" },
            OpCase { "Acos", KernelOp::Acos, 0, "OpExtInst %f32 %glsl Acos %val_sig" },
            OpCase { "Atan", KernelOp::Atan, 0, "OpExtInst %f32 %glsl Atan %val_sig" },
            OpCase { "Sinh", KernelOp::Sinh, 0, "OpExtInst %f32 %glsl Sinh %val_sig" },
            OpCase { "Cosh", KernelOp::Cosh, 0, "OpExtInst %f32 %glsl Cosh %val_sig" },
            OpCase { "Tanh", KernelOp::Tanh, 0, "OpExtInst %f32 %glsl Tanh %val_sig" },
            OpCase { "Exp", KernelOp::Exp, 0, "OpExtInst %f32 %glsl Exp %val_sig" },
            OpCase { "Exp2", KernelOp::Exp2, 0, "OpExtInst %f32 %glsl Exp2 %val_sig" },
            OpCase { "Log", KernelOp::Log, 0, "OpExtInst %f32 %glsl Log %val_sig" },
            OpCase { "Log2", KernelOp::Log2, 0, "OpExtInst %f32 %glsl Log2 %val_sig" }),
        case_name);

    class AsmBinaryOpTest : public ::testing::TestWithParam<OpCase> { };

    TEST_P(AsmBinaryOpTest, LowersToItsInstructionOverBothLoadedElements)
    {
        const auto& param = GetParam();

        const std::string text = emit_spirv_asm(two_input_ssbo_spec(param.op, param.pc_count));

        EXPECT_TRUE(contains_all(text,
            { { param.expected, "instruction for the op" },
                { "%gep_out = OpAccessChain %pelem_out %buf_out %c0u %i", "output element addressed" },
                { "OpFunctionEnd", "module is complete" } }));
    }

    INSTANTIATE_TEST_SUITE_P(TwoSsbo, AsmBinaryOpTest,
        ::testing::Values(
            OpCase { "Add", KernelOp::Add, 0, "%res = OpFAdd %f32 %val_a %val_b" },
            OpCase { "Multiply", KernelOp::Multiply, 0, "%res = OpFMul %f32 %val_a %val_b" },
            OpCase { "Sub", KernelOp::Sub, 0, "%res = OpFSub %f32 %val_a %val_b" },
            OpCase { "Mix", KernelOp::Mix, 1, "%scl = OpFMul %f32 %dlt %pc_p" },
            OpCase { "Pow", KernelOp::Pow, 0, "OpExtInst %f32 %glsl Pow %val_a %val_b" },
            OpCase { "Atan2", KernelOp::Atan2, 0, "OpExtInst %f32 %glsl Atan2 %val_a %val_b" },
            OpCase { "Min", KernelOp::Min, 0, "OpExtInst %f32 %glsl FMin %val_a %val_b" },
            OpCase { "MaxTwo", KernelOp::MaxTwo, 0, "OpExtInst %f32 %glsl FMax %val_a %val_b" },
            OpCase { "Step", KernelOp::Step, 0, "OpExtInst %f32 %glsl Step %val_a %val_b" },
            OpCase { "SmoothStep", KernelOp::SmoothStep, 1, "OpExtInst %f32 %glsl SmoothStep %val_a %val_b %pc_p" }),
        case_name);

    class AsmVectorOpTest : public ::testing::TestWithParam<OpCase> { };

    TEST_P(AsmVectorOpTest, KeepsTheVectorElementTypeOnTheResult)
    {
        const auto& param = GetParam();

        const std::string text = emit_spirv_asm(ssbo_spec(param.op, param.pc_count, GpuDataFormat::VEC4_F32));

        EXPECT_TRUE(contains_all(text, { { param.expected, "result typed as the vec4 element" } }));
    }

    INSTANTIATE_TEST_SUITE_P(Vec4, AsmVectorOpTest,
        ::testing::Values(
            OpCase { "Scale", KernelOp::Scale, 1, "%res = OpVectorTimesScalar %v4f32 %val_sig %pc_a" },
            OpCase { "Offset", KernelOp::Offset, 1, "%res = OpFAdd %v4f32 %val_sig %spc_a" },
            OpCase { "Abs", KernelOp::Abs, 0, "OpExtInst %v4f32 %glsl FAbs %val_sig" },
            OpCase { "Negate", KernelOp::Negate, 0, "%res = OpFNegate %v4f32 %val_sig" },
            OpCase { "Floor", KernelOp::Floor, 0, "OpExtInst %v4f32 %glsl Floor %val_sig" },
            OpCase { "Sqrt", KernelOp::Sqrt, 0, "OpExtInst %v4f32 %glsl Sqrt %val_sig" },
            OpCase { "Sin", KernelOp::Sin, 0, "OpExtInst %v4f32 %glsl Sin %val_sig" },
            OpCase { "Ceil", KernelOp::Ceil, 0, "OpExtInst %v4f32 %glsl Ceil %val_sig" },
            OpCase { "InverseSqrt", KernelOp::InverseSqrt, 0, "OpExtInst %v4f32 %glsl InverseSqrt %val_sig" },
            OpCase { "Exp", KernelOp::Exp, 0, "OpExtInst %v4f32 %glsl Exp %val_sig" },
            OpCase { "Fma", KernelOp::Fma, 2, "OpExtInst %v4f32 %glsl Fma %val_sig %spc_a %spc_b" }),
        case_name);

    // ------------------------------------------------------------------
    // Image templates
    // ------------------------------------------------------------------

    TEST(AsmGeneratorTest, SingleInputImageReadsOnceScalesPerChannelAndWrites)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("dst", BindingDirection::Output)
                                    .pc("gain")
                                    .op(KernelOp::Scale)
                                    .workgroup(8, 8, 1)
                                    .build();

        const std::string text = emit_spirv_asm(spec);

        EXPECT_TRUE(contains_all(text,
            { { "StorageImageWriteWithoutFormat", "output image capability" },
                { "StorageImageReadWithoutFormat", "input image capability" },
                { "%raw_in0", "first image read result" },
                { "OpImageRead %v4f32 %img_in_val0", "input read" },
                { "OpFMul %f32", "scale applied per channel" },
                { "OpImageWrite %img_out_val", "output write" } }));
        EXPECT_TRUE(contains_none(text, { { "%raw_in1", "no second input was declared" } }));
    }

    TEST(AsmGeneratorTest, TwoInputImageReadsBothAndSubtractsPerChannel)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("a", BindingDirection::Input)
                                    .storage_image("b", BindingDirection::Input)
                                    .storage_image("out", BindingDirection::Output)
                                    .op(KernelOp::Sub)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "StorageImageReadWithoutFormat", "input image capability" },
                { "StorageImageWriteWithoutFormat", "output image capability" },
                { "%raw_in0", "first image read result" },
                { "%raw_in1", "second image read result" },
                { "OpImageRead %v4f32 %img_in_val0", "first input read" },
                { "OpImageRead %v4f32 %img_in_val1", "second input read" },
                { "OpFSub %f32", "subtraction per channel" },
                { "OpImageWrite %img_out_val", "output write" } }));
    }

    TEST(AsmGeneratorTest, SampledTextureIsSampledAtExplicitLodZero)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .texture("src")
                                    .storage_image("dst", BindingDirection::Output)
                                    .pc("width")
                                    .pc("height")
                                    .op(KernelOp::Scale)
                                    .pc("gain")
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "OpTypeSampler", "sampler type" },
                { "OpTypeImage %f32 2D 0 0 0 1", "sampled, not storage, image type" },
                { "OpTypeSampledImage", "combined type" },
                { "%tex_src", "texture variable" },
                { "OpDecorate %tex_src DescriptorSet", "texture decoration" },
                { "\"main\" %gid_var %tex_src", "texture in the entry point interface" },
                { "OpCompositeConstruct %v2f32", "uv coordinate" },
                { "OpImageSampleExplicitLod", "sample instruction" },
                { "Lod %lod_zero", "level of detail zero" },
                { "OpImageWrite %img_out_val", "output write" } }));
    }

    TEST(AsmGeneratorTest, CompareGeOnAnImageSelectsOneOrZeroAgainstTheThreshold)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("out", BindingDirection::Output)
                                    .pc("threshold")
                                    .op(KernelOp::CompareGE)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "%pc_threshold", "threshold loaded" },
                { "OpImageRead %v4f32 %img_in_val0", "input read" },
                { "%bool", "boolean type" },
                { "OpFOrdGreaterThanEqual %bool", "comparison" },
                { "OpSelect %f32", "one or zero selected" },
                { "OpImageWrite %img_out", "output write" } }));
    }

    TEST(AsmGeneratorTest, ChannelDotOnAnImageWeightsEveryChannelAndBroadcastsTheSum)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("out", BindingDirection::Output)
                                    .pc("wr")
                                    .pc("wg")
                                    .pc("wb")
                                    .pc("wa")
                                    .op(KernelOp::ChannelDot)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "OpImageRead %v4f32 %img_in_val0", "input read" },
                { "%pc_wr", "red weight" },
                { "%pc_wg", "green weight" },
                { "%pc_wb", "blue weight" },
                { "%pc_wa", "alpha weight" },
                { "%dot_r", "partial product" },
                { "%dot_val", "accumulated dot" },
                { "OpCompositeConstruct %v4f32 %dot_val %dot_val %dot_val %dot_val", "broadcast to all channels" },
                { "OpImageWrite %img_out", "output write" } }));
    }

    TEST(AsmGeneratorTest, ChannelReplicateOnAnImageCopiesTheFirstChannelAndSetsOpaqueAlpha)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("out", BindingDirection::Output)
                                    .op(KernelOp::ChannelReplicate)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "OpImageRead %v4f32 %img_in_val0", "input read" },
                { "OpCompositeExtract %f32", "channel extracted" },
                { "OpConstant %f32 1.0", "alpha constant" },
                { "OpCompositeConstruct %v4f32", "broadcast" },
                { "OpImageWrite %img_out", "output write" } }));
    }

    // ------------------------------------------------------------------
    // Structured templates
    // ------------------------------------------------------------------

    TEST(AsmGeneratorTest, ReductionUsesLocalInvocationsAndASharedMemoryTree)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("data", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .tmpl(KernelTemplate::Reduction)
                                    .op(KernelOp::Sum)
                                    .workgroup(256, 1, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "%lid_var", "local invocation variable" },
                { "BuiltIn LocalInvocationId", "local invocation builtin" },
                { "%arr_sh_t", "workgroup array type" },
                { "%shared", "shared variable" },
                { "OpLoopMerge", "structured loop" },
                { "OpPhi %u32", "stride loop variable" },
                { "OpControlBarrier", "barrier between steps" },
                { "OpFAdd %f32", "sum combine" },
                { "OpULessThan", "active-lane guard" },
                { "loop_hdr", "back edge target" } }));
    }

    TEST(AsmGeneratorTest, Convolve2dLoopsOverTheKernelWithClampedCoordinatesAndAccumulates)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .tmpl(KernelTemplate::Convolve2D)
                                    .storage_image("out", BindingDirection::Output)
                                    .storage_image("src", BindingDirection::Input)
                                    .ssbo("kernel", BindingDirection::Input, GpuDataFormat::FLOAT32)
                                    .pc("radius", GpuDataFormat::UINT32)
                                    .pc("width", GpuDataFormat::UINT32)
                                    .pc("height", GpuDataFormat::UINT32)
                                    .workgroup(8, 8, 1)
                                    .build();

        EXPECT_TRUE(contains_all(emit_spirv_asm(spec),
            { { "StorageImageReadWithoutFormat", "input image capability" },
                { "StorageImageWriteWithoutFormat", "output image capability" },
                { "%pc_radius", "radius loaded" },
                { "%pc_width", "width loaded" },
                { "%pc_height", "height loaded" },
                { "OpUGreaterThanEqual %bool", "out of bounds guard" },
                { "%ky_hdr", "outer loop" },
                { "%kx_hdr", "inner loop" },
                { "OpPhi %u32", "loop counter" },
                { "OpPhi %v4f32", "accumulator" },
                { "OpLoopMerge", "structured loops" },
                { "OpExtInst %i32 %glsl SMax", "clamp low" },
                { "OpExtInst %i32 %glsl SMin", "clamp high" },
                { "OpImageRead %v4f32 %img_src", "tap read" },
                { "%buf_kernel", "weight buffer" },
                { "OpFAdd %v4f32", "accumulate" },
                { "OpImageWrite %img_out", "output write" } }));
    }

    // ------------------------------------------------------------------
    // GLSL kernel path
    // ------------------------------------------------------------------

    ShaderSpec scale_kernel_spec()
    {
        return ShaderSpec::Assemble {}
            .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
            .pc("gain")
            .pc("count", GpuDataFormat::UINT32)
            .workgroup(64, 2, 1)
            .kernel(MF_KERNEL([](float* sig, float gain, uint32_t count, uint32_t i) {
                sig[i] = sig[i] * gain;
            }))
            .build();
    }

    TEST(GlslKernelTest, HeaderDeclaresTheVersionAndWorkgroupSize)
    {
        const std::string text = emit_glsl_kernel(scale_kernel_spec());

        EXPECT_EQ(text.rfind("#version 460\n", 0), 0U);
        EXPECT_TRUE(contains_all(text,
            { { "layout(local_size_x = 64, local_size_y = 2, local_size_z = 1) in;", "workgroup size from the spec" } }));
    }

    TEST(GlslKernelTest, SsbosBecomeStd430BlocksNamedAfterTheirBinding)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .ssbo("pos", BindingDirection::InOut, GpuDataFormat::VEC4_F32)
                                    .ssbo("ids", BindingDirection::Input, GpuDataFormat::UINT32)
                                    .kernel(MF_KERNEL([](float* sig, vec4* pos, uint* ids, uint32_t i) { }))
                                    .build();

        EXPECT_TRUE(contains_all(emit_glsl_kernel(spec),
            { { "layout(set = 0, binding = 0, std430) buffer Block_sig { float sig[]; };", "float block" },
                { "layout(set = 0, binding = 1, std430) buffer Block_pos { vec4 pos[]; };", "vec4 block" },
                { "layout(set = 0, binding = 2, std430) buffer Block_ids { uint ids[]; };", "uint block" } }));
    }

    TEST(GlslKernelTest, ImagesAndTexturesUseTheirOwnDeclarations)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .storage_image("src", BindingDirection::Input)
                                    .storage_image("dst", BindingDirection::Output)
                                    .texture("tex")
                                    .kernel(MF_KERNEL([](uint32_t i) { }))
                                    .build();

        EXPECT_TRUE(contains_all(emit_glsl_kernel(spec),
            { { "layout(set = 0, binding = 0, rgba32f) readonly uniform image2D src;", "input images are read only" },
                { "layout(set = 0, binding = 1, rgba32f) writeonly uniform image2D dst;", "output images are write only" },
                { "layout(set = 0, binding = 2) uniform sampler2D tex;", "sampled texture" } }));
    }

    TEST(GlslKernelTest, PushConstantsBecomeABlockAndLocalCopiesOfEachField)
    {
        const std::string text = emit_glsl_kernel(scale_kernel_spec());

        EXPECT_TRUE(contains_all(text,
            { { "layout(push_constant) uniform PC {\n    float gain;\n    uint count;\n} pc;", "block in declaration order" },
                { "    float gain = pc.gain;", "float field copied" },
                { "    uint count = pc.count;", "uint field copied" } }));
    }

    TEST(GlslKernelTest, NoPushConstantBlockIsEmittedWhenNoFieldIsDeclared)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("sig", BindingDirection::InOut, GpuDataFormat::FLOAT32)
                                    .kernel(MF_KERNEL([](float* sig, uint32_t i) { sig[i] = 0.0f; }))
                                    .build();

        EXPECT_TRUE(contains_none(emit_glsl_kernel(spec), { { "push_constant", "no fields declared" } }));
    }

    TEST(GlslKernelTest, IndexIsDeclaredAndCoordOnlyWhenAnImageIsBound)
    {
        const ShaderSpec buffer_only = scale_kernel_spec();
        const ShaderSpec with_image = ShaderSpec::Assemble {}
                                          .storage_image("dst", BindingDirection::Output)
                                          .kernel(MF_KERNEL([](uint32_t i) { }))
                                          .build();

        const std::string buffer_text = emit_glsl_kernel(buffer_only);
        const std::string image_text = emit_glsl_kernel(with_image);

        EXPECT_TRUE(contains_all(buffer_text, { { "    uint i = gl_GlobalInvocationID.x;", "linear index" } }));
        EXPECT_TRUE(contains_none(buffer_text, { { "ivec2 coord", "no image bound" } }));
        EXPECT_TRUE(contains_all(image_text,
            { { "    uint i = gl_GlobalInvocationID.x;", "linear index" },
                { "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);", "pixel coordinate" } }));
    }

    TEST(GlslKernelTest, KernelBodyFollowsThePushConstantLocalsAndClosesMain)
    {
        const std::string text = emit_glsl_kernel(scale_kernel_spec());

        const auto locals = text.find("    uint count = pc.count;");
        const auto body = text.find("sig[i] = sig[i] * gain;");

        ASSERT_NE(locals, std::string::npos);
        ASSERT_NE(body, std::string::npos);
        EXPECT_LT(locals, body);
        EXPECT_EQ(text.substr(text.size() - 3), "\n}\n");
    }

    TEST(GlslKernelTest, FunctionsKeepFullSignaturesAndComeAfterPushConstantsBeforeMain)
    {
        const ShaderSpec spec = ShaderSpec::Assemble {}
                                    .ssbo("pos", BindingDirection::InOut, GpuDataFormat::VEC4_F32)
                                    .pc("dt")
                                    .function("vec3", "swirl", "vec3 p",
                                        "    vec3 axis = vec3(0.0f, 1.0f, 0.0f);\n"
                                        "    return cross(axis, p) * 2.0f;")
                                    .function("float", "falloff", "vec3 p",
                                        "    float d = length(p) / 3.0f;\n"
                                        "    return 1.0f - smoothstep(0.2f, 0.9f, clamp(d, 0.0f, 1.0f));")
                                    .kernel(MF_KERNEL([](vec4* pos, float dt, uint32_t i) {
                                        vec3 p = pos[i].xyz;
                                        pos[i] = vec4(p + swirl(p) * falloff(p) * dt, 1.0f);
                                    }))
                                    .build();

        const std::string text = emit_glsl_kernel(spec);

        const auto push = text.find("layout(push_constant)");
        const auto swirl = text.find("vec3 swirl(vec3 p) {");
        const auto falloff = text.find("float falloff(vec3 p) {");
        const auto main = text.find("void main() {");

        EXPECT_TRUE(contains_all(text,
            { { "return cross(axis, p) * 2.0f;", "first body injected verbatim" },
                { "swirl(p) * falloff(p)", "kernel calls both by name" } }));
        EXPECT_TRUE(contains_none(text, { { "vec3 swirl(p)", "parameter types must be kept" } }));
        ASSERT_NE(swirl, std::string::npos);
        ASSERT_NE(falloff, std::string::npos);
        ASSERT_NE(push, std::string::npos);
        ASSERT_NE(main, std::string::npos);
        EXPECT_GT(swirl, push);
        EXPECT_LT(swirl, falloff);
        EXPECT_LT(falloff, main);
    }

} // namespace
} // namespace MayaFlux::Test
