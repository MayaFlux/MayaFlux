#include "ShaderSpec.hpp"

namespace MayaFlux::Portal::Graphics::detail {

namespace {

    using Id = std::string;

    struct Word64 {
        Word64(Id low, Id high)
            : lo(std::move(low))
            , hi(std::move(high))
        {
        }

        Id lo;
        Id hi;
    };

    struct Shifted {
        Shifted(Word64 shifted, Id lost)
            : value(std::move(shifted))
            , sticky(std::move(lost))
        {
        }

        Word64 value;
        Id sticky;
    };

    class Builder {
    public:
        [[nodiscard]] SoftDoubleModule finish(const SoftDoubleUse& use)
        {
            if (use.add) {
                emit_add();
            }
            if (use.mul) {
                emit_mul();
            }
            if (use.less) {
                emit_less();
            }

            SoftDoubleModule out;
            out.declarations += "%sd_pair = OpTypeStruct %u32 %u32\n";
            out.declarations += "%sd_fn_bin = OpTypeFunction %v2u32 %v2u32 %v2u32\n";
            out.declarations += "%sd_fn_less = OpTypeFunction %bool %v2u32 %v2u32\n";
            for (const uint32_t value : m_constants) {
                out.declarations += "%sd_k" + std::to_string(value) + " = OpConstant %u32 " + std::to_string(value) + "\n";
            }
            out.functions = std::move(m_text);
            return out;
        }

    private:
        std::string m_text;
        std::set<uint32_t> m_constants;
        uint32_t m_next { 0 };

        Id k(uint32_t value)
        {
            m_constants.insert(value);
            return "%sd_k" + std::to_string(value);
        }

        Id ins(std::string_view type, std::string_view opcode, std::initializer_list<std::string_view> args)
        {
            Id id = "%sd" + std::to_string(m_next++);
            m_text += id + " = " + std::string(opcode) + " " + std::string(type);
            for (const auto& arg : args) {
                m_text += ' ';
                m_text += arg;
            }
            m_text += '\n';
            return id;
        }

        Id word(std::string_view opcode, const Id& a, const Id& b) { return ins("%u32", opcode, { a, b }); }
        Id test(std::string_view opcode, const Id& a, const Id& b) { return ins("%bool", opcode, { a, b }); }

        Id add(const Id& a, const Id& b) { return word("OpIAdd", a, b); }
        Id sub(const Id& a, const Id& b) { return word("OpISub", a, b); }
        Id band(const Id& a, const Id& b) { return word("OpBitwiseAnd", a, b); }
        Id bor(const Id& a, const Id& b) { return word("OpBitwiseOr", a, b); }
        Id bxor(const Id& a, const Id& b) { return word("OpBitwiseXor", a, b); }
        Id bnot(const Id& a) { return ins("%u32", "OpNot", { a }); }
        Id shl(const Id& a, const Id& n) { return word("OpShiftLeftLogical", a, n); }
        Id shr(const Id& a, const Id& n) { return word("OpShiftRightLogical", a, n); }
        Id umin(const Id& a, const Id& b) { return ins("%u32", "OpExtInst", { "%glsl", "UMin", a, b }); }
        Id umax(const Id& a, const Id& b) { return ins("%u32", "OpExtInst", { "%glsl", "UMax", a, b }); }
        Id msb(const Id& a) { return ins("%u32", "OpExtInst", { "%glsl", "FindUMsb", a }); }

        Id eq(const Id& a, const Id& b) { return test("OpIEqual", a, b); }
        Id ne(const Id& a, const Id& b) { return test("OpINotEqual", a, b); }
        Id ult(const Id& a, const Id& b) { return test("OpULessThan", a, b); }
        Id uge(const Id& a, const Id& b) { return test("OpUGreaterThanEqual", a, b); }
        Id sle(const Id& a, const Id& b) { return test("OpSLessThanEqual", a, b); }
        Id sge(const Id& a, const Id& b) { return test("OpSGreaterThanEqual", a, b); }
        Id land(const Id& a, const Id& b) { return test("OpLogicalAnd", a, b); }
        Id lor(const Id& a, const Id& b) { return test("OpLogicalOr", a, b); }

