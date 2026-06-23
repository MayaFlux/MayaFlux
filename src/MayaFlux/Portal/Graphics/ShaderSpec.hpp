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
 * The body expression from ShaderSpec is substituted into the selected template.
 */
enum class KernelTemplate : uint8_t {
    Elementwise, ///< f(x[i]) -> y[i]; one thread per element, index variable "i"
    Reduction, ///< f(x[0..n]) -> scalar; shared-memory tree reduction
    Stencil, ///< f(x[i-k..i+k]) -> y[i]; neighbourhood reads, radius supplied via PC
    GeometryEmit, ///< Writes into vertex SSBO with atomic counter; no output SSBO
};

/**
 * @struct BindingSlot
 * @brief Declaration of one SSBO or image binding in a generated shader.
 *
 * For SSBO bindings use any scalar or vector GpuDataFormat.
 * For image bindings set modality to TEXTURE_2D (emits sampler2D) or
 * IMAGE_2D (emits image2D storage); the generator maps modality to the
 * correct GLSL declaration.
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
 *
 * Valid formats: FLOAT32, INT32, UINT32, VEC2_F32, VEC3_F32, VEC4_F32.
 * GLSL type name and byte size are derived from format by the generator.
 */
struct PushConstantField {
    std::string name;
    Kakshya::GpuDataFormat format { Kakshya::GpuDataFormat::FLOAT32 };
};

/**
 * @struct ShaderSpec
 * @brief Complete declarative description of a generated compute shader.
 *
 * Pure value type. Carries no Vulkan state. Consumed by
 * ShaderFoundry::load_shader(const ShaderSpec&) which emits GLSL,
 * compiles via the existing shaderc path, and caches by content hash.
 *
 * Binding indices start at 1. Index 0 at set 0 is engine-reserved
 * (ViewTransform UBO) and is never emitted by the generator.
 *
 * The body_expr is substituted verbatim into the selected KernelTemplate.
 * Within it, binding names are the declared names (e.g. "sig[i] * pc.gain").
 * The loop index variable is always "i".
 *
 * Build via ShaderSpec::Assemble:
 * @code
 * ShaderID id = get_shader_foundry().load_shader(
 *     ShaderSpec::Assemble{}
 *         .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
 *         .pc("gain")
 *         .body("sig[i] = sig[i] * pc.gain;")
 *         .build()
 * );
 * @endcode
 */
struct ShaderSpec {
    KernelTemplate tmpl { KernelTemplate::Elementwise };
    std::vector<BindingSlot> bindings;
    std::vector<PushConstantField> pc_fields;
    std::string body_expr;
    std::array<uint32_t, 3> workgroup_size { 256, 1, 1 };
    uint32_t push_constant_bytes { 0 };

    /**
     * @class Assemble
     * @brief Fluent assembler producing a ShaderSpec.
     *
     * Accumulates binding, PC, and body declarations, then constructs
     * the ShaderSpec in build(). Nested to make the relationship to
     * ShaderSpec explicit at every call site.
     *
     * Binding indices are assigned in declaration order starting at 1.
     * Index 0 at set 0 is engine-reserved (ViewTransform UBO).
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
         * @brief Declare an SSBO binding.
         * @param name      Variable name in body_expr and generated GLSL.
         * @param direction Input, Output, or InOut.
         * @param format    Element type of the buffer array.
         * @param modality  Semantic meaning; defaults to SCALAR_F32.
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
         * @param name Variable name in body_expr.
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
         * @param name      Variable name in body_expr.
         * @param direction Input (readonly), Output (writeonly), or InOut (read_write).
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
         * @param name   Field name; accessible in body_expr as pc.<name>.
         * @param format GpuDataFormat (scalar or vector).
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

        /** @brief Set the GLSL body substituted into the kernel template. */
        Assemble& body(std::string expr)
        {
            m_body = std::move(expr);
            return *this;
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
                .bindings = std::move(m_bindings),
                .pc_fields = std::move(m_pc_fields),
                .body_expr = std::move(m_body),
                .workgroup_size = m_workgroup,
                .push_constant_bytes = pc_bytes,
            };
        }

    private:
        KernelTemplate m_tmpl { KernelTemplate::Elementwise };
        std::vector<BindingSlot> m_bindings;
        std::vector<PushConstantField> m_pc_fields;
        std::string m_body;
        std::array<uint32_t, 3> m_workgroup { 256, 1, 1 };
        uint32_t m_next_binding { 1 };
    };
};

} // namespace MayaFlux::Portal::Graphics
