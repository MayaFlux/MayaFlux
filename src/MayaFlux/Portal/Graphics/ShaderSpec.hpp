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
    BitonicSort, ///< Bitonic sort network; one thread per element
};

/**
 * @struct KernelSource
 * @brief Parsed representation of a user-supplied kernel lambda.
 *
 * Produced by the MF_KERNEL macro, which stringifies the lambda text
 * at the call site. The parameter names must match binding and push
 * constant field names declared on the enclosing ShaderSpec in
 * declaration order: SSBO bindings first, then PC fields, then
 * optionally a uint32_t index parameter named i.
 *
 * The body is extracted verbatim between the outermost braces and
 * injected into the generated GLSL compute shader by the emitter.
 *
 * @code
 * auto spec = ShaderSpec::Assemble{}
 *     .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
 *     .pc("gain")
 *     .kernel(MF_KERNEL([](float* sig, float gain, uint32_t i) {
 *         sig[i] = sig[i] * gain + sin(float(i) * 0.01f);
 *     }))
 *     .build();
 * @endcode
 */
struct KernelSource {
    std::string raw;
    std::vector<std::string> param_names;
    std::string body;

    /**
     * @brief Parse parameter names and body from a stringified lambda.
     *
     * Extracts identifiers from the parameter list by stripping type
     * tokens (everything up to and including the last space or * before
     * the identifier). Extracts body as the text between the first { and
     * the matching closing }.
     *
     * @param stringified Raw text produced by MF_KERNEL.
     * @return Populated KernelSource.
     */
    [[nodiscard]] static KernelSource parse(std::string_view stringified)
    {
        KernelSource ks;
        ks.raw = std::string(stringified);

        ///< Locate parameter list: between first '(' after '[' and its matching ')'
        const auto bracket = stringified.find('[');
        const auto paren_open = stringified.find('(', bracket);
        if (paren_open == std::string_view::npos)
            return ks;

        std::size_t depth = 1;
        std::size_t paren_close = paren_open + 1;
        while (paren_close < stringified.size() && depth > 0) {
            if (stringified[paren_close] == '(') {
                ++depth;
            } else if (stringified[paren_close] == ')') {
                --depth;
            }
            ++paren_close;
        }
        --paren_close; ///< points at the closing ')'

        const auto params_text = stringified.substr(paren_open + 1, paren_close - paren_open - 1);

        ///< Split on ',' and extract the last token (the identifier) from each param
        auto extract_name = [](std::string_view param) -> std::string {
            ///< strip trailing whitespace
            auto end = param.find_last_not_of(" \t\n\r");
            if (end == std::string_view::npos)
                return {};
            param = param.substr(0, end + 1);
            ///< find last space or * - identifier follows
            auto sep = param.find_last_of(" *\t");
            if (sep == std::string_view::npos)
                return std::string(param);
            return std::string(param.substr(sep + 1));
        };

        std::size_t start = 0;
        while (start < params_text.size()) {
            ///< find next comma not inside <> or ()
            int angle = 0, paren = 0;
            std::size_t pos = start;
            while (pos < params_text.size()) {
                char c = params_text[pos];
                if (c == '<') {
                    ++angle;
                } else if (c == '>') {
                    --angle;
                } else if (c == '(') {
                    ++paren;
                } else if (c == ')') {
                    --paren;
                } else if (c == ',' && angle == 0 && paren == 0) {
                    break;
                }
                ++pos;
            }
            auto name = extract_name(params_text.substr(start, pos - start));
            if (!name.empty())
                ks.param_names.push_back(std::move(name));
            start = (pos < params_text.size()) ? pos + 1 : pos;
        }

        ///< Locate body: between first '{' after ')' and its matching '}'
        const auto brace_open = stringified.find('{', paren_close);
        if (brace_open == std::string_view::npos)
            return ks;

        depth = 1;
        std::size_t brace_close = brace_open + 1;
        while (brace_close < stringified.size() && depth > 0) {
            if (stringified[brace_close] == '{') {
                ++depth;
            } else if (stringified[brace_close] == '}') {
                --depth;
            }
            ++brace_close;
        }
        --brace_close;

        ks.body = std::string(stringified.substr(brace_open + 1, brace_close - brace_open - 1));
        return ks;
    }
};