        Id sel(const Id& c, const Id& a, const Id& b) { return ins("%u32", "OpSelect", { c, a, b }); }
        Id bsel(const Id& c, const Id& a, const Id& b) { return ins("%bool", "OpSelect", { c, a, b }); }

        Id pair(const Id& lo, const Id& hi) { return ins("%v2u32", "OpCompositeConstruct", { lo, hi }); }

        Word64 unpack(const Id& v)
        {
            return { ins("%u32", "OpCompositeExtract", { v, "0" }), ins("%u32", "OpCompositeExtract", { v, "1" }) };
        }

        Word64 select64(const Id& c, const Word64& a, const Word64& b)
        {
            return { sel(c, a.lo, b.lo), sel(c, a.hi, b.hi) };
        }

        Id shl_safe(const Id& a, const Id& n)
        {
            return sel(ult(n, k(32)), shl(a, umin(n, k(31))), k(0));
        }

        Id shr_safe(const Id& a, const Id& n)
        {
            return sel(ult(n, k(32)), shr(a, umin(n, k(31))), k(0));
        }

        Word64 shl64(const Word64& w, const Id& n)
        {
            const Id narrow_hi = bor(shl_safe(w.hi, n), shr_safe(w.lo, sub(k(32), n)));
            const Id narrow_lo = shl_safe(w.lo, n);
            const Id wide_hi = shl_safe(w.lo, sub(n, k(32)));
            const Id wide = uge(n, k(32));
            return { sel(wide, k(0), narrow_lo), sel(wide, wide_hi, narrow_hi) };
        }

        Word64 shr64(const Word64& w, const Id& n)
        {
            const Id narrow_lo = bor(shr_safe(w.lo, n), shl_safe(w.hi, sub(k(32), n)));
            const Id narrow_hi = shr_safe(w.hi, n);
            const Id wide_lo = shr_safe(w.hi, sub(n, k(32)));
            const Id wide = uge(n, k(32));
            return { sel(wide, wide_lo, narrow_lo), sel(wide, k(0), narrow_hi) };
        }

        Shifted shr64_sticky(const Word64& w, const Id& n)
        {
            Word64 value = shr64(w, n);
            const Word64 lost = shl64(w, sub(k(64), umin(n, k(64))));
            return { std::move(value), lor(ne(lost.lo, k(0)), ne(lost.hi, k(0))) };
        }

        Word64 add64(const Word64& a, const Word64& b)
        {
            const Id lo = add(a.lo, b.lo);
            const Id carry = sel(ult(lo, a.lo), k(1), k(0));
            return { lo, add(add(a.hi, b.hi), carry) };
        }

        Word64 sub64(const Word64& a, const Word64& b)
        {
            const Id lo = sub(a.lo, b.lo);
            const Id borrow = sel(ult(a.lo, b.lo), k(1), k(0));
            return { lo, sub(sub(a.hi, b.hi), borrow) };
        }

        Id ult64(const Word64& a, const Word64& b)
        {
            return lor(ult(a.hi, b.hi), land(eq(a.hi, b.hi), ult(a.lo, b.lo)));
        }

        Id msb64(const Word64& w)
        {
            return sel(ne(w.hi, k(0)), add(k(32), msb(w.hi)), msb(w.lo));
        }

        Word64 mul32(const Id& a, const Id& b)
        {
            const Id product = ins("%sd_pair", "OpUMulExtended", { a, b });
            return { ins("%u32", "OpCompositeExtract", { product, "0" }), ins("%u32", "OpCompositeExtract", { product, "1" }) };
        }

