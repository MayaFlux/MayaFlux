#include "ShaderSpec.hpp"

/**
 * @file AsmSkin.cpp
 * @brief SPIR-V assembly emitter of KernelTemplate::Skin.
 *
 * One thread deforms one vertex. It first adds to the rest record each morph target's difference by
 * that target's weight, in a loop over target_count. It then blends up to four joint matrices of a
 * palette by the vertex's weights, adding the identity for whatever weight is left over, so a vertex no joint moves stays
 * where it is. The blended matrix carries the position, its upper 3x3 carries the tangent, and the
 * cofactor of that 3x3 carries the normal, which is exact under non uniform scale and needs no
 * inverse. The tangent's handedness follows the sign of the 3x3's determinant.
 */

namespace MayaFlux::Portal::Graphics::detail {

namespace {

    using Id = std::string;

    struct Vec3 {
        Id x;
        Id y;
        Id z;
    };

    constexpr uint32_t k_rest_words = 10;
    constexpr uint32_t k_delta_words = 9;
    constexpr uint32_t k_normal_word = 10;
    constexpr uint32_t k_tangent_word = 13;

    class SkinBuilder {
    public:
        [[nodiscard]] std::string build(const ShaderSpec& spec)
        {
            const std::string& rest = spec.bindings[0].name;
            const std::string& joints = spec.bindings[1].name;
            const std::string& weights = spec.bindings[2].name;
            const std::string& palette = spec.bindings[3].name;
            const std::string& vertices = spec.bindings[4].name;
            const std::string& deltas = spec.bindings[5].name;
            const std::string& morph_weights = spec.bindings[6].name;

            m_text += "%main = OpFunction %void None %voidfn\n";
            m_text += "%entry = OpLabel\n";

            const Id v = ins("%u32", "OpCompositeExtract", { ins("%v3u32", "OpLoad", { "%gid_var" }), "0" });
            const Id first = push_constant(0);
            const Id count = push_constant(1);
            const Id stride = push_constant(2);
            const Id base = push_constant(3);
            const Id targets = push_constant(4);

            const Id inside = ins("%bool", "OpULessThan", { v, count });
            m_text += "OpSelectionMerge %sk_end None\n";
            m_text += "OpBranchConditional " + inside + " %sk_body %sk_end\n";
            m_text += "%sk_body = OpLabel\n";

            const Id rest_base = umul(v, u(k_rest_words));
            const Id influence_base = umul(v, u(4));
            std::array<Id, 10> r;
            for (uint32_t i = 0; i < k_rest_words; ++i) {
                r[i] = load_f32(rest, uadd(rest_base, u(i)));
            }

            const Id zero = u(0);
            const Id one = u(1);
            m_text += "OpBranch %sk_morph_pre\n";
            m_text += "%sk_morph_pre = OpLabel\n";
            m_text += "OpBranch %sk_morph_head\n";
            m_text += "%sk_morph_head = OpLabel\n";
            m_text += "%sk_t = OpPhi %u32 " + zero + " %sk_morph_pre %sk_t_next %sk_morph_next\n";
            std::array<Id, k_delta_words> morphed;
            for (uint32_t i = 0; i < k_delta_words; ++i) {
                morphed[i] = "%sk_acc" + std::to_string(i);
                m_text += morphed[i] + " = OpPhi %f32 " + r[i] + " %sk_morph_pre %sk_sum" + std::to_string(i) + " %sk_morph_next\n";
            }
            const Id more = ins("%bool", "OpULessThan", { "%sk_t", targets });
            m_text += "OpLoopMerge %sk_morph_done %sk_morph_next None\n";
            m_text += "OpBranchConditional " + more + " %sk_morph_body %sk_morph_done\n";
            m_text += "%sk_morph_body = OpLabel\n";
            const Id weight_t = load_f32(morph_weights, "%sk_t");
            const Id delta_base = umul(uadd(umul("%sk_t", count), v), u(k_delta_words));
            for (uint32_t i = 0; i < k_delta_words; ++i) {
                const Id scaled = fmul(weight_t, load_f32(deltas, uadd(delta_base, u(i))));
                m_text += "%sk_sum" + std::to_string(i) + " = OpFAdd %f32 " + morphed[i] + " " + scaled + "\n";
            }
            m_text += "OpBranch %sk_morph_next\n";
            m_text += "%sk_morph_next = OpLabel\n";
            m_text += "%sk_t_next = OpIAdd %u32 %sk_t " + one + "\n";
            m_text += "OpBranch %sk_morph_head\n";
            m_text += "%sk_morph_done = OpLabel\n";
            for (uint32_t i = 0; i < k_delta_words; ++i) {
                r[i] = morphed[i];
            }

            std::array<Id, 4> joint;
            std::array<Id, 4> weight;
            for (uint32_t c = 0; c < 4; ++c) {
                joint[c] = load_u32(joints, uadd(influence_base, u(c)));
                weight[c] = load_f32(weights, uadd(influence_base, u(c)));
            }

            const Id leftover = fsub(f(1.0F), fadd(fadd(weight[0], weight[1]), fadd(weight[2], weight[3])));

            constexpr std::array<uint32_t, 12> elements { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14 };
            std::array<Id, 16> m;
            std::array<Id, 4> matrix_base;
            for (uint32_t c = 0; c < 4; ++c) {
                matrix_base[c] = umul(uadd(base, joint[c]), u(16));
            }
            for (const uint32_t e : elements) {
                Id sum = fmul(weight[0], load_f32(palette, uadd(matrix_base[0], u(e))));
                for (uint32_t c = 1; c < 4; ++c) {
                    sum = fadd(sum, fmul(weight[c], load_f32(palette, uadd(matrix_base[c], u(e)))));
                }
                if (e == 0 || e == 5 || e == 10) {
                    sum = fadd(sum, leftover);
                }
                m[e] = sum;
            }

            const Vec3 a0 { m[0], m[1], m[2] };
            const Vec3 a1 { m[4], m[5], m[6] };
            const Vec3 a2 { m[8], m[9], m[10] };
            const Vec3 translation { m[12], m[13], m[14] };

            const Vec3 position = add3(add3(scale3(a0, r[0]), scale3(a1, r[1])), add3(scale3(a2, r[2]), translation));

            const Vec3 c0 = cross3(a1, a2);
            const Vec3 c1 = cross3(a2, a0);
            const Vec3 c2 = cross3(a0, a1);
            const Vec3 normal = normalise3(add3(add3(scale3(c0, r[3]), scale3(c1, r[4])), scale3(c2, r[5])));

            const Vec3 tangent = normalise3(add3(add3(scale3(a0, r[6]), scale3(a1, r[7])), scale3(a2, r[8])));
            const Id mirrored = ins("%bool", "OpFOrdLessThan", { dot3(a0, c0), f(0.0F) });
            const Id handedness = fmul(r[9], ins("%f32", "OpSelect", { mirrored, f(-1.0F), f(1.0F) }));

            const Id out = umul(uadd(first, v), stride);
            store_f32(vertices, uadd(out, u(0)), position.x);
            store_f32(vertices, uadd(out, u(1)), position.y);
            store_f32(vertices, uadd(out, u(2)), position.z);
            store_f32(vertices, uadd(out, u(k_normal_word)), normal.x);
            store_f32(vertices, uadd(out, u(k_normal_word + 1)), normal.y);
            store_f32(vertices, uadd(out, u(k_normal_word + 2)), normal.z);
            store_f32(vertices, uadd(out, u(k_tangent_word)), tangent.x);
            store_f32(vertices, uadd(out, u(k_tangent_word + 1)), tangent.y);
            store_f32(vertices, uadd(out, u(k_tangent_word + 2)), tangent.z);
            store_f32(vertices, uadd(out, u(k_tangent_word + 3)), handedness);

            m_text += "OpBranch %sk_end\n";
            m_text += "%sk_end = OpLabel\n";
            m_text += "OpReturn\n";
            m_text += "OpFunctionEnd\n";
            return declarations() + m_text;
        }

