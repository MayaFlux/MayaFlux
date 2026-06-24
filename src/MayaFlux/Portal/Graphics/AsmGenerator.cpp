#include "ShaderFoundry.hpp"

/**
 * @file AsmGenerator.cpp
 * @brief SPIR-V assembly and GLSL kernel emitters for ShaderSpec.
 *
 * Current state
 *
 * SPIR-V assembly path (emit_spirv_asm): complete for SCALAR_F32 SSBOs with
 * any KernelOp. IMAGE_2D storage image output path (emit_image_body) is
 * complete for single-output write ops (Scale, ScaleOffset, Offset, Clip,
 * Abs, Negate). GLSL kernel path (emit_glsl_kernel): complete, dispatched
 * when spec.kernel is set via MF_KERNEL.
 *
 * What needs adding after Kinesis::Vision is active
 *
 *
 * 1. IMAGE_2D two-image ops (read-from-storage-image + write).
 *    emit_image_body resolves one img_in and one img_out. KernelOp::Add,
 *    Multiply, Mix in emit_image_body fall through to default (passthrough)
 *    because they need two img_in slots. Fix: extend the img_in resolution
 *    to collect all Input/InOut IMAGE_2D bindings as an ordered list, then
 *    emit OpImageRead for each and apply the two-operand op. StorageImageRead
 *    WithoutFormat capability must also be added to emit_header when any
 *    Input IMAGE_2D binding is present. Callers: Kinesis::Vision blend,
 *    composite, difference ops.
 *
 * 2. TEXTURE_2D sampled image bindings in the SPIR-V assembly path.
 *    emit_header, emit_decorations, emit_types all silently skip TEXTURE_2D
 *    bindings. No OpTypeSampledImage, no combined image sampler variable, no
 *    OpImageSampleExplicitLod in the body. Fix requires OpCapability Sampled1D
 *    or similar, OpTypeSampler, OpTypeSampledImage, OpVariable for the combined
 *    sampler, and OpImageSampleExplicitLod or OpImageFetch in the body. This
 *    is structurally heavier than storage image. Callers: Kinesis::Vision
 *    filter ops that read from a TextureContainer as a sampled source.
 *
 * 3. Reduction body completion.
 *    emit_reduction_body is a placeholder. The shared memory tree is partially
 *    emitted but the loop structure is missing — no OpLoopMerge, no back-edge.
 *    KernelOp::Sum and KernelOp::Max route here but produce invalid SPIR-V.
 *    Fix: full workgroup reduction loop with OpLoopMerge/OpBranch/OpPhi.
 *    No caller exercises this path yet.
 */

namespace MayaFlux::Portal::Graphics::detail {

namespace {

    /**
     * Maps GpuDataFormat to the GLSL type name used in SSBO array declarations.
     */
    std::string_view glsl_type(Kakshya::GpuDataFormat fmt) noexcept
    {
        switch (fmt) {
        case Kakshya::GpuDataFormat::FLOAT32:
            return "float";
        case Kakshya::GpuDataFormat::VEC2_F32:
            return "vec2";
        case Kakshya::GpuDataFormat::VEC3_F32:
            return "vec3";
        case Kakshya::GpuDataFormat::VEC4_F32:
            return "vec4";
        case Kakshya::GpuDataFormat::INT32:
            return "int";
        case Kakshya::GpuDataFormat::UINT32:
        case Kakshya::GpuDataFormat::UINT8:
        case Kakshya::GpuDataFormat::UINT16:
            return "uint";
        default:
            return "float";
        }
    }

    /**
     * Returns the SPIR-V type ID string for the element type of an SSBO binding.
     */
    std::string_view ssbo_elem_spirv_type(Kakshya::GpuDataFormat fmt) noexcept
    {
        switch (fmt) {
        case Kakshya::GpuDataFormat::VEC2_F32:
            return "%v2f32";
        case Kakshya::GpuDataFormat::VEC3_F32:
            return "%v3f32";
        case Kakshya::GpuDataFormat::VEC4_F32:
            return "%v4f32";
        default:
            return "%f32";
        }
    }