        void begin(std::string_view name, std::string_view result, std::string_view function_type)
        {
            const std::string base = "%sd_f64_" + std::string(name);
            m_text += base + " = OpFunction " + std::string(result) + " None " + std::string(function_type) + "\n";
            m_text += base + "_a = OpFunctionParameter %v2u32\n";
            m_text += base + "_b = OpFunctionParameter %v2u32\n";
            m_text += base + "_entry = OpLabel\n";
        }

        void end(const Id& value)
        {
            m_text += "OpReturnValue " + value + "\n";
            m_text += "OpFunctionEnd\n\n";
        }

        Id round_pack(const Id& sign, const Id& e, const Word64& m, const Id& sticky)
        {
            const Id subnormal = sle(e, k(0));
            const Shifted down = shr64_sticky(m, sub(k(1), e));
            const Word64 m1 = select64(subnormal, down.value, m);
            const Id st1 = bsel(subnormal, lor(sticky, down.sticky), sticky);
            const Id e1 = sel(subnormal, k(1), e);

            const Id half = ne(band(m1.lo, k(512)), k(0));
            const Id below = lor(ne(band(m1.lo, k(511)), k(0)), st1);
            const Id odd = ne(band(m1.lo, k(1024)), k(0));
            const Id up = land(half, lor(below, odd));
            const Word64 m2 = add64(m1, Word64 { sel(up, k(1024), k(0)), k(0) });

            const Id mant_lo = bor(shr(m2.lo, k(10)), shl(m2.hi, k(22)));
            const Id mant_hi = shr(m2.hi, k(10));
            const Id grew = ne(band(mant_hi, k(2097152)), k(0));
            const Id lo2 = sel(grew, bor(shr(mant_lo, k(1)), shl(mant_hi, k(31))), mant_lo);
            const Id hi2 = sel(grew, shr(mant_hi, k(1)), mant_hi);
            const Id e2 = sel(grew, add(e1, k(1)), e1);

            const Id hidden = ne(band(hi2, k(1048576)), k(0));
            const Id field = sel(hidden, e2, k(0));
            const Id overflow = sge(e2, k(2047));
            const Id hi_norm = bor(bor(sign, shl(field, k(20))), band(hi2, k(1048575)));
            const Id hi_out = sel(overflow, bor(sign, k(2146435072)), hi_norm);
            const Id lo_out = sel(overflow, k(0), lo2);
            return pair(lo_out, hi_out);
        }