    private:
        std::string m_text;
        std::set<uint32_t> m_words;
        std::set<float> m_floats;
        uint32_t m_next { 0 };

        [[nodiscard]] static std::string float_name(float value)
        {
            return value == 0.0F ? "%sk_f0" : (value == 1.0F ? "%sk_f1" : "%sk_fn1");
        }

        [[nodiscard]] std::string declarations() const
        {
            std::string out;
            for (const uint32_t value : m_words) {
                out += "%sk_u" + std::to_string(value) + " = OpConstant %u32 " + std::to_string(value) + "\n";
            }
            for (const float value : m_floats) {
                out += float_name(value) + " = OpConstant %f32 " + (value == 0.0F ? "0.0" : (value == 1.0F ? "1.0" : "-1.0")) + "\n";
            }
            return out;
        }

        Id u(uint32_t value)
        {
            m_words.insert(value);
            return "%sk_u" + std::to_string(value);
        }

        Id f(float value)
        {
            m_floats.insert(value);
            return float_name(value);
        }

        Id ins(std::string_view type, std::string_view opcode, std::initializer_list<std::string_view> args)
        {
            Id id = "%sk" + std::to_string(m_next++);
            m_text += id + " = " + std::string(opcode) + " " + std::string(type);
            for (const auto& arg : args) {
                m_text += ' ';
                m_text += arg;
            }
            m_text += '\n';
            return id;
        }