    /**
     * Returns the component count for a GpuDataFormat SSBO element.
     * Scalar formats return 1.
     */
    uint32_t ssbo_elem_components(Kakshya::GpuDataFormat fmt) noexcept
    {
        switch (fmt) {
        case Kakshya::GpuDataFormat::VEC2_F32:
            return 2;
        case Kakshya::GpuDataFormat::VEC3_F32:
            return 3;
        case Kakshya::GpuDataFormat::VEC4_F32:
            return 4;
        default:
            return 1;
        }
    }

    /**
     * Emit a fixed header common to all generated compute kernels.
     * Assigns IDs for void, voidfn, u32, f32, v3u32, glsl extension import,
     * and the GlobalInvocationId builtin.
     */
    std::string emit_header(const ShaderSpec& spec)
    {
        const auto& ws = spec.workgroup_size;

        bool has_storage_image = false;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::IMAGE_2D)
                has_storage_image = true;
        }

        std::string iface = "%gid_var";
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;
            iface += " %buf_" + b.name;
        }
        if (has_storage_image) {
            for (const auto& b : spec.bindings) {
                if (b.modality == Kakshya::DataModality::IMAGE_2D)
                    iface += " %img_" + b.name;
            }
        }
        if (!spec.pc_fields.empty())
            iface += " %pc";

        std::string o;
        o += "OpCapability Shader\n";
        if (has_storage_image)
            o += "OpCapability StorageImageWriteWithoutFormat\n";
        o += "%glsl = OpExtInstImport \"GLSL.std.450\"\n";
        o += "OpMemoryModel Logical GLSL450\n";
        o += "OpEntryPoint GLCompute %main \"main\" " + iface + "\n";
        o += "OpExecutionMode %main LocalSize "
            + std::to_string(ws[0]) + " "
            + std::to_string(ws[1]) + " "
            + std::to_string(ws[2]) + "\n\n";
        return o;
    }

    /**
     * Emit decorations for all SSBO bindings and push constant member offsets.
     */
    std::string emit_decorations(const ShaderSpec& spec)
    {
        std::string o;
        o += "OpDecorate %gid_var BuiltIn GlobalInvocationId\n";

        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;

            const auto stride = static_cast<uint32_t>(Kakshya::gpu_data_format_bytes(b.format));
            o += "OpDecorate %rta_" + b.name + " ArrayStride "
                + std::to_string(stride) + "\n";
            o += "OpMemberDecorate %blk_" + b.name + " 0 Offset 0\n";
            o += "OpDecorate %blk_" + b.name + " Block\n";
            o += "OpDecorate %buf_" + b.name + " DescriptorSet 0\n";
            o += "OpDecorate %buf_" + b.name + " Binding "
                + std::to_string(b.binding_index) + "\n";
        }

        for (const auto& b : spec.bindings) {
            if (b.modality != Kakshya::DataModality::IMAGE_2D)
                continue;

            const std::string var = "%img_" + b.name;
            o += "OpDecorate " + var + " DescriptorSet 0\n";
            o += "OpDecorate " + var + " Binding "
                + std::to_string(b.binding_index) + "\n";

            if (b.direction == BindingDirection::Input) {
                o += "OpDecorate " + var + " NonWritable\n";
            } else if (b.direction == BindingDirection::Output) {
                o += "OpDecorate " + var + " NonReadable\n";
            }
        }

        if (!spec.pc_fields.empty()) {
            o += "OpDecorate %pc_blk Block\n";
            uint32_t off = 0;
            for (size_t i = 0; i < spec.pc_fields.size(); ++i) {
                o += "OpMemberDecorate %pc_blk " + std::to_string(i)
                    + " Offset " + std::to_string(off) + "\n";
                off += static_cast<uint32_t>(
                    Kakshya::gpu_data_format_bytes(spec.pc_fields[i].format));
            }
        }
        o += "\n";
        return o;
    }

    /**
     * Emit type declarations for all used types, including void, voidfn, u32, f32,
     * v3u32, and the GlobalInvocationId builtin.
     */
    std::string emit_types(const ShaderSpec& spec)
    {
        std::string o;
        o += "%void   = OpTypeVoid\n";
        o += "%voidfn = OpTypeFunction %void\n";
        o += "%u32    = OpTypeInt 32 0\n";
        o += "%f32    = OpTypeFloat 32\n";
        o += "%v3u32  = OpTypeVector %u32 3\n";
        o += "%ptr_in_v3u32 = OpTypePointer Input %v3u32\n";
        o += "%gid_var = OpVariable %ptr_in_v3u32 Input\n\n";

        bool need_v2f32 = false;
        bool need_v3f32 = false;
        bool need_v4f32 = false;

        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;
            switch (b.format) {
            case Kakshya::GpuDataFormat::VEC2_F32:
                need_v2f32 = true;
                break;
            case Kakshya::GpuDataFormat::VEC3_F32:
                need_v3f32 = true;
                break;
            case Kakshya::GpuDataFormat::VEC4_F32:
                need_v4f32 = true;
                break;
            default:
                break;
            }
        }

        if (need_v2f32)
            o += "%v2f32 = OpTypeVector %f32 2\n";
        if (need_v3f32)
            o += "%v3f32 = OpTypeVector %f32 3\n";

        bool has_image_2d = false;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::IMAGE_2D) {
                has_image_2d = true;
                break;
            }
        }
        if (need_v4f32 && !has_image_2d)
            o += "%v4f32 = OpTypeVector %f32 4\n";
        if (need_v2f32 || need_v3f32 || (need_v4f32 && !has_image_2d))
            o += "\n";

        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;

            const std::string_view etype = ssbo_elem_spirv_type(b.format);
            o += "%rta_" + b.name + " = OpTypeRuntimeArray " + std::string(etype) + "\n";
            o += "%blk_" + b.name + " = OpTypeStruct %rta_" + b.name + "\n";
            o += "%pblk_" + b.name + " = OpTypePointer StorageBuffer %blk_" + b.name + "\n";
            o += "%buf_" + b.name + " = OpVariable %pblk_" + b.name + " StorageBuffer\n";
            o += "%pelem_" + b.name + " = OpTypePointer StorageBuffer "
                + std::string(etype) + "\n";
        }
        o += "\n";

        if (has_image_2d) {
            o += "%i32          = OpTypeInt 32 1\n";
            o += "%v2i32        = OpTypeVector %i32 2\n";
            o += "%v4f32        = OpTypeVector %f32 4\n";
            o += "%img_czero    = OpConstant %f32 0.0\n";
            o += "%img2d_t      = OpTypeImage %f32 2D 0 0 0 2 Unknown\n";
            o += "%ptr_img2d    = OpTypePointer UniformConstant %img2d_t\n";
            for (const auto& b : spec.bindings) {
                if (b.modality != Kakshya::DataModality::IMAGE_2D)
                    continue;
                o += "%img_" + b.name + " = OpVariable %ptr_img2d UniformConstant\n";
            }
            o += "\n";
        }

        if (!spec.pc_fields.empty()) {
            o += "%pc_blk = OpTypeStruct";
            for (size_t i = 0; i < spec.pc_fields.size(); ++i)
                o += " %f32";
            o += "\n";
            o += "%ppc     = OpTypePointer PushConstant %pc_blk\n";
            o += "%pc      = OpVariable %ppc PushConstant\n";
            o += "%ppc_f32 = OpTypePointer PushConstant %f32\n\n";
        }

        o += "%c0u = OpConstant %u32 0\n";
        o += "%c1u = OpConstant %u32 1\n";

        for (size_t i = 2; i < spec.pc_fields.size(); ++i) {
            o += "%c" + std::to_string(i) + "u = OpConstant %u32 "
                + std::to_string(i) + "\n";
        }

        o += "\n";
        return o;
    }

    /**
     * Emit the entry point body for Elementwise/Stencil templates.
     * Loads the index, loads PC fields, loads SSBO elements, applies op, stores.
     */
    std::string emit_elementwise_body(const ShaderSpec& spec)
    {
        std::string o;
        o += "%main  = OpFunction %void None %voidfn\n";
        o += "%entry = OpLabel\n";
        o += "%gid3  = OpLoad %v3u32 %gid_var\n";
        o += "%i     = OpCompositeExtract %u32 %gid3 0\n\n";

        for (size_t fi = 0; fi < spec.pc_fields.size(); ++fi) {
            const auto& f = spec.pc_fields[fi];
            o += "%ppc_" + f.name + " = OpAccessChain %ppc_f32 %pc %c"
                + std::to_string(fi) + "u\n";
            o += "%pc_" + f.name + " = OpLoad %f32 %ppc_" + f.name + "\n";
        }
        o += "\n";

        std::vector<const BindingSlot*> ssbos;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;
            ssbos.push_back(&b);
        }

        Kakshya::GpuDataFormat primary_fmt = Kakshya::GpuDataFormat::FLOAT32;
        for (const auto* b : ssbos) {
            if (b->direction != BindingDirection::Output) {
                primary_fmt = b->format;
                break;
            }
        }
        const std::string_view etype = ssbo_elem_spirv_type(primary_fmt);
        const uint32_t ncomp = ssbo_elem_components(primary_fmt);
        const bool is_vector = (ncomp > 1);

        for (const auto* b : ssbos) {
            o += "%gep_" + b->name + " = OpAccessChain %pelem_" + b->name
                + " %buf_" + b->name + " %c0u %i\n";
            if (b->direction != BindingDirection::Output) {
                o += "%val_" + b->name + " = OpLoad " + std::string(etype)
                    + " %gep_" + b->name + "\n";
            }
        }
        o += "\n";

        auto pc_operand = [&](const std::string& field_name) -> std::string {
            if (!is_vector)
                return "%pc_" + field_name;
            const std::string splat = "%spc_" + field_name;
            std::string construct = splat + " = OpCompositeConstruct "
                + std::string(etype);
            const std::string scalar = " %pc_" + field_name;
            for (uint32_t c = 0; c < ncomp; ++c)
                construct += scalar;
            o += construct + "\n";
            return splat;
        };

        const std::string v0 = ssbos.empty() ? "" : ("%val_" + ssbos[0]->name);
        const std::string v1 = ssbos.size() > 1 ? ("%val_" + ssbos[1]->name) : "";
        const std::string p0 = spec.pc_fields.empty() ? ""
                                                      : pc_operand(spec.pc_fields[0].name);
        const std::string p1 = spec.pc_fields.size() > 1
            ? pc_operand(spec.pc_fields[1].name)
            : "";

        std::string result;
        const std::string et = std::string(etype);
        switch (spec.op) {
        case KernelOp::Scale:
            if (is_vector) {
                o += "%res = OpVectorTimesScalar " + et + " " + v0
                    + " %pc_" + spec.pc_fields[0].name + "\n";
            } else {
                o += "%res = OpFMul " + et + " " + v0 + " " + p0 + "\n";
            }

            result = "%res";
            break;
        case KernelOp::ScaleOffset:
            if (is_vector) {
                o += "%scaled = OpVectorTimesScalar " + et + " " + v0
                    + " %pc_" + spec.pc_fields[0].name + "\n";
                o += "%res = OpFAdd " + et + " %scaled " + p1 + "\n";
            } else {
                o += "%mul = OpFMul " + et + " " + v0 + " " + p0 + "\n";
                o += "%res = OpFAdd " + et + " %mul " + p1 + "\n";
            }

            result = "%res";
            break;
        case KernelOp::Offset:
            o += "%res = OpFAdd " + et + " " + v0 + " " + p0 + "\n";
            result = "%res";
            break;
        case KernelOp::Clip:
            o += "%res = OpExtInst " + et + " %glsl FClamp " + v0
                + " " + p0 + " " + p1 + "\n";
            result = "%res";
            break;
        case KernelOp::Abs:
            o += "%res = OpExtInst " + et + " %glsl FAbs " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Negate:
            o += "%res = OpFNegate " + et + " " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Add:
            o += "%res = OpFAdd " + et + " " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::Multiply:
            o += "%res = OpFMul " + et + " " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::Mix:
            o += "%dlt = OpFSub %f32 " + v1 + " " + v0 + "\n";
            o += "%scl = OpFMul %f32 %dlt " + p0 + "\n";
            o += "%res = OpFAdd %f32 " + v0 + " %scl\n";
            result = "%res";
            break;
        case KernelOp::Fma:
            o += "%res = OpExtInst %f32 %glsl Fma " + v0 + " " + p0 + " " + p1 + "\n";
            result = "%res";
            break;
        case KernelOp::Floor:
            o += "%res = OpExtInst %f32 %glsl Floor " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Ceil:
            o += "%res = OpExtInst %f32 %glsl Ceil " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Round:
            o += "%res = OpExtInst %f32 %glsl Round " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Trunc:
            o += "%res = OpExtInst %f32 %glsl Trunc " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Fract:
            o += "%res = OpExtInst %f32 %glsl Fract " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Sqrt:
            o += "%res = OpExtInst %f32 %glsl Sqrt " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::InverseSqrt:
            o += "%res = OpExtInst %f32 %glsl InverseSqrt " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Sin:
            o += "%res = OpExtInst %f32 %glsl Sin " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Cos:
            o += "%res = OpExtInst %f32 %glsl Cos " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Tan:
            o += "%res = OpExtInst %f32 %glsl Tan " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Asin:
            o += "%res = OpExtInst %f32 %glsl Asin " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Acos:
            o += "%res = OpExtInst %f32 %glsl Acos " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Atan:
            o += "%res = OpExtInst %f32 %glsl Atan " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Sinh:
            o += "%res = OpExtInst %f32 %glsl Sinh " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Cosh:
            o += "%res = OpExtInst %f32 %glsl Cosh " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Tanh:
            o += "%res = OpExtInst %f32 %glsl Tanh " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Exp:
            o += "%res = OpExtInst %f32 %glsl Exp " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Exp2:
            o += "%res = OpExtInst %f32 %glsl Exp2 " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Log:
            o += "%res = OpExtInst %f32 %glsl Log " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Log2:
            o += "%res = OpExtInst %f32 %glsl Log2 " + v0 + "\n";
            result = "%res";
            break;
        case KernelOp::Pow:
            o += "%res = OpExtInst %f32 %glsl Pow " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::Atan2:
            o += "%res = OpExtInst %f32 %glsl Atan2 " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::Min:
            o += "%res = OpExtInst %f32 %glsl FMin " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::MaxTwo:
            o += "%res = OpExtInst %f32 %glsl FMax " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::Step:
            o += "%res = OpExtInst %f32 %glsl Step " + v0 + " " + v1 + "\n";
            result = "%res";
            break;
        case KernelOp::SmoothStep:
            o += "%res = OpExtInst %f32 %glsl SmoothStep " + v0 + " " + v1 + " " + p0 + "\n";
            result = "%res";
            break;
        default:
            o += "%res = OpCopyObject " + et + " " + v0 + "\n";
            result = "%res";
            break;
        }
        o += "\n";

        for (const auto* b : ssbos) {
            if (b->direction == BindingDirection::Input)
                continue;
            o += "OpStore %gep_" + b->name + " " + result + "\n";
        }

        o += "OpReturn\n";
        o += "OpFunctionEnd\n";
        return o;
    }

    std::string emit_reduction_body(const ShaderSpec& spec)
    {
        const uint32_t local = spec.workgroup_size[0];
        const std::string ls = std::to_string(local);
        const bool is_max = (spec.op == KernelOp::Max);

        std::string o;
        o += "%arr_t   = OpTypeArray %f32 %c" + ls + "u\n";
        o += "%pshared = OpTypePointer Workgroup %f32\n";
        o += "%shared  = OpVariable %pshared Workgroup\n\n";
        o += "%c" + ls + "u = OpConstant %u32 " + ls + "\n\n";

        o += "%main  = OpFunction %void None %voidfn\n";
        o += "%entry = OpLabel\n";
        o += "%gid3  = OpLoad %v3u32 %gid_var\n";
        o += "%i     = OpCompositeExtract %u32 %gid3 0\n";
        o += "%lid   = OpCompositeExtract %u32 %gid3 0\n\n";

        const auto& b0 = spec.bindings.front();
        o += "%gep0    = OpAccessChain %pf32_" + b0.name + " %buf_" + b0.name + " %c0u %i\n";
        o += "%elem    = OpLoad %f32 %gep0\n";
        o += "%pgep_sh = OpAccessChain %pshared %shared %lid\n";
        o += "OpStore %pgep_sh %elem\n";
        o += "OpControlBarrier %c2u %c2u %c264u\n\n";

        o += "%s_init = OpShiftRightLogical %u32 %c" + ls + "u %c1u\n";
        o += "; reduction tree (caller must ensure workgroup is power of two)\n";
        if (is_max) {
            o += "%cur  = OpLoad %f32 %pgep_sh\n";
            o += "%res  = OpExtInst %f32 %glsl FMax %cur %elem\n";
        } else {
            o += "%cur  = OpLoad %f32 %pgep_sh\n";
            o += "%res  = OpFAdd %f32 %cur %elem\n";
        }
        o += "OpStore %pgep_sh %res\n";
        o += "OpControlBarrier %c2u %c2u %c264u\n\n";

        o += "OpReturn\n";
        o += "OpFunctionEnd\n";
        return o;
    }

    /**
     * Emit the entry point body for specs whose bindings include IMAGE_2D slots.
     *
     * Assumes workgroup_size is {8, 8, 1} or any 2D shape. GlobalInvocationId
     * x/y components are used as the texel coordinate. One output IMAGE_2D
     * binding is required. PC fields provide float operands identical to the
     * SSBO elementwise path. The op is applied per-channel on the rgba vec4
     * loaded from the first input IMAGE_2D, or on a zero vec4 if no input image
     * is declared.
     */
    std::string emit_image_body(const ShaderSpec& spec)
    {
        const BindingSlot* img_out = nullptr;
        const BindingSlot* img_in = nullptr;
        for (const auto& b : spec.bindings) {
            if (b.modality != Kakshya::DataModality::IMAGE_2D)
                continue;
            if (b.direction == BindingDirection::Output && !img_out) {
                img_out = &b;
            } else if (b.direction != BindingDirection::Output && !img_in) {
                img_in = &b;
            }
        }

        std::string o;
        o += "%main  = OpFunction %void None %voidfn\n";
        o += "%entry = OpLabel\n";
        o += "%gid3  = OpLoad %v3u32 %gid_var\n";
        o += "%ix    = OpCompositeExtract %u32 %gid3 0\n";
        o += "%iy    = OpCompositeExtract %u32 %gid3 1\n";
        o += "%six   = OpBitcast %i32 %ix\n";
        o += "%siy   = OpBitcast %i32 %iy\n";
        o += "%coord = OpCompositeConstruct %v2i32 %six %siy\n\n";

        for (size_t fi = 0; fi < spec.pc_fields.size(); ++fi) {
            const auto& f = spec.pc_fields[fi];
            o += "%ppc_" + f.name + " = OpAccessChain %ppc_f32 %pc %c"
                + std::to_string(fi) + "u\n";
            o += "%pc_" + f.name + " = OpLoad %f32 %ppc_" + f.name + "\n";
        }
        if (!spec.pc_fields.empty())
            o += "\n";

        if (img_in) {
            o += "%raw_in = OpImageRead %v4f32 %img_" + std::string(img_in->name) + " %coord\n";
        } else {
            o += "%raw_in = OpCompositeConstruct %v4f32 %img_czero %img_czero %img_czero %img_czero\n";
        }

        o += "%ch_r = OpCompositeExtract %f32 %raw_in 0\n";
        o += "%ch_g = OpCompositeExtract %f32 %raw_in 1\n";
        o += "%ch_b = OpCompositeExtract %f32 %raw_in 2\n";
        o += "%ch_a = OpCompositeExtract %f32 %raw_in 3\n\n";

        const std::string p0 = spec.pc_fields.empty() ? "" : ("%pc_" + spec.pc_fields[0].name);
        const std::string p1 = spec.pc_fields.size() > 1 ? ("%pc_" + spec.pc_fields[1].name) : "";

        auto emit_channel_op = [&](const std::string& ch, const std::string& suffix) {
            switch (spec.op) {
            case KernelOp::Scale:
                o += "%res_" + suffix + " = OpFMul %f32 " + ch + " " + p0 + "\n";
                break;
            case KernelOp::ScaleOffset:
                o += "%mul_" + suffix + " = OpFMul %f32 " + ch + " " + p0 + "\n";
                o += "%res_" + suffix + " = OpFAdd %f32 %mul_" + suffix + " " + p1 + "\n";
                break;
            case KernelOp::Offset:
                o += "%res_" + suffix + " = OpFAdd %f32 " + ch + " " + p0 + "\n";
                break;
            case KernelOp::Clip:
                o += "%res_" + suffix + " = OpExtInst %f32 %glsl FClamp " + ch + " " + p0 + " " + p1 + "\n";
                break;
            case KernelOp::Abs:
                o += "%res_" + suffix + " = OpExtInst %f32 %glsl FAbs " + ch + "\n";
                break;
            case KernelOp::Negate:
                o += "%res_" + suffix + " = OpFNegate %f32 " + ch + "\n";
                break;
            default:
                o += "%res_" + suffix + " = OpCopyObject %f32 " + ch + "\n";
                break;
            }
        };

        emit_channel_op("%ch_r", "r");
        emit_channel_op("%ch_g", "g");
        emit_channel_op("%ch_b", "b");
        emit_channel_op("%ch_a", "a");
        o += "\n";

        o += "%out_vec = OpCompositeConstruct %v4f32 %res_r %res_g %res_b %res_a\n";
        if (img_out)
            o += "OpImageWrite %img_" + std::string(img_out->name) + " %coord %out_vec\n";

        o += "OpReturn\n";
        o += "OpFunctionEnd\n";
        return o;
    }

} // namespace