        void emit_add()
        {
            begin("add", "%v2u32", "%sd_fn_bin");
            const Word64 wa = unpack("%sd_f64_add_a");
            const Word64 wb = unpack("%sd_f64_add_b");

            const Id ea_raw = band(shr(wa.hi, k(20)), k(2047));
            const Id eb_raw = band(shr(wb.hi, k(20)), k(2047));
            const Id fa = bor(band(wa.hi, k(1048575)), wa.lo);
            const Id fb = bor(band(wb.hi, k(1048575)), wb.lo);
            const Id a_max = eq(ea_raw, k(2047));
            const Id b_max = eq(eb_raw, k(2047));
            const Id a_nan = land(a_max, ne(fa, k(0)));
            const Id b_nan = land(b_max, ne(fb, k(0)));
            const Id a_inf = land(a_max, eq(fa, k(0)));
            const Id b_inf = land(b_max, eq(fb, k(0)));
            const Id sa = band(wa.hi, k(2147483648));
            const Id sb = band(wb.hi, k(2147483648));
            const Id invalid = lor(lor(a_nan, b_nan), land(land(a_inf, b_inf), ne(sa, sb)));
            const Id special = lor(a_max, b_max);
            const Id inf_lo = sel(a_inf, wa.lo, wb.lo);
            const Id inf_hi = sel(a_inf, wa.hi, wb.hi);
            const Id special_lo = sel(invalid, k(0), inf_lo);
            const Id special_hi = sel(invalid, k(2146959360), inf_hi);

            const Word64 mag_a { wa.lo, band(wa.hi, k(2147483647)) };
            const Word64 mag_b { wb.lo, band(wb.hi, k(2147483647)) };
            const Id a_smaller = ult64(mag_a, mag_b);
            const Word64 x { sel(a_smaller, wb.lo, wa.lo), sel(a_smaller, wb.hi, wa.hi) };
            const Word64 y { sel(a_smaller, wa.lo, wb.lo), sel(a_smaller, wa.hi, wb.hi) };
            const Id sx = band(x.hi, k(2147483648));
            const Id sy = band(y.hi, k(2147483648));
            const Id ex_raw = band(shr(x.hi, k(20)), k(2047));
            const Id ey_raw = band(shr(y.hi, k(20)), k(2047));
            const Id ex = umax(ex_raw, k(1));
            const Id ey = umax(ey_raw, k(1));
            const Word64 mx_raw { x.lo, bor(band(x.hi, k(1048575)), sel(ne(ex_raw, k(0)), k(1048576), k(0))) };
            const Word64 my_raw { y.lo, bor(band(y.hi, k(1048575)), sel(ne(ey_raw, k(0)), k(1048576), k(0))) };
            const Word64 mx = shl64(mx_raw, k(10));
            const Word64 my_full = shl64(my_raw, k(10));
            const Shifted aligned = shr64_sticky(my_full, sub(ex, ey));

            const Id same = eq(sx, sy);
            const Word64 sum = add64(mx, aligned.value);
            const Word64 diff = sub64(sub64(mx, aligned.value), Word64 { sel(aligned.sticky, k(1), k(0)), k(0) });
            const Word64 m = select64(same, sum, diff);
            const Id is_zero = land(eq(m.lo, k(0)), eq(m.hi, k(0)));
            const Id zero_sign = sel(same, sx, k(0));

            const Id p = msb64(m);
            const Id carried = eq(p, k(63));
            const Shifted halved = shr64_sticky(m, k(1));
            const Id left = sel(ult(p, k(62)), umin(sub(k(62), p), sub(ex, k(1))), k(0));
            const Word64 lifted = shl64(m, left);
            const Word64 norm = select64(carried, halved.value, lifted);
            const Id e = sel(carried, add(ex, k(1)), sub(ex, left));
            const Id sticky = bsel(carried, lor(aligned.sticky, halved.sticky), aligned.sticky);
            const Word64 packed = unpack(round_pack(sx, e, norm, sticky));

            const Id finite_lo = sel(is_zero, k(0), packed.lo);
            const Id finite_hi = sel(is_zero, zero_sign, packed.hi);
            end(pair(sel(special, special_lo, finite_lo), sel(special, special_hi, finite_hi)));
        }

        struct Unpacked {
            Word64 mant;
            Id exp;
            Id is_zero;
            Id is_inf;
            Id is_nan;
        };

        Unpacked unpack_normalised(const Word64& w)
        {
            const Id e_raw = band(shr(w.hi, k(20)), k(2047));
            const Id frac = bor(band(w.hi, k(1048575)), w.lo);
            const Id max_exp = eq(e_raw, k(2047));
            const Word64 mant { w.lo, bor(band(w.hi, k(1048575)), sel(ne(e_raw, k(0)), k(1048576), k(0))) };
            const Id lead = sub(k(52), msb64(mant));
            Word64 normalised = shl64(mant, lead);
            Id exponent = sub(umax(e_raw, k(1)), lead);
            Id zero = land(eq(e_raw, k(0)), eq(frac, k(0)));
            Id inf = land(max_exp, eq(frac, k(0)));
            Id nan = land(max_exp, ne(frac, k(0)));
            return Unpacked {
                .mant = std::move(normalised),
                .exp = std::move(exponent),
                .is_zero = std::move(zero),
                .is_inf = std::move(inf),
                .is_nan = std::move(nan)
            };
        }

