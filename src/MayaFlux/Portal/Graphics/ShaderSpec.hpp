#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Portal::Graphics {

/**
 * @enum BindingDirection
 * @brief Data flow direction for a shader binding slot.
 */
enum class BindingDirection : uint8_t {
    Input,
    Output,
    InOut,
};

/**
 * @enum KernelTemplate
 * @brief Structural shape of the generated compute kernel.
 *
 * Determines thread dispatch math, binding structure, and loop shape.
 */
enum class KernelTemplate : uint8_t {
    Elementwise, ///< f(x[i]) -> y[i]; one thread per element
    Reduction, ///< f(x[0..n]) -> scalar; shared-memory tree reduction
    Stencil, ///< f(x[i-k..i+k]) -> y[i]; neighbourhood reads, radius in PC
    GeometryEmit, ///< Writes into vertex SSBO with atomic counter
};

/**
 * @enum KernelOp
 * @brief Named operation the SPIR-V emitter knows how to lower to opcodes.
 *
 * Each value maps to a fixed, unambiguous instruction sequence. The emitter
 * in VKShaderModule::emit_spirv_asm() handles each case explicitly.
 * No string parsing or expression compilation is involved.
 *
 * Naming convention: operands are listed in application order.
 * PC fields are consumed in declaration order from ShaderSpec::pc_fields.
 * SSBO operands are the InOut/Output bindings in declaration order.
 *
 * Single-SSBO elementwise operations (sig[i] = f(sig[i], pc...)):
 *   Scale        sig[i] = sig[i] * pc[0]
 *   ScaleOffset  sig[i] = sig[i] * pc[0] + pc[1]
 *   Offset       sig[i] = sig[i] + pc[0]
 *   Clip         sig[i] = clamp(sig[i], pc[0], pc[1])
 *   Abs          sig[i] = abs(sig[i])
 *   Negate       sig[i] = -sig[i]
 *
 * Two-SSBO elementwise operations (out[i] = f(a[i], b[i])):
 *   Add          out[i] = a[i] + b[i]
 *   Multiply     out[i] = a[i] * b[i]
 *   Mix          out[i] = a[i] + (b[i] - a[i]) * pc[0]
 *
 * Reduction operations (one InOut SSBO, shared memory):
 *   Sum          accumulate + into shared[lid], tree reduce
 *   Max          accumulate max into shared[lid], tree reduce
 */
enum class KernelOp : uint8_t {
    Scale,
    ScaleOffset,
    Offset,
    Clip,
    Abs,
    Negate,
    Add,
    Multiply,
    Mix,
    Sum,
    Max,
};

/**
 * @struct BindingSlot
 * @brief Declaration of one SSBO or image binding in a generated shader.
 */
struct BindingSlot {
    std::string name;
    BindingDirection direction;
    Kakshya::GpuDataFormat format;
    Kakshya::DataModality modality { Kakshya::DataModality::SCALAR_F32 };
    uint32_t binding_index { 0 };
};

/**
 * @struct PushConstantField
 * @brief One field in the generated push constant block.
 */
struct PushConstantField {
    std::string name;
    Kakshya::GpuDataFormat format { Kakshya::GpuDataFormat::FLOAT32 };
};

/**
 * @struct ShaderSpec
 * @brief Complete declarative description of a generated compute shader.
 *
 * Pure value type. Consumed by ShaderFoundry::load_shader(const ShaderSpec&)
 * which emits SPIR-V assembly via VKShaderModule::emit_spirv_asm() and
 * assembles it via VKShaderModule::create_from_spirv_asm(). No shaderc
 * or GLSL toolchain is involved.
 *
 * Binding indices start at 1. Index 0 at set 0 is engine-reserved.
 *
 * The op field selects a named operation the emitter knows how to lower
 * to SPIR-V opcodes deterministically. PC fields are consumed in
 * declaration order as operands.
 *
 * @code
 * auto spec = ShaderSpec::Assemble{}
 *     .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
 *     .pc("gain")
 *     .pc("offset")
 *     .op(KernelOp::ScaleOffset)
 *     .build();
 * @endcode
 */