/**
 * @enum KernelOp
 * @brief Named operation the SPIR-V emitter knows how to lower to opcodes.
 *
 * Each value maps to a fixed, unambiguous instruction sequence. No string
 * parsing or expression compilation is involved.
 *
 * Naming convention: operands are listed in application order.
 * PC fields are consumed in declaration order from ShaderSpec::pc_fields.
 * SSBO operands are the InOut/Output bindings in declaration order.
 *
 * Single-SSBO elementwise — arithmetic (sig[i] = f(sig[i], pc...)):
 *   Scale        sig[i] = sig[i] * pc[0]
 *   ScaleOffset  sig[i] = sig[i] * pc[0] + pc[1]
 *   Fma          sig[i] = sig[i] * pc[0] + pc[1]   (fused, single instruction)
 *   Offset       sig[i] = sig[i] + pc[0]
 *   Clip         sig[i] = clamp(sig[i], pc[0], pc[1])
 *   Abs          sig[i] = abs(sig[i])
 *   Negate       sig[i] = -sig[i]
 *   Floor        sig[i] = floor(sig[i])
 *   Ceil         sig[i] = ceil(sig[i])
 *   Round        sig[i] = round(sig[i])
 *   Trunc        sig[i] = trunc(sig[i])
 *   Fract        sig[i] = fract(sig[i])
 *   Sqrt         sig[i] = sqrt(sig[i])
 *   InverseSqrt  sig[i] = 1/sqrt(sig[i])
 *
 * Single-SSBO elementwise — transcendental (sig[i] = f(sig[i])):
 *   Sin          sig[i] = sin(sig[i])
 *   Cos          sig[i] = cos(sig[i])
 *   Tan          sig[i] = tan(sig[i])
 *   Asin         sig[i] = asin(sig[i])
 *   Acos         sig[i] = acos(sig[i])
 *   Atan         sig[i] = atan(sig[i])
 *   Sinh         sig[i] = sinh(sig[i])
 *   Cosh         sig[i] = cosh(sig[i])
 *   Tanh         sig[i] = tanh(sig[i])
 *   Exp          sig[i] = e^sig[i]
 *   Exp2         sig[i] = 2^sig[i]
 *   Log          sig[i] = ln(sig[i])
 *   Log2         sig[i] = log2(sig[i])
 *
 * Two-SSBO elementwise (out[i] = f(a[i], b[i])):
 *   Add          out[i] = a[i] + b[i]
 *   Multiply     out[i] = a[i] * b[i]
 *   Mix          out[i] = a[i] + (b[i] - a[i]) * pc[0]
 *   Sub          out[i] = a[i] - b[i]
 *   Pow          out[i] = pow(a[i], b[i])
 *   Atan2        out[i] = atan(a[i], b[i])
 *   Min          out[i] = min(a[i], b[i])
 *   MaxTwo       out[i] = max(a[i], b[i])
 *   Step         out[i] = step(a[i], b[i])   edge=a, x=b
 *
 * Two-SSBO + one PC:
 *   SmoothStep   out[i] = smoothstep(a[i], b[i], pc[0])
 *
 * Reduction operations (one InOut SSBO, shared memory):
 *   Sum          accumulate + into shared[lid], tree reduce
 *   Max          accumulate max into shared[lid], tree reduce
 */
enum class KernelOp : uint8_t {
    // arithmetic
    Scale,
    ScaleOffset,
    Fma,
    Offset,
    Clip,
    Abs,
    Negate,
    Floor,
    Ceil,
    Round,
    Trunc,
    Fract,
    Sqrt,
    InverseSqrt,
    // transcendental
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Sinh,
    Cosh,
    Tanh,
    Exp,
    Exp2,
    Log,
    Log2,
    // two-SSBO
    Add,
    Multiply,
    Mix,
    Sub,
    Pow,
    Atan2,
    Min,
    MaxTwo,
    Step,
    SmoothStep,
    // reduction
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
    std::optional<KernelSource> kernel; ///< When set, KernelOp is ignored.

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

        /**
         * @brief Supply a user kernel via MF_KERNEL. When set, KernelOp is ignored.
         * @param ks KernelSource produced by MF_KERNEL.
         */
        Assemble& kernel(KernelSource ks)
        {
            m_kernel = std::move(ks);
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
                .kernel = std::move(m_kernel),
            };
        }

    private:
        KernelTemplate m_tmpl { KernelTemplate::Elementwise };
        KernelOp m_op { KernelOp::Scale };
        std::vector<BindingSlot> m_bindings;
        std::vector<PushConstantField> m_pc_fields;
        std::array<uint32_t, 3> m_workgroup { 256, 1, 1 };
        uint32_t m_next_binding { 1 };
        std::optional<KernelSource> m_kernel;
    };
};

/**
 * @brief Stringifies a kernel lambda for use with ShaderSpec::Assemble::kernel().
 *
 * The lambda is never evaluated. Its text is captured at the call site and
 * parsed into parameter names and a body by KernelSource::parse(). Parameter
 * names must match the binding and push constant field names declared on the
 * enclosing ShaderSpec in declaration order.
 *
 * @code
 * .kernel(MF_KERNEL([](float* sig, float gain, uint32_t i) {
 *     sig[i] *= gain;
 * }))
 * @endcode
 */
#define MF_KERNEL(lambda) \
    MayaFlux::Portal::Graphics::KernelSource::parse(#lambda)

} // namespace MayaFlux::Portal::Graphics