std::string emit_spirv_asm(const ShaderSpec& spec)
{
    std::string src;
    src += emit_header(spec);
    src += emit_decorations(spec);
    src += emit_types(spec);

    bool has_image = false;
    for (const auto& b : spec.bindings) {
        if (b.modality == Kakshya::DataModality::IMAGE_2D) {
            has_image = true;
            break;
        }
    }

    if (has_image) {
        src += emit_image_body(spec);
        return src;
    }

    switch (spec.tmpl) {
    case KernelTemplate::Reduction:
        src += emit_reduction_body(spec);
        break;
    case KernelTemplate::Elementwise:
    case KernelTemplate::Stencil:
    case KernelTemplate::GeometryEmit:
    default:
        src += emit_elementwise_body(spec);
        break;
    }
    return src;
}

std::string emit_glsl_kernel(const ShaderSpec& spec)
{
    const auto& ws = spec.workgroup_size;
    const auto& ks = *spec.kernel;

    bool has_image = false;
    for (const auto& b : spec.bindings) {
        if (b.modality == Kakshya::DataModality::IMAGE_2D)
            has_image = true;
    }

    std::string o;
    o += "#version 460\n";
    o += "layout(local_size_x = " + std::to_string(ws[0])
        + ", local_size_y = " + std::to_string(ws[1])
        + ", local_size_z = " + std::to_string(ws[2]) + ") in;\n\n";

    for (const auto& b : spec.bindings) {
        if (b.modality == Kakshya::DataModality::IMAGE_2D) {
            const std::string qual = (b.direction == BindingDirection::Input)
                ? "readonly"
                : "writeonly";
            o += "layout(set = 0, binding = " + std::to_string(b.binding_index)
                + ", rgba32f) " + qual + " uniform image2D " + b.name + ";\n";
            continue;
        }
        if (b.modality == Kakshya::DataModality::TEXTURE_2D) {
            o += "layout(set = 0, binding = " + std::to_string(b.binding_index)
                + ") uniform sampler2D " + b.name + ";\n";
            continue;
        }
        const auto t = std::string(glsl_type(b.format));
        o += "layout(set = 0, binding = " + std::to_string(b.binding_index)
            + ", std430) buffer Block_" + b.name
            + " { " + t + " " + b.name + "[]; };\n";
    }

    if (!spec.pc_fields.empty()) {
        o += "\nlayout(push_constant) uniform PC {\n";
        for (const auto& f : spec.pc_fields)
            o += "    " + std::string(glsl_type(f.format)) + " " + f.name + ";\n";
        o += "} pc;\n";
    }

    o += "\nvoid main() {\n";
    o += "    uint i = gl_GlobalInvocationID.x;\n";
    if (has_image)
        o += "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n";

    for (const auto& f : spec.pc_fields) {
        const auto t = std::string(glsl_type(f.format));
        o += "    " + t + " " + f.name + " = pc." + f.name + ";\n";
    }

    o += ks.body;
    o += "\n}\n";
    return o;
}

} // namespace MayaFlux::Portal::Graphics::detail