struct ShaderSpec {
    KernelTemplate tmpl { KernelTemplate::Elementwise };
    KernelOp op { KernelOp::Scale };
    std::vector<BindingSlot> bindings;
    std::vector<PushConstantField> pc_fields;
    std::array<uint32_t, 3> workgroup_size { 256, 1, 1 };
    uint32_t push_constant_bytes { 0 };

    /**
     * @class Assemble
     * @brief Fluent assembler producing a ShaderSpec.
     *
     * Binding indices are assigned in declaration order starting at 1.
     */
    class Assemble {
    public:
        Assemble() = default;

        /** @brief Set the kernel template. Defaults to Elementwise. */
        Assemble& tmpl(KernelTemplate t)
        {
            m_tmpl = t;
            return *this;
        }

        /**
         * @brief Set the named operation the emitter will lower to SPIR-V.
         * @param o KernelOp value.
         */
        Assemble& op(KernelOp o)
        {
            m_op = o;
            return *this;
        }

        /**
         * @brief Declare an SSBO binding.
         */
        Assemble& ssbo(
            std::string name,
            BindingDirection direction,
            Kakshya::GpuDataFormat format,
            Kakshya::DataModality modality = Kakshya::DataModality::SCALAR_F32)
        {
            m_bindings.push_back({
                .name = std::move(name),
                .direction = direction,
                .format = format,
                .modality = modality,
                .binding_index = m_next_binding++,
            });
            return *this;
        }

        /**
         * @brief Declare a sampled image binding (sampler2D).
         */
        Assemble& texture(std::string name)
        {
            m_bindings.push_back({
                .name = std::move(name),
                .direction = BindingDirection::Input,
                .format = Kakshya::GpuDataFormat::VEC4_F32,
                .modality = Kakshya::DataModality::TEXTURE_2D,
                .binding_index = m_next_binding++,
            });
            return *this;
        }

        /**
         * @brief Declare a storage image binding (image2D).
         */
        Assemble& storage_image(
            std::string name,
            BindingDirection direction = BindingDirection::Output)
        {
            m_bindings.push_back({
                .name = std::move(name),
                .direction = direction,
                .format = Kakshya::GpuDataFormat::VEC4_F32,
                .modality = Kakshya::DataModality::IMAGE_2D,
                .binding_index = m_next_binding++,
            });
            return *this;
        }

        /**
         * @brief Declare a push constant field with explicit format.
         */
        Assemble& pc(std::string name, Kakshya::GpuDataFormat format)
        {
            m_pc_fields.push_back({ .name = std::move(name), .format = format });
            return *this;
        }

        /** @brief Declare a float push constant field. */
        Assemble& pc(std::string name)
        {
            return pc(std::move(name), Kakshya::GpuDataFormat::FLOAT32);
        }

        /** @brief Override workgroup size. Defaults to {256, 1, 1}. */
        Assemble& workgroup(uint32_t x, uint32_t y = 1, uint32_t z = 1)
        {
            m_workgroup = { x, y, z };
            return *this;
        }

        /** @brief Finalise and return the ShaderSpec. */
        [[nodiscard]] ShaderSpec build()
        {
            uint32_t pc_bytes = 0;
            for (const auto& f : m_pc_fields)
                pc_bytes += Kakshya::gpu_data_format_bytes(f.format);

            return ShaderSpec {
                .tmpl = m_tmpl,
                .op = m_op,
                .bindings = std::move(m_bindings),
                .pc_fields = std::move(m_pc_fields),
                .workgroup_size = m_workgroup,
                .push_constant_bytes = pc_bytes,
            };
        }

    private:
        KernelTemplate m_tmpl { KernelTemplate::Elementwise };
        KernelOp m_op { KernelOp::Scale };
        std::vector<BindingSlot> m_bindings;
        std::vector<PushConstantField> m_pc_fields;
        std::array<uint32_t, 3> m_workgroup { 256, 1, 1 };
        uint32_t m_next_binding { 1 };
    };
};

} // namespace MayaFlux::Portal::Graphics