        Id push_constant(uint32_t member)
        {
            return ins("%u32", "OpLoad", { ins("%ppc_u32", "OpAccessChain", { "%pc", u(member) }) });
        }

        Id load_f32(const std::string& binding, const Id& index)
        {
            return ins("%f32", "OpLoad", { ins("%pelem_" + binding, "OpAccessChain", { "%buf_" + binding, "%c0u", index }) });
        }

        Id load_u32(const std::string& binding, const Id& index)
        {
            return ins("%u32", "OpLoad", { ins("%pelem_" + binding, "OpAccessChain", { "%buf_" + binding, "%c0u", index }) });
        }

        void store_f32(const std::string& binding, const Id& index, const Id& value)
        {
            const Id pointer = ins("%pelem_" + binding, "OpAccessChain", { "%buf_" + binding, "%c0u", index });
            m_text += "OpStore " + pointer + " " + value + "\n";
        }

        Id uadd(const Id& a, const Id& b) { return ins("%u32", "OpIAdd", { a, b }); }
        Id umul(const Id& a, const Id& b) { return ins("%u32", "OpIMul", { a, b }); }
        Id fadd(const Id& a, const Id& b) { return ins("%f32", "OpFAdd", { a, b }); }
        Id fsub(const Id& a, const Id& b) { return ins("%f32", "OpFSub", { a, b }); }
        Id fmul(const Id& a, const Id& b) { return ins("%f32", "OpFMul", { a, b }); }

        Vec3 add3(const Vec3& a, const Vec3& b) { return { fadd(a.x, b.x), fadd(a.y, b.y), fadd(a.z, b.z) }; }
        Vec3 scale3(const Vec3& a, const Id& s) { return { fmul(a.x, s), fmul(a.y, s), fmul(a.z, s) }; }

        Vec3 cross3(const Vec3& a, const Vec3& b)
        {
            return {
                fsub(fmul(a.y, b.z), fmul(a.z, b.y)),
                fsub(fmul(a.z, b.x), fmul(a.x, b.z)),
                fsub(fmul(a.x, b.y), fmul(a.y, b.x)),
            };
        }

        Id dot3(const Vec3& a, const Vec3& b)
        {
            return fadd(fadd(fmul(a.x, b.x), fmul(a.y, b.y)), fmul(a.z, b.z));
        }

        Vec3 normalise3(const Vec3& a)
        {
            const Id length_squared = dot3(a, a);
            const Id positive = ins("%bool", "OpFOrdGreaterThan", { length_squared, f(0.0F) });
            const Id inverse = ins("%f32", "OpExtInst", { "%glsl", "InverseSqrt", length_squared });
            return scale3(a, ins("%f32", "OpSelect", { positive, inverse, f(1.0F) }));
        }
    };

} // namespace

std::string emit_skin(const ShaderSpec& spec)
{
    using Format = Kakshya::GpuDataFormat;
    constexpr std::array<Format, 7> formats {
        Format::FLOAT32, Format::UINT32, Format::FLOAT32, Format::FLOAT32, Format::FLOAT32, Format::FLOAT32, Format::FLOAT32
    };

    bool shaped = spec.bindings.size() == formats.size() && spec.pc_fields.size() == 5;
    for (size_t i = 0; shaped && i < formats.size(); ++i) {
        shaped = spec.bindings[i].format == formats[i];
    }
    for (size_t i = 0; shaped && i < spec.pc_fields.size(); ++i) {
        shaped = spec.pc_fields[i].format == Format::UINT32;
    }
    const uint32_t local = spec.workgroup_size[0];
    if (!shaped || local < 1 || local > 1024) {
        return {};
    }

    SkinBuilder builder;
    return builder.build(spec);
}

} // namespace MayaFlux::Portal::Graphics::detail