        void emit_mul()
        {
            begin("mul", "%v2u32", "%sd_fn_bin");
            const Word64 wa = unpack("%sd_f64_mul_a");
            const Word64 wb = unpack("%sd_f64_mul_b");
            const Unpacked a = unpack_normalised(wa);
            const Unpacked b = unpack_normalised(wb);

            const Id sign = band(bxor(wa.hi, wb.hi), k(2147483648));
            const Id invalid = lor(lor(a.is_nan, b.is_nan), lor(land(a.is_inf, b.is_zero), land(b.is_inf, a.is_zero)));
            const Id inf = lor(a.is_inf, b.is_inf);
            const Id zero = lor(a.is_zero, b.is_zero);

            const Word64 p0 = mul32(a.mant.lo, b.mant.lo);
            const Word64 p1 = mul32(a.mant.hi, b.mant.lo);
            const Word64 p2 = mul32(a.mant.lo, b.mant.hi);
            const Word64 p3 = mul32(a.mant.hi, b.mant.hi);
            const Word64 cross = add64(p1, p2);
            const Word64 middle = add64(Word64 { p0.hi, k(0) }, Word64 { cross.lo, k(0) });
            const Word64 upper = add64(add64(p3, Word64 { cross.hi, k(0) }), Word64 { middle.hi, k(0) });
            const Id r0 = p0.lo;
            const Id r1 = middle.lo;
            const Id r2 = upper.lo;
            const Id r3 = upper.hi;

            const Id big = ne(band(shr(r3, k(9)), k(1)), k(0));
            const Word64 m42 { bor(shr(r1, k(10)), shl(r2, k(22))), bor(shr(r2, k(10)), shl(r3, k(22))) };
            const Word64 m43 { bor(shr(r1, k(11)), shl(r2, k(21))), bor(shr(r2, k(11)), shl(r3, k(21))) };
            const Id st42 = lor(ne(r0, k(0)), ne(band(r1, k(1023)), k(0)));
            const Id st43 = lor(ne(r0, k(0)), ne(band(r1, k(2047)), k(0)));
            const Word64 m = select64(big, m43, m42);
            const Id sticky = bsel(big, st43, st42);
            const Id e = add(sub(add(a.exp, b.exp), k(1023)), sel(big, k(1), k(0)));
            const Word64 packed = unpack(round_pack(sign, e, m, sticky));

            const Id inf_hi = bor(sign, k(2146435072));
            const Id lo = sel(lor(invalid, lor(inf, zero)), k(0), packed.lo);
            const Id hi = sel(invalid, k(2146959360), sel(inf, inf_hi, sel(zero, sign, packed.hi)));
            end(pair(lo, hi));
        }

        Word64 order_key(const Word64& w)
        {
            const Id max_exp = eq(band(shr(w.hi, k(20)), k(2047)), k(2047));
            const Id is_nan = land(max_exp, ne(bor(band(w.hi, k(1048575)), w.lo), k(0)));
            const Id is_zero = eq(bor(shl(w.hi, k(1)), w.lo), k(0));
            const Id hi = sel(is_zero, k(0), w.hi);
            const Id lo = sel(is_zero, k(0), w.lo);
            const Id negative = ne(band(hi, k(2147483648)), k(0));
            const Id key_lo = sel(negative, bnot(lo), lo);
            const Id key_hi = sel(negative, bnot(hi), bor(hi, k(2147483648)));
            return { sel(is_nan, k(4294967295), key_lo), sel(is_nan, k(4294967295), key_hi) };
        }

        void emit_less()
        {
            begin("less", "%bool", "%sd_fn_less");
            const Word64 ka = order_key(unpack("%sd_f64_less_a"));
            const Word64 kb = order_key(unpack("%sd_f64_less_b"));
            end(ult64(ka, kb));
        }
    };

} // namespace

SoftDoubleModule emit_soft_double(const SoftDoubleUse& use)
{
    Builder builder;
    return builder.finish(use);
}

} // namespace MayaFlux::Portal::Graphics::detail
