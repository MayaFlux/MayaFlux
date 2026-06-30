#include "ShaderFoundry.hpp"

/**
 * @file AsmGenerator.cpp
 * @brief SPIR-V assembly and GLSL kernel emitters for ShaderSpec.
 *
 * The assembly path (emit_spirv_asm) covers all binding modalities and
 * kernel templates:
 *
 * SSBO (SCALAR_F32, VEC2_F32, VEC3_F32, VEC4_F32) — all KernelOps via
 * emit_elementwise_body. ArrayStride, element type, and pointer type derived
 * from GpuDataFormat. Scalar PC operands splatted to vector width via
 * OpCompositeConstruct before arithmetic.
 *
 * IMAGE_2D storage images — emit_image_body handles any number of Input/InOut
 * images and one Output image with all single- and two-operand KernelOps
 * applied per channel.
 *
 * TEXTURE_2D sampled images — emit_image_body loads each binding via
 * OpTypeSampledImage / OpImageSampleExplicitLod at normalised UV derived
 * from GlobalInvocationID divided by width/height PC fields (first two PC
 * fields by convention when TEXTURE_2D bindings are present).
 *
 * KernelTemplate::Reduction — emit_reduction_body emits a structured
 * workgroup tree reduction with OpLoopMerge, OpPhi for the stride variable,
 * OpULessThan active-thread guard, and two OpControlBarrier synchronisation
 * points per iteration. Supports KernelOp::Sum and KernelOp::Max.
 *
 * GLSL kernel path (emit_glsl_kernel) handles all three binding modalities
 * when spec.kernel is set via MF_KERNEL.
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
        case Kakshya::GpuDataFormat::UINT32:
        case Kakshya::GpuDataFormat::UINT8:
        case Kakshya::GpuDataFormat::UINT16:
            return "%u32";
        case Kakshya::GpuDataFormat::INT32:
            return "%i32";
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
        bool has_input_image = false;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::IMAGE_2D) {
                has_storage_image = true;
                if (b.direction != BindingDirection::Output)
                    has_input_image = true;
            }
        }

        std::string iface = "%gid_var";
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;
            if (b.modality == Kakshya::DataModality::TEXTURE_2D) {
                iface += " %tex_" + b.name;
                continue;
            }
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

        if (spec.tmpl == KernelTemplate::Reduction)
            iface += " %lid_var";

        std::string o;
        o += "OpCapability Shader\n";

        if (has_storage_image)
            o += "OpCapability StorageImageWriteWithoutFormat\n";
        if (has_input_image)
            o += "OpCapability StorageImageReadWithoutFormat\n";

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

        for (const auto& b : spec.bindings) {
            if (b.modality != Kakshya::DataModality::TEXTURE_2D)
                continue;
            o += "OpDecorate %tex_" + b.name + " DescriptorSet 0\n";
            o += "OpDecorate %tex_" + b.name + " Binding "
                + std::to_string(b.binding_index) + "\n";
        }

        if (spec.tmpl == KernelTemplate::Reduction)
            o += "OpDecorate %lid_var BuiltIn LocalInvocationId\n";

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

        bool need_i32 = false;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D
                || b.modality == Kakshya::DataModality::IMAGE_2D)
                continue;
            if (b.format == Kakshya::GpuDataFormat::INT32)
                need_i32 = true;
        }

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
        if (need_i32 && !has_image_2d)
            o += "%i32 = OpTypeInt 32 1\n";

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

        bool has_texture_2d = false;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D) {
                has_texture_2d = true;
                break;
            }
        }

        if (has_texture_2d) {
            if (!need_v2f32 && !has_image_2d)
                o += "%v2f32      = OpTypeVector %f32 2\n";
            o += "%sampler_t  = OpTypeSampler\n";
            o += "%img2d_s_t  = OpTypeImage %f32 2D 0 0 0 1 Unknown\n";
            o += "%simgc_t    = OpTypeSampledImage %img2d_s_t\n";
            o += "%ptr_simg   = OpTypePointer UniformConstant %simgc_t\n";
            for (const auto& b : spec.bindings) {
                if (b.modality != Kakshya::DataModality::TEXTURE_2D)
                    continue;
                o += "%tex_" + b.name + " = OpVariable %ptr_simg UniformConstant\n";
            }
            o += "%lod_zero   = OpConstant %f32 0.0\n";
            o += "\n";
        }

        if (spec.tmpl == KernelTemplate::Reduction || spec.tmpl == KernelTemplate::BitonicSort) {
            const uint32_t local = spec.workgroup_size[0];
            const std::string ls = std::to_string(local);
            o += "%bool        = OpTypeBool\n";
            o += "%lid_var  = OpVariable %ptr_in_v3u32 Input\n";
            o += "%arr_sh_t    = OpTypeArray %f32 %c" + ls + "u\n";
            o += "%psh_arr     = OpTypePointer Workgroup %arr_sh_t\n";
            o += "%shared      = OpVariable %psh_arr Workgroup\n";
            o += "%psh_f32     = OpTypePointer Workgroup %f32\n";
            o += "%c" + ls + "u   = OpConstant %u32 " + ls + "\n";
            o += "%c2u         = OpConstant %u32 2\n";
            o += "%c264u       = OpConstant %u32 264\n"; // AcquireRelease | WorkgroupMemory
            o += "\n";
        }

        if (!spec.pc_fields.empty()) {
            bool pc_has_uint = false;
            bool pc_has_int = false;
            o += "%pc_blk = OpTypeStruct";
            for (const auto& f : spec.pc_fields) {
                o += " " + std::string(ssbo_elem_spirv_type(f.format));
                if (f.format == Kakshya::GpuDataFormat::UINT32) {
                    pc_has_uint = true;
                } else if (f.format == Kakshya::GpuDataFormat::INT32) {
                    pc_has_int = true;
                }
            }
            o += "\n";
            o += "%ppc     = OpTypePointer PushConstant %pc_blk\n";
            o += "%pc      = OpVariable %ppc PushConstant\n";
            o += "%ppc_f32 = OpTypePointer PushConstant %f32\n";
            if (pc_has_uint)
                o += "%ppc_u32 = OpTypePointer PushConstant %u32\n";
            if (pc_has_int)
                o += "%ppc_i32 = OpTypePointer PushConstant %i32\n";
            o += "\n";
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
            const std::string_view pptr = (f.format == Kakshya::GpuDataFormat::UINT32)
                ? "%ppc_u32"
                : (f.format == Kakshya::GpuDataFormat::INT32 ? "%ppc_i32" : "%ppc_f32");
            const std::string_view ld_type = ssbo_elem_spirv_type(f.format);
            o += "%ppc_" + f.name + " = OpAccessChain " + std::string(pptr)
                + " %pc %c" + std::to_string(fi) + "u\n";
            o += "%pc_" + f.name + " = OpLoad " + std::string(ld_type)
                + " %ppc_" + f.name + "\n";
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
        case KernelOp::Sub:
            o += "%res = OpFSub " + et + " " + v0 + " " + v1 + "\n";
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
        const auto& b0 = spec.bindings.front();

        std::string o;
        o += "%main    = OpFunction %void None %voidfn\n";
        o += "%entry   = OpLabel\n";

        o += "%gid3    = OpLoad %v3u32 %gid_var\n";
        o += "%i       = OpCompositeExtract %u32 %gid3 0\n";
        o += "%lid3    = OpLoad %v3u32 %lid_var\n";
        o += "%lid     = OpCompositeExtract %u32 %lid3 0\n\n";

        o += "%gep_in  = OpAccessChain %pelem_" + b0.name
            + " %buf_" + b0.name + " %c0u %i\n";
        o += "%elem    = OpLoad %f32 %gep_in\n";
        o += "%pgsh    = OpAccessChain %psh_f32 %shared %lid\n";
        o += "OpStore %pgsh %elem\n";
        o += "OpControlBarrier %c2u %c2u %c264u\n\n";

        o += "%s_init  = OpShiftRightLogical %u32 %c" + ls + "u %c1u\n";
        o += "OpBranch %loop_hdr\n\n";

        o += "%loop_hdr = OpLabel\n";
        o += "%stride  = OpPhi %u32 %s_init %entry %s_next %loop_cont\n";
        o += "OpLoopMerge %loop_merge %loop_cont None\n";
        o += "OpBranch %loop_body\n\n";

        o += "%loop_body = OpLabel\n";
        o += "%active  = OpULessThan %bool %lid %stride\n";
        o += "OpSelectionMerge %sel_merge None\n";
        o += "OpBranchConditional %active %do_op %sel_merge\n\n";

        o += "%do_op   = OpLabel\n";
        o += "%pgsh_a  = OpAccessChain %psh_f32 %shared %lid\n";
        o += "%a       = OpLoad %f32 %pgsh_a\n";
        o += "%lid_b   = OpIAdd %u32 %lid %stride\n";
        o += "%pgsh_b  = OpAccessChain %psh_f32 %shared %lid_b\n";
        o += "%b       = OpLoad %f32 %pgsh_b\n";

        if (is_max) {
            o += "%combined = OpExtInst %f32 %glsl FMax %a %b\n";
        } else {
            o += "%combined = OpFAdd %f32 %a %b\n";
        }

        o += "OpStore %pgsh_a %combined\n";
        o += "OpBranch %sel_merge\n\n";

        o += "%sel_merge = OpLabel\n";
        o += "OpBranch %loop_cont\n\n";

        o += "%loop_cont = OpLabel\n";
        o += "OpControlBarrier %c2u %c2u %c264u\n";
        o += "%s_next  = OpShiftRightLogical %u32 %stride %c1u\n";
        o += "%done    = OpIEqual %bool %s_next %c0u\n";
        o += "OpBranchConditional %done %loop_merge %loop_hdr\n\n";

        o += "%loop_merge = OpLabel\n";
        o += "%is_zero = OpIEqual %bool %lid %c0u\n";
        o += "OpSelectionMerge %write_merge None\n";
        o += "OpBranchConditional %is_zero %do_write %write_merge\n\n";

        o += "%do_write = OpLabel\n";
        o += "%result  = OpLoad %f32 %pgsh\n";
        o += "%gep_out = OpAccessChain %pelem_" + b0.name
            + " %buf_" + b0.name + " %c0u %c0u\n";
        o += "OpStore %gep_out %result\n";
        o += "OpBranch %write_merge\n\n";

        o += "%write_merge = OpLabel\n";
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
        std::vector<const BindingSlot*> img_inputs;
        for (const auto& b : spec.bindings) {
            if (b.modality != Kakshya::DataModality::IMAGE_2D)
                continue;
            if (b.direction == BindingDirection::Output && !img_out) {
                img_out = &b;
            } else if (b.direction != BindingDirection::Output) {
                img_inputs.push_back(&b);
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

        std::vector<const BindingSlot*> tex_inputs;
        for (const auto& b : spec.bindings) {
            if (b.modality == Kakshya::DataModality::TEXTURE_2D)
                tex_inputs.push_back(&b);
        }

        if (!tex_inputs.empty()) {
            const std::string pw = spec.pc_fields.size() > 0
                ? ("%pc_" + spec.pc_fields[0].name)
                : "%lod_zero";
            const std::string ph = spec.pc_fields.size() > 1
                ? ("%pc_" + spec.pc_fields[1].name)
                : "%lod_zero";
            o += "%fix = OpConvertUToF %f32 %ix\n";
            o += "%fiy = OpConvertUToF %f32 %iy\n";
            o += "%u   = OpFDiv %f32 %fix " + pw + "\n";
            o += "%v   = OpFDiv %f32 %fiy " + ph + "\n";
            o += "%uv  = OpCompositeConstruct %v2f32 %u %v\n\n";

            for (size_t ti = 0; ti < tex_inputs.size(); ++ti) {
                const std::string idx = std::to_string(img_inputs.size() + ti);
                o += "%simg_" + tex_inputs[ti]->name
                    + " = OpLoad %simgc_t %tex_" + tex_inputs[ti]->name + "\n";
                o += "%raw_in" + idx + " = OpImageSampleExplicitLod %v4f32 %simg_"
                    + tex_inputs[ti]->name + " %uv Lod %lod_zero\n";
            }
            o += "\n";
        }

        for (size_t ii = 0; ii < img_inputs.size(); ++ii) {
            o += "%raw_in" + std::to_string(ii) + " = OpImageRead %v4f32 %img_"
                + img_inputs[ii]->name + " %coord\n";
        }
        if (img_inputs.empty()) {
            o += "%raw_in0 = OpCompositeConstruct %v4f32 %img_czero %img_czero"
                 " %img_czero %img_czero\n";
        }
        o += "\n";

        o += "%ch0_r = OpCompositeExtract %f32 %raw_in0 0\n";
        o += "%ch0_g = OpCompositeExtract %f32 %raw_in0 1\n";
        o += "%ch0_b = OpCompositeExtract %f32 %raw_in0 2\n";
        o += "%ch0_a = OpCompositeExtract %f32 %raw_in0 3\n";

        const bool has_second = img_inputs.size() > 1
            || (!img_inputs.empty() && !tex_inputs.empty())
            || tex_inputs.size() > 1;

        const std::string second_idx = img_inputs.size() > 1
            ? "1"
            : (!tex_inputs.empty() ? std::to_string(img_inputs.size()) : "");

        if (has_second && !second_idx.empty()) {
            o += "%ch1_r = OpCompositeExtract %f32 %raw_in" + second_idx + " 0\n";
            o += "%ch1_g = OpCompositeExtract %f32 %raw_in" + second_idx + " 1\n";
            o += "%ch1_b = OpCompositeExtract %f32 %raw_in" + second_idx + " 2\n";
            o += "%ch1_a = OpCompositeExtract %f32 %raw_in" + second_idx + " 3\n";
        }
        o += "\n";

        const std::string p0 = spec.pc_fields.empty()
            ? ""
            : ("%pc_" + spec.pc_fields[0].name);
        const std::string p1 = spec.pc_fields.size() > 1
            ? ("%pc_" + spec.pc_fields[1].name)
            : "";

        auto emit_channel_op = [&](
                                   const std::string& c0, const std::string& c1,
                                   const std::string& suffix) {
            switch (spec.op) {
            case KernelOp::Scale:
                o += "%res_" + suffix + " = OpFMul %f32 " + c0 + " " + p0 + "\n";
                break;
            case KernelOp::ScaleOffset:
                o += "%mul_" + suffix + " = OpFMul %f32 " + c0 + " " + p0 + "\n";
                o += "%res_" + suffix + " = OpFAdd %f32 %mul_" + suffix + " " + p1 + "\n";
                break;
            case KernelOp::Offset:
                o += "%res_" + suffix + " = OpFAdd %f32 " + c0 + " " + p0 + "\n";
                break;
            case KernelOp::Clip:
                o += "%res_" + suffix + " = OpExtInst %f32 %glsl FClamp "
                    + c0 + " " + p0 + " " + p1 + "\n";
                break;
            case KernelOp::Abs:
                o += "%res_" + suffix + " = OpExtInst %f32 %glsl FAbs " + c0 + "\n";
                break;
            case KernelOp::Negate:
                o += "%res_" + suffix + " = OpFNegate %f32 " + c0 + "\n";
                break;
            case KernelOp::Add:
                o += "%res_" + suffix + " = OpFAdd %f32 " + c0 + " " + c1 + "\n";
                break;
            case KernelOp::Multiply:
                o += "%res_" + suffix + " = OpFMul %f32 " + c0 + " " + c1 + "\n";
                break;
            case KernelOp::Mix:
                o += "%res_" + suffix + " = OpExtInst %f32 %glsl FMix "
                    + c0 + " " + c1 + " " + p0 + "\n";
                break;
            case KernelOp::Sub:
                o += "%res_" + suffix + " = OpFSub %f32 " + c0 + " " + c1 + "\n";
                break;
            default:
                o += "%res_" + suffix + " = OpCopyObject %f32 " + c0 + "\n";
                break;
            }
        };

        const std::string zero = "%img_czero";
        const std::string c1r = has_second ? "%ch1_r" : zero;
        const std::string c1g = has_second ? "%ch1_g" : zero;
        const std::string c1b = has_second ? "%ch1_b" : zero;
        const std::string c1a = has_second ? "%ch1_a" : zero;

        emit_channel_op("%ch0_r", c1r, "r");
        emit_channel_op("%ch0_g", c1g, "g");
        emit_channel_op("%ch0_b", c1b, "b");
        emit_channel_op("%ch0_a", c1a, "a");
        o += "\n";

        o += "%out_vec = OpCompositeConstruct %v4f32 %res_r %res_g %res_b %res_a\n";
        if (img_out)
            o += "OpImageWrite %img_" + std::string(img_out->name) + " %coord %out_vec\n";

        o += "OpReturn\n";
        o += "OpFunctionEnd\n";
        return o;
    }

    /**
     * Emit the entry point body for BitonicSort template.
     * Loads the index, loads PC fields, loads SSBO elements, applies op, stores.
     */
    std::string emit_bitonic_body(const ShaderSpec& spec)
    {
        const auto& bkeys = spec.bindings[0];
        const auto& bidx = spec.bindings[1];

        const std::string ktype(ssbo_elem_spirv_type(bkeys.format));
        const std::string itype(ssbo_elem_spirv_type(bidx.format));

        std::string o;
        o += "%main           = OpFunction %void None %voidfn\n";
        o += "%entry          = OpLabel\n";
        o += "%gid3           = OpLoad %v3u32 %gid_var\n";
        o += "%i              = OpCompositeExtract %u32 %gid3 0\n\n";

        o += "%ppc_stage      = OpAccessChain %ppc_u32 %pc %c0u\n";
        o += "%stage          = OpLoad %u32 %ppc_stage\n";
        o += "%ppc_pass       = OpAccessChain %ppc_u32 %pc %c1u\n";
        o += "%pass           = OpLoad %u32 %ppc_pass\n";
        o += "%ppc_count      = OpAccessChain %ppc_u32 %pc %c2u\n";
        o += "%count          = OpLoad %u32 %ppc_count\n";
        o += "%ppc_desc       = OpAccessChain %ppc_u32 %pc %c3u\n";
        o += "%descending     = OpLoad %u32 %ppc_desc\n\n";

        o += "%c1u_shift      = OpShiftLeftLogical %u32 %c1u %pass\n";
        o += "%partner        = OpBitwiseXor %u32 %i %c1u_shift\n\n";

        o += "%partner_le_i   = OpULessThanEqual %bool %partner %i\n";
        o += "OpSelectionMerge %early_merge None\n";
        o += "OpBranchConditional %partner_le_i %early_ret %bounds_chk\n\n";

        o += "%bounds_chk     = OpLabel\n";
        o += "%i_oob          = OpUGreaterThanEqual %bool %i %count\n";
        o += "%p_oob          = OpUGreaterThanEqual %bool %partner %count\n";
        o += "%oob            = OpLogicalOr %bool %i_oob %p_oob\n";
        o += "OpSelectionMerge %early_merge None\n";
        o += "OpBranchConditional %oob %early_ret %do_sort\n\n";

        o += "%do_sort        = OpLabel\n";

        o += "%gep_ki         = OpAccessChain %pelem_" + bkeys.name
            + " %buf_" + bkeys.name + " %c0u %i\n";
        o += "%key_i          = OpLoad " + ktype + " %gep_ki\n";
        o += "%gep_kp         = OpAccessChain %pelem_" + bkeys.name
            + " %buf_" + bkeys.name + " %c0u %partner\n";
        o += "%key_p          = OpLoad " + ktype + " %gep_kp\n\n";

        o += "%gep_ii         = OpAccessChain %pelem_" + bidx.name
            + " %buf_" + bidx.name + " %c0u %i\n";
        o += "%idx_i          = OpLoad " + itype + " %gep_ii\n";
        o += "%gep_ip         = OpAccessChain %pelem_" + bidx.name
            + " %buf_" + bidx.name + " %c0u %partner\n";
        o += "%idx_p          = OpLoad " + itype + " %gep_ip\n\n";

        o += "%dir_shift      = OpShiftRightLogical %u32 %i %stage\n";
        o += "%dir_bit        = OpBitwiseAnd %u32 %dir_shift %c1u\n\n";

        o += "%gt             = OpFOrdGreaterThan %bool %key_i %key_p\n";
        o += "%gt_u           = OpSelect %u32 %gt %c1u %c0u\n";
        o += "%xor1           = OpBitwiseXor %u32 %gt_u %dir_bit\n";
        o += "%xor2           = OpBitwiseXor %u32 %xor1 %descending\n";
        o += "%do_swap        = OpINotEqual %bool %xor2 %c0u\n\n";

        o += "%new_ki         = OpSelect " + ktype + " %do_swap %key_p %key_i\n";
        o += "%new_kp         = OpSelect " + ktype + " %do_swap %key_i %key_p\n";
        o += "%new_ii         = OpSelect " + itype + " %do_swap %idx_p %idx_i\n";
        o += "%new_ip         = OpSelect " + itype + " %do_swap %idx_i %idx_p\n\n";

        o += "OpStore %gep_ki %new_ki\n";
        o += "OpStore %gep_kp %new_kp\n";
        o += "OpStore %gep_ii %new_ii\n";
        o += "OpStore %gep_ip %new_ip\n";
        o += "OpBranch %early_merge\n\n";

        o += "%early_ret      = OpLabel\n";
        o += "OpBranch %early_merge\n\n";

        o += "%early_merge    = OpLabel\n";
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
    case KernelTemplate::BitonicSort:
        src += emit_bitonic_body(spec);
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
