#include "PrimitiveMill.hpp"

#include "ShaderFoundry.hpp"
#include "ShaderSpec.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Portal::Graphics {

using Buffers::VKBuffer;

namespace {

    constexpr uint32_t WORKGROUP = 256;
    constexpr uint32_t ABSENT = 0xFFFFFFFFU;

    /** @brief Words per DrawRun in the run table: topology, offset, count. */
    constexpr size_t RUN_WORDS = 3;

    /**
     * @struct MillOffsets
     * @brief Word offsets into one source vertex, resolved from its layout.
     */
    struct MillOffsets {
        uint32_t stride_words { 0 };
        uint32_t position { ABSENT };
        uint32_t scalar { ABSENT };
        uint32_t tangent { ABSENT };
        uint32_t uv { ABSENT };
    };

    /**
     * @brief Resolve @p layout into word offsets.
     * @return false when the stride is not word aligned or no position
     *         attribute is addressable, which makes the layout unmillable.
     */
    bool offsets_from(const Kakshya::VertexLayout& layout, MillOffsets& out)
    {
        if (layout.stride_bytes == 0 || layout.stride_bytes % sizeof(uint32_t) != 0) {
            return false;
        }

        const auto position = layout.find_word_offset(Kakshya::DataModality::VERTEX_POSITIONS_3D);
        if (!position) {
            return false;
        }

        out.stride_words = layout.stride_bytes / static_cast<uint32_t>(sizeof(uint32_t));
        out.position = *position;
        out.scalar = layout.find_word_offset(Kakshya::DataModality::SCALAR_F32).value_or(ABSENT);
        out.tangent = layout.find_word_offset(Kakshya::DataModality::VERTEX_TANGENTS_3D).value_or(ABSENT);
        out.uv = layout.find_word_offset(Kakshya::DataModality::TEXTURE_COORDS_2D).value_or(ABSENT);

        return true;
    }

    /**
     * @struct MillPC
     * @brief Push constants for the mill kernel.
     *
     * Field order and widths must match build_mill_spec()'s pc() declaration
     * order, which is how ShaderSpec assigns offsets.
     */
    struct MillPC {
        uint32_t total;
        uint32_t run_count;
        uint32_t stride_words;
        uint32_t position_offset;
        uint32_t scalar_offset;
        uint32_t uv_offset;
        uint32_t mode;
        uint32_t synth_uv;
        float width_scale;
        float point_scale;
        float fallback_extent;
        float eye_x;
        float eye_y;
        float eye_z;
    };

    static_assert(sizeof(MillPC) == 14 * sizeof(uint32_t),
        "MillPC must be tightly packed 4-byte fields matching build_mill_spec's pc() list");

    void add_helpers(ShaderSpec::Assemble& assemble)
    {
        assemble.function("uint", "find_run", "uint g, uint n",
            "    uint lo = 0u;\n"
            "    uint hi = n;\n"
            "    while (lo + 1u < hi) {\n"
            "        uint mid = (lo + hi) >> 1u;\n"
            "        if (prefix[mid] <= g) { lo = mid; } else { hi = mid; }\n"
            "    }\n"
            "    return lo;\n");

        assemble.function("vec3", "read_pos", "uint v, uint sw, uint po",
            "    uint b = v * sw + po;\n"
            "    return vec3(uintBitsToFloat(src[b]), uintBitsToFloat(src[b + 1u]), "
            "uintBitsToFloat(src[b + 2u]));\n");

        assemble.function("float", "read_extent", "uint v, uint sw, uint so, float fb",
            "    if (so == 0xffffffffu) { return fb; }\n"
            "    return uintBitsToFloat(src[v * sw + so]);\n");

        assemble.function("void", "copy_vertex", "uint d, uint s, uint sw",
            "    uint db = d * sw;\n"
            "    uint sb = s * sw;\n"
            "    for (uint k = 0u; k < sw; ++k) { dst[db + k] = src[sb + k]; }\n");

        assemble.function("void", "write_pos", "uint d, uint sw, uint po, vec3 p",
            "    uint b = d * sw + po;\n"
            "    dst[b] = floatBitsToUint(p.x);\n"
            "    dst[b + 1u] = floatBitsToUint(p.y);\n"
            "    dst[b + 2u] = floatBitsToUint(p.z);\n");

        assemble.function("void", "write_uv", "uint d, uint sw, uint uo, vec2 t",
            "    if (uo == 0xffffffffu) { return; }\n"
            "    uint b = d * sw + uo;\n"
            "    dst[b] = floatBitsToUint(t.x);\n"
            "    dst[b + 1u] = floatBitsToUint(t.y);\n");

        assemble.function("vec3", "view_normal", "vec3 p, vec3 eye",
            "    vec3 n = eye - p;\n"
            "    float l = length(n);\n"
            "    return l < 1e-8 ? vec3(0.0, 0.0, 1.0) : n / l;\n");

        /**
         * Direction arriving at vertex s from the nearest preceding vertex at a
         * different position, within [lo, s]. Skipping coincident vertices lets
         * one path serve both a true strip and a strip emitted as duplicated
         * pairs. Zero when s starts the run.
         */
        assemble.function("vec3", "dir_in", "uint s, uint lo, uint sw, uint po",
            "    vec3 p = read_pos(s, sw, po);\n"
            "    uint c = s;\n"
            "    for (uint k = 0u; k < 4u; ++k) {\n"
            "        if (c <= lo) { break; }\n"
            "        c = c - 1u;\n"
            "        vec3 q = read_pos(c, sw, po);\n"
            "        if (length(p - q) > 1e-6) { return normalize(p - q); }\n"
            "    }\n"
            "    return vec3(0.0);\n");

        /** Direction leaving vertex s toward the next distinct vertex below hi. */
        assemble.function("vec3", "dir_out", "uint s, uint hi, uint sw, uint po",
            "    vec3 p = read_pos(s, sw, po);\n"
            "    uint c = s;\n"
            "    for (uint k = 0u; k < 4u; ++k) {\n"
            "        c = c + 1u;\n"
            "        if (c >= hi) { break; }\n"
            "        vec3 q = read_pos(c, sw, po);\n"
            "        if (length(q - p) > 1e-6) { return normalize(q - p); }\n"
            "    }\n"
            "    return vec3(0.0);\n");

        /**
         * Width at vertex s, averaged over every vertex sharing its position
         * and the nearest distinct vertex on each side.
         *
         * A producer that emits each interior sample twice can give the two
         * copies different thickness, which makes the ribbon step at a point
         * where it should be continuous: one quad ends at one width and the
         * next begins at another. Averaging over position rather than over
         * vertex index makes both copies agree, and folding in the neighbours
         * damps per-sample jitter that reads as serration once the ribbon is
         * more than a pixel wide.
         */
        assemble.function("float", "extent_at",
            "uint s, uint lo, uint hi, uint sw, uint po, uint so, float fb",
            "    vec3 p = read_pos(s, sw, po);\n"
            "    float sum = read_extent(s, sw, so, fb);\n"
            "    float cnt = 1.0;\n"
            "    uint c = s;\n"
            "    for (uint k = 0u; k < 4u; ++k) {\n"
            "        if (c <= lo) { break; }\n"
            "        c = c - 1u;\n"
            "        sum += read_extent(c, sw, so, fb);\n"
            "        cnt += 1.0;\n"
            "        if (length(read_pos(c, sw, po) - p) > 1e-6) { break; }\n"
            "    }\n"
            "    c = s;\n"
            "    for (uint k = 0u; k < 4u; ++k) {\n"
            "        c = c + 1u;\n"
            "        if (c >= hi) { break; }\n"
            "        sum += read_extent(c, sw, so, fb);\n"
            "        cnt += 1.0;\n"
            "        if (length(read_pos(c, sw, po) - p) > 1e-6) { break; }\n"
            "    }\n"
            "    return sum / cnt;\n");

        /**
         * Offset from vertex s to the ribbon edge, mitred.
         *
         * The side vector follows the bisector of the segments meeting at s,
         * so both quads sharing s place their corners identically and the
         * ribbon stays continuous. Scaling by the reciprocal of the bisector's
         * projection onto the segment normal keeps the width constant through
         * the turn; the clamp is the usual miter limit, past which a very sharp
         * corner would otherwise throw the corner out to infinity.
         */
        assemble.function("vec3", "offset_from",
            "vec3 p, vec3 din, vec3 dout, vec3 seg, vec3 eye, float half_w",
            "    vec3 vn = view_normal(p, eye);\n"
            "    vec3 ns = cross(seg, vn);\n"
            "    float nsl = length(ns);\n"
            "    if (nsl < 1e-6) { return vec3(0.0); }\n"
            "    ns = ns / nsl;\n"
            "    vec3 t = din + dout;\n"
            "    float tl = length(t);\n"
            "    if (tl < 1e-6) { return ns * half_w; }\n"
            "    vec3 nm = cross(t / tl, vn);\n"
            "    float nml = length(nm);\n"
            "    if (nml < 1e-6) { return ns * half_w; }\n"
            "    nm = nm / nml;\n"
            "    float proj = dot(nm, ns);\n"
            "    if (abs(proj) < 0.25) { return ns * half_w; }\n"
            "    return nm * (half_w / proj);\n");

        /**
         * Direction of the segment preceding a LINE_LIST pair that starts at
         * @p s, or zero when the previous pair ends somewhere else.
         *
         * Pairs that meet at a shared position are a polyline written as
         * disconnected segments, which is what a path producer emits when it
         * duplicates each interior sample. Pairs that do not meet are genuinely
         * separate edges and must not be joined.
         */
        assemble.function("vec3", "pair_dir_in", "uint s, uint lo, uint sw, uint po",
            "    if (s < lo + 2u) { return vec3(0.0); }\n"
            "    vec3 p = read_pos(s, sw, po);\n"
            "    vec3 b = read_pos(s - 1u, sw, po);\n"
            "    if (length(b - p) > 1e-6) { return vec3(0.0); }\n"
            "    vec3 a = read_pos(s - 2u, sw, po);\n"
            "    vec3 d = b - a;\n"
            "    float l = length(d);\n"
            "    return l < 1e-6 ? vec3(0.0) : d / l;\n");

        /** Direction of the segment following a LINE_LIST pair ending at s. */
        assemble.function("vec3", "pair_dir_out", "uint s, uint hi, uint sw, uint po",
            "    if (s + 2u >= hi) { return vec3(0.0); }\n"
            "    vec3 p = read_pos(s, sw, po);\n"
            "    vec3 a = read_pos(s + 1u, sw, po);\n"
            "    if (length(a - p) > 1e-6) { return vec3(0.0); }\n"
            "    vec3 b = read_pos(s + 2u, sw, po);\n"
            "    vec3 d = b - a;\n"
            "    float l = length(d);\n"
            "    return l < 1e-6 ? vec3(0.0) : d / l;\n");

        /** Width at s averaged with a coincident neighbour at @p o, if any. */
        assemble.function("float", "pair_extent",
            "uint s, uint o, uint lo, uint hi, uint sw, uint po, uint so, float fb",
            "    float e = read_extent(s, sw, so, fb);\n"
            "    if (o < lo || o >= hi) { return e; }\n"
            "    if (length(read_pos(o, sw, po) - read_pos(s, sw, po)) > 1e-6) { return e; }\n"
            "    return 0.5 * (e + read_extent(o, sw, so, fb));\n");
    }

    /**
     * @brief Kernel milling one output vertex per invocation.
     *
     * src and dst are declared UINT32 so an attribute copy is bit-exact
     * whatever the attribute's real type; only position and texture coordinate
     * words are reinterpreted. Topology codes follow PrimitiveTopology's
     * declaration order.
     *
     * A zero-length segment collapses all six corners onto one position and
     * skips the attribute copy: the triangles have no area, so nothing
     * interpolates them and the untouched words are never read. That is the
     * common case for a producer emitting a strip as duplicated vertex pairs,
     * where every second segment is a seam.
     */
    ShaderSpec build_mill_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("src", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("runs", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("prefix", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("dst", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("total", Kakshya::GpuDataFormat::UINT32)
            .pc("run_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("scalar_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("uv_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("mode", Kakshya::GpuDataFormat::UINT32)
            .pc("synth_uv", Kakshya::GpuDataFormat::UINT32)
            .pc("width_scale", Kakshya::GpuDataFormat::FLOAT32)
            .pc("point_scale", Kakshya::GpuDataFormat::FLOAT32)
            .pc("fallback_extent", Kakshya::GpuDataFormat::FLOAT32)
            .pc("eye_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("eye_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("eye_z", Kakshya::GpuDataFormat::FLOAT32)
            .workgroup(WORKGROUP);

        add_helpers(assemble);

        std::string body;
        body += "    if (i >= total) { return; }\n";
        body += "    uint sw = stride_words;\n";
        body += "    uint po = position_offset;\n";
        body += "    vec3 eye = vec3(eye_x, eye_y, eye_z);\n";
        body += "    uint r = find_run(i, run_count);\n";
        body += "    uint rb = r * 3u;\n";
        body += "    uint topo = runs[rb];\n";
        body += "    uint voff = runs[rb + 1u];\n";
        body += "    uint local = i - prefix[r];\n";

        body += "    if (topo == 3u) {\n";
        body += "        copy_vertex(i, voff + local, sw);\n";
        body += "        return;\n";
        body += "    }\n";

        body += "    if (topo == 4u || topo == 5u) {\n";
        body += "        uint tri = local / 3u;\n";
        body += "        uint c = local - tri * 3u;\n";
        body += "        uint a; uint b1; uint b2;\n";
        body += "        if (topo == 5u) { a = 0u; b1 = tri + 1u; b2 = tri + 2u; }\n";
        body += "        else {\n";
        body += "            a = tri; b1 = tri + 1u; b2 = tri + 2u;\n";
        body += "            if ((tri & 1u) == 1u) { uint t = a; a = b1; b1 = t; }\n";
        body += "        }\n";
        body += "        uint pick = c == 0u ? a : (c == 1u ? b1 : b2);\n";
        body += "        copy_vertex(i, voff + pick, sw);\n";
        body += "        return;\n";
        body += "    }\n";

        body += "    uint corner = local % 6u;\n";
        body += "    uint quad = local / 6u;\n";
        body += "    vec2 uv;\n";

        body += "    if (topo == 0u) {\n";
        body += "        uint s = voff + quad;\n";
        body += "        vec3 p = read_pos(s, sw, po);\n";
        body += "        float h = read_extent(s, sw, scalar_offset, fallback_extent)\n";
        body += "               * point_scale * 0.5;\n";
        body += "        vec3 rx; vec3 ry;\n";
        body += "        if (mode == 1u) {\n";
        body += "            rx = vec3(h, 0.0, 0.0);\n";
        body += "            ry = vec3(0.0, h, 0.0);\n";
        body += "        } else {\n";
        body += "            vec3 n = view_normal(p, eye);\n";
        body += "            vec3 up = abs(n.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);\n";
        body += "            rx = normalize(cross(up, n)) * h;\n";
        body += "            ry = normalize(cross(n, rx)) * h;\n";
        body += "        }\n";
        body += "        vec3 o;\n";
        body += "        if (corner == 0u) { o = -rx - ry; uv = vec2(0.0, 0.0); }\n";
        body += "        else if (corner == 1u) { o = rx - ry; uv = vec2(1.0, 0.0); }\n";
        body += "        else if (corner == 2u) { o = rx + ry; uv = vec2(1.0, 1.0); }\n";
        body += "        else if (corner == 3u) { o = -rx - ry; uv = vec2(0.0, 0.0); }\n";
        body += "        else if (corner == 4u) { o = rx + ry; uv = vec2(1.0, 1.0); }\n";
        body += "        else { o = -rx + ry; uv = vec2(0.0, 1.0); }\n";
        body += "        copy_vertex(i, s, sw);\n";
        body += "        write_pos(i, sw, po, p + o);\n";
        body += "        if (synth_uv == 1u) { write_uv(i, sw, uv_offset, uv); }\n";
        body += "        return;\n";
        body += "    }\n";

        body += "    uint s0 = topo == 1u ? voff + quad * 2u : voff + quad;\n";
        body += "    uint s1 = s0 + 1u;\n";
        body += "    vec3 p0 = read_pos(s0, sw, po);\n";
        body += "    vec3 p1 = read_pos(s1, sw, po);\n";
        body += "    vec3 d = p1 - p0;\n";
        body += "    if (length(d) < 1e-6) {\n";
        body += "        write_pos(i, sw, po, p0);\n";
        body += "        return;\n";
        body += "    }\n";
        body += "    float dl = length(d);\n";
        body += "    vec3 dn = d / dl;\n";
        body += "    float h0 = read_extent(s0, sw, scalar_offset, fallback_extent)\n";
        body += "            * width_scale * 0.5;\n";
        body += "    float h1 = read_extent(s1, sw, scalar_offset, fallback_extent)\n";
        body += "            * width_scale * 0.5;\n";
        body += "    vec3 e0; vec3 e1;\n";
        body += "    if (mode == 1u) {\n";
        body += "        vec3 sv = vec3(-dn.y, dn.x, 0.0);\n";
        body += "        e0 = sv * h0;\n";
        body += "        e1 = sv * h1;\n";
        body += "    } else {\n";
        body += "        uint lo = voff;\n";
        body += "        uint hi = voff + runs[rb + 2u];\n";
        body += "        vec3 din; vec3 dout;\n";
        body += "        if (topo == 2u) {\n";
        body += "            din = dir_in(s0, lo, sw, po);\n";
        body += "            dout = dir_out(s1, hi, sw, po);\n";
        body += "            h0 = extent_at(s0, lo, hi, sw, po, scalar_offset, fallback_extent)\n";
        body += "               * width_scale * 0.5;\n";
        body += "            h1 = extent_at(s1, lo, hi, sw, po, scalar_offset, fallback_extent)\n";
        body += "               * width_scale * 0.5;\n";
        body += "        } else {\n";
        body += "            din = pair_dir_in(s0, lo, sw, po);\n";
        body += "            dout = pair_dir_out(s1, hi, sw, po);\n";
        body += "            h0 = pair_extent(s0, s0 - 1u, lo, hi, sw, po, scalar_offset, fallback_extent)\n";
        body += "               * width_scale * 0.5;\n";
        body += "            h1 = pair_extent(s1, s1 + 1u, lo, hi, sw, po, scalar_offset, fallback_extent)\n";
        body += "               * width_scale * 0.5;\n";
        body += "        }\n";
        body += "        e0 = offset_from(p0, din, dn, dn, eye, h0);\n";
        body += "        e1 = offset_from(p1, dn, dout, dn, eye, h1);\n";
        body += "    }\n";
        body += "    vec3 pos; uint pick;\n";
        body += "    if (corner == 0u) { pos = p0 - e0; pick = s0; uv = vec2(0.0, 1.0); }\n";
        body += "    else if (corner == 1u) { pos = p0 + e0; pick = s0; uv = vec2(0.0, 0.0); }\n";
        body += "    else if (corner == 2u) { pos = p1 + e1; pick = s1; uv = vec2(1.0, 0.0); }\n";
        body += "    else if (corner == 3u) { pos = p0 - e0; pick = s0; uv = vec2(0.0, 1.0); }\n";
        body += "    else if (corner == 4u) { pos = p1 + e1; pick = s1; uv = vec2(1.0, 0.0); }\n";
        body += "    else { pos = p1 - e1; pick = s1; uv = vec2(1.0, 1.0); }\n";
        body += "    copy_vertex(i, pick, sw);\n";
        body += "    write_pos(i, sw, po, pos);\n";
        body += "    if (synth_uv == 1u) { write_uv(i, sw, uv_offset, uv); }\n";

        assemble.kernel(KernelSource { .body = std::move(body) });

        return assemble.build();
    }

    MillPC make_pc(
        const MillSpec& spec,
        const MillView& view,
        const MillOffsets& off,
        uint32_t total,
        size_t run_count)
    {
        return MillPC {
            .total = total,
            .run_count = static_cast<uint32_t>(run_count),
            .stride_words = off.stride_words,
            .position_offset = off.position,
            .scalar_offset = spec.use_vertex_extent ? off.scalar : ABSENT,
            .uv_offset = off.uv,
            .mode = spec.ribbon == MillSpec::Ribbon::WorldPlane ? 1U : 0U,
            .synth_uv = spec.synthesize_uv ? 1U : 0U,
            .width_scale = spec.width_scale,
            .point_scale = spec.point_scale,
            .fallback_extent = spec.fallback_extent,
            .eye_x = view.eye.x,
            .eye_y = view.eye.y,
            .eye_z = view.eye.z
        };
    }

    /** @brief Exclusive prefix of milled counts, with the total appended. */
    void build_prefix(std::span<const DrawRun> runs, std::vector<uint32_t>& prefix)
    {
        prefix.assign(runs.size() + 1, 0U);
        for (size_t r = 0; r < runs.size(); ++r) {
            prefix[r + 1] = prefix[r]
                + triangle_vertex_count(runs[r].topology, runs[r].vertex_count);
        }
    }

} // namespace

PrimitiveMill::PrimitiveMill(MillSpec spec)
    : m_spec(spec)
{
}

PrimitiveMill::~PrimitiveMill()
{
    release();
}

uint32_t PrimitiveMill::milled_vertex_count(std::span<const DrawRun> runs)
{
    uint32_t total = 0;
    for (const auto& run : runs) {
        total += triangle_vertex_count(run.topology, run.vertex_count);
    }
    return total;
}

bool PrimitiveMill::ensure_kernel()
{
    if (m_pipeline != INVALID_COMPUTE_PIPELINE) {
        return true;
    }

    const auto spec = build_mill_spec();
    m_push_constant_size = spec.push_constant_bytes;

    if (m_push_constant_size != sizeof(MillPC)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: kernel expects {} push constant bytes but MillPC is {}",
            m_push_constant_size, sizeof(MillPC));
        return false;
    }

    auto& foundry = get_shader_foundry();
    m_shader = foundry.load_shader(spec);
    if (m_shader == INVALID_SHADER) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: kernel failed to compile");
        return false;
    }

    auto& press = get_compute_press();
    m_pipeline = press.create_pipeline_auto(m_shader, m_push_constant_size);
    if (m_pipeline == INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: pipeline creation failed");
        return false;
    }

    m_sets = press.allocate_pipeline_descriptors(m_pipeline);
    if (m_sets.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: descriptor allocation failed");
        return false;
    }

    return true;
}

bool PrimitiveMill::ensure_buffers(
    const std::shared_ptr<VKBuffer>& source,
    const Kakshya::VertexLayout& layout,
    uint32_t total,
    size_t run_count)
{
    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();
    if (!svc) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: BufferService unavailable");
        return false;
    }

    if (total > m_output_capacity) {
        auto milled_layout = layout;
        milled_layout.vertex_count = total;

        m_output = std::make_shared<VKBuffer>(
            static_cast<size_t>(total) * layout.stride_bytes,
            VKBuffer::Usage::VERTEX,
            source->get_modality());
        m_output->set_vertex_layout(milled_layout);
        svc->initialize_buffer(m_output);

        m_output_capacity = total;
        m_bound_source.reset();
    } else if (m_output) {
        auto milled_layout = *m_output->get_vertex_layout();
        milled_layout.vertex_count = total;
        m_output->set_vertex_layout(milled_layout);
    }

    const auto run_bytes = std::max<size_t>(run_count * RUN_WORDS * sizeof(uint32_t), sizeof(uint32_t));
    if (!m_run_buf || m_run_buf->get_size_bytes() < run_bytes) {
        m_run_buf = std::make_shared<VKBuffer>(
            run_bytes, VKBuffer::Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);
        svc->initialize_buffer(m_run_buf);
        m_bound_source.reset();
    }

    const auto prefix_bytes = std::max<size_t>(m_prefix.size() * sizeof(uint32_t), sizeof(uint32_t));
    if (!m_prefix_buf || m_prefix_buf->get_size_bytes() < prefix_bytes) {
        m_prefix_buf = std::make_shared<VKBuffer>(
            prefix_bytes, VKBuffer::Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);
        svc->initialize_buffer(m_prefix_buf);
        m_bound_source.reset();
    }

    return m_output && m_run_buf && m_prefix_buf;
}

void PrimitiveMill::write_descriptors(const std::shared_ptr<VKBuffer>& source)
{
    if (m_bound_source.lock() == source) {
        return;
    }

    auto& foundry = get_shader_foundry();
    const auto set = m_sets.front();

    const auto bind = [&](uint32_t binding, const std::shared_ptr<VKBuffer>& buf) {
        foundry.update_descriptor_buffer(
            set, binding, vk::DescriptorType::eStorageBuffer,
            buf->get_buffer(), 0, buf->get_size_bytes());
    };

    bind(0, source);
    bind(1, m_run_buf);
    bind(2, m_prefix_buf);
    bind(3, m_output);

    m_bound_source = source;
}

void PrimitiveMill::resolve_pending()
{
    if (m_pending_fence == INVALID_FENCE) {
        return;
    }

    auto& foundry = get_shader_foundry();
    foundry.wait_for_fence(m_pending_fence);
    foundry.release_fence(m_pending_fence);
    m_pending_fence = INVALID_FENCE;
}

uint32_t PrimitiveMill::mill(
    const std::shared_ptr<VKBuffer>& source,
    std::span<const DrawRun> runs,
    const MillView& view)
{
    resolve_pending();

    m_milled_count = 0;

    if (!source || runs.empty()) {
        return 0;
    }

    const auto layout = source->get_vertex_layout();
    if (!layout.has_value()) {
        return 0;
    }

    MillOffsets off;
    if (!offsets_from(*layout, off)) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: layout is not millable, stride {} needs word alignment "
            "and an addressable position attribute",
            layout->stride_bytes);
        return 0;
    }

    const uint32_t source_vertices = layout->vertex_count;
    for (const auto& run : runs) {
        if (run.vertex_count == 0) {
            continue;
        }
        if (run.vertex_offset > source_vertices
            || run.vertex_count > source_vertices - run.vertex_offset) {
            MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
                "PrimitiveMill: run [{}, {}) exceeds the source's {} vertices",
                run.vertex_offset, run.vertex_offset + run.vertex_count, source_vertices);
            return 0;
        }
    }

    build_prefix(runs, m_prefix);
    const uint32_t total = m_prefix.back();
    if (total == 0) {
        return 0;
    }

    if (!ensure_kernel() || !ensure_buffers(source, *layout, total, runs.size())) {
        return 0;
    }

    auto* run_ptr = static_cast<uint32_t*>(m_run_buf->get_mapped_ptr());
    auto* prefix_ptr = static_cast<uint32_t*>(m_prefix_buf->get_mapped_ptr());
    if (!run_ptr || !prefix_ptr) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: run or prefix buffer is not host mapped");
        return 0;
    }

    for (size_t r = 0; r < runs.size(); ++r) {
        run_ptr[r * RUN_WORDS + 0] = static_cast<uint32_t>(runs[r].topology);
        run_ptr[r * RUN_WORDS + 1] = runs[r].vertex_offset;
        run_ptr[r * RUN_WORDS + 2] = runs[r].vertex_count;
    }
    std::memcpy(prefix_ptr, m_prefix.data(), m_prefix.size() * sizeof(uint32_t));

    write_descriptors(source);

    const auto pc = make_pc(m_spec, view, off, total, runs.size());

    auto& foundry = get_shader_foundry();
    auto& press = get_compute_press();

    auto cmd_id = foundry.begin_commands(ShaderFoundry::CommandBufferType::COMPUTE);

    foundry.buffer_barrier(
        cmd_id,
        m_output->get_buffer(),
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::AccessFlagBits::eShaderWrite,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::PipelineStageFlagBits::eComputeShader);

    press.bind_all(cmd_id, m_pipeline, m_sets, &pc, sizeof(MillPC));
    press.dispatch(cmd_id, (total + WORKGROUP - 1) / WORKGROUP, 1, 1);

    foundry.buffer_barrier(
        cmd_id,
        m_output->get_buffer(),
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eVertexInput);

    m_pending_fence = foundry.submit_async(cmd_id);
    if (m_pending_fence == INVALID_FENCE) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "PrimitiveMill: dispatch submission failed");
        return 0;
    }

    m_milled_count = total;

    MF_RT_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "PrimitiveMill: milled {} runs into {} vertices", runs.size(), total);

    return total;
}

void PrimitiveMill::release()
{
    resolve_pending();

    auto& foundry = get_shader_foundry();
    auto& press = get_compute_press();

    if (m_pipeline != INVALID_COMPUTE_PIPELINE) {
        press.destroy_pipeline(m_pipeline);
        m_pipeline = INVALID_COMPUTE_PIPELINE;
    }

    if (m_shader != INVALID_SHADER) {
        foundry.destroy_shader(m_shader);
        m_shader = INVALID_SHADER;
    }

    m_sets.clear();
    m_bound_source.reset();
    m_output.reset();
    m_run_buf.reset();
    m_prefix_buf.reset();
    m_prefix.clear();
    m_output_capacity = 0;
    m_milled_count = 0;
    m_push_constant_size = 0;
}

// ===========================================================================
// Host path
// ===========================================================================

namespace {

    /** @brief Reads and writes one vertex record as words, mirroring the kernel. */
    struct HostRecords {
        const uint32_t* src;
        uint32_t* dst;
        uint32_t stride_words;

        [[nodiscard]] glm::vec3 read_pos(uint32_t v, uint32_t po) const
        {
            const uint32_t* w = src + static_cast<size_t>(v) * stride_words + po;
            glm::vec3 p;
            std::memcpy(&p.x, w, sizeof(float));
            std::memcpy(&p.y, w + 1, sizeof(float));
            std::memcpy(&p.z, w + 2, sizeof(float));
            return p;
        }

        [[nodiscard]] float read_extent(uint32_t v, uint32_t so, float fb) const
        {
            if (so == ABSENT) {
                return fb;
            }
            float f = 0.0F;
            std::memcpy(&f, src + static_cast<size_t>(v) * stride_words + so, sizeof(float));
            return f;
        }

        void copy_vertex(uint32_t d, uint32_t s) const
        {
            std::memcpy(
                dst + static_cast<size_t>(d) * stride_words,
                src + static_cast<size_t>(s) * stride_words,
                static_cast<size_t>(stride_words) * sizeof(uint32_t));
        }

        void write_pos(uint32_t d, uint32_t po, const glm::vec3& p) const
        {
            uint32_t* w = dst + static_cast<size_t>(d) * stride_words + po;
            std::memcpy(w, &p.x, sizeof(float));
            std::memcpy(w + 1, &p.y, sizeof(float));
            std::memcpy(w + 2, &p.z, sizeof(float));
        }

        void write_uv(uint32_t d, uint32_t uo, const glm::vec2& t) const
        {
            if (uo == ABSENT) {
                return;
            }
            uint32_t* w = dst + static_cast<size_t>(d) * stride_words + uo;
            std::memcpy(w, &t.x, sizeof(float));
            std::memcpy(w + 1, &t.y, sizeof(float));
        }
    };

    glm::vec3 host_view_normal(const glm::vec3& p, const glm::vec3& eye)
    {
        const glm::vec3 n = eye - p;
        const float l = glm::length(n);
        return l < 1e-8F ? glm::vec3(0.0F, 0.0F, 1.0F) : n / l;
    }

    glm::vec3 host_side_at(
        const glm::vec3& p,
        const glm::vec3& seg,
        const glm::vec3& eye,
        float half_w,
        bool world_plane)
    {
        glm::vec3 dir = seg;
        const float dl = glm::length(dir);
        if (dl < 1e-8F) {
            return glm::vec3(0.0F);
        }
        dir /= dl;

        glm::vec3 s = world_plane
            ? glm::vec3(-dir.y, dir.x, 0.0F)
            : glm::cross(dir, host_view_normal(p, eye));

        const float sl = glm::length(s);
        return (sl < 1e-6F ? glm::vec3(0.0F, 1.0F, 0.0F) : s / sl) * half_w;
    }

} // namespace

Kakshya::VertexLayout mill_on_host(
    std::span<const uint8_t> src,
    const Kakshya::VertexLayout& layout,
    std::span<const DrawRun> runs,
    const MillSpec& spec,
    const MillView& view,
    std::vector<uint8_t>& dst)
{
    auto result = layout;
    result.vertex_count = 0;
    dst.clear();

    MillOffsets off;
    if (!offsets_from(layout, off)) {
        return result;
    }

    std::vector<uint32_t> prefix;
    build_prefix(runs, prefix);
    const uint32_t total = prefix.empty() ? 0U : prefix.back();
    if (total == 0) {
        return result;
    }

    dst.assign(static_cast<size_t>(total) * layout.stride_bytes, 0);
    result.vertex_count = total;

    const HostRecords rec {
        .src = reinterpret_cast<const uint32_t*>(src.data()),
        .dst = reinterpret_cast<uint32_t*>(dst.data()),
        .stride_words = off.stride_words
    };

    const bool world_plane = spec.ribbon == MillSpec::Ribbon::WorldPlane;
    const uint32_t scalar_offset = spec.use_vertex_extent ? off.scalar : ABSENT;

    for (size_t r = 0; r < runs.size(); ++r) {
        const auto& run = runs[r];
        const uint32_t emitted = prefix[r + 1] - prefix[r];
        const uint32_t base = prefix[r];
        const uint32_t voff = run.vertex_offset;

        for (uint32_t local = 0; local < emitted; ++local) {
            const uint32_t out = base + local;

            if (run.topology == PrimitiveTopology::TRIANGLE_LIST) {
                rec.copy_vertex(out, voff + local);
                continue;
            }

            if (run.topology == PrimitiveTopology::TRIANGLE_STRIP
                || run.topology == PrimitiveTopology::TRIANGLE_FAN) {
                const uint32_t tri = local / 3U;
                const uint32_t c = local - tri * 3U;
                uint32_t a = 0;
                uint32_t b1 = tri + 1U;
                uint32_t b2 = tri + 2U;
                if (run.topology == PrimitiveTopology::TRIANGLE_STRIP) {
                    a = tri;
                    if ((tri & 1U) == 1U) {
                        std::swap(a, b1);
                    }
                }
                const uint32_t pick = c == 0U ? a : (c == 1U ? b1 : b2);
                rec.copy_vertex(out, voff + pick);
                continue;
            }

            const uint32_t corner = local % 6U;
            const uint32_t quad = local / 6U;

            if (run.topology == PrimitiveTopology::POINT_LIST) {
                const uint32_t s = voff + quad;
                const glm::vec3 p = rec.read_pos(s, off.position);
                const float h = rec.read_extent(s, scalar_offset, spec.fallback_extent)
                    * spec.point_scale * 0.5F;

                glm::vec3 rx;
                glm::vec3 ry;
                if (world_plane) {
                    rx = glm::vec3(h, 0.0F, 0.0F);
                    ry = glm::vec3(0.0F, h, 0.0F);
                } else {
                    const glm::vec3 n = host_view_normal(p, view.eye);
                    const glm::vec3 up = std::abs(n.y) < 0.99F
                        ? glm::vec3(0.0F, 1.0F, 0.0F)
                        : glm::vec3(1.0F, 0.0F, 0.0F);
                    rx = glm::normalize(glm::cross(up, n)) * h;
                    ry = glm::normalize(glm::cross(n, rx)) * h;
                }

                glm::vec3 o;
                glm::vec2 uv;
                switch (corner) {
                case 1:
                    o = rx - ry;
                    uv = { 1.0F, 0.0F };
                    break;
                case 2:
                case 4:
                    o = rx + ry;
                    uv = { 1.0F, 1.0F };
                    break;
                case 5:
                    o = -rx + ry;
                    uv = { 0.0F, 1.0F };
                    break;
                default:
                    o = -rx - ry;
                    uv = { 0.0F, 0.0F };
                    break;
                }

                rec.copy_vertex(out, s);
                rec.write_pos(out, off.position, p + o);
                if (spec.synthesize_uv) {
                    rec.write_uv(out, off.uv, uv);
                }
                continue;
            }

            const uint32_t s0 = run.topology == PrimitiveTopology::LINE_LIST
                ? voff + quad * 2U
                : voff + quad;
            const uint32_t s1 = s0 + 1U;

            const glm::vec3 p0 = rec.read_pos(s0, off.position);
            const glm::vec3 p1 = rec.read_pos(s1, off.position);
            const glm::vec3 d = p1 - p0;

            if (glm::length(d) < 1e-6F) {
                rec.write_pos(out, off.position, p0);
                continue;
            }

            const float h0 = rec.read_extent(s0, scalar_offset, spec.fallback_extent)
                * spec.width_scale * 0.5F;
            const float h1 = rec.read_extent(s1, scalar_offset, spec.fallback_extent)
                * spec.width_scale * 0.5F;

            const glm::vec3 e0 = host_side_at(
                p0, d, view.eye, h0, world_plane);
            const glm::vec3 e1 = host_side_at(
                p1, d, view.eye, h1, world_plane);

            glm::vec3 pos;
            uint32_t pick = s0;
            glm::vec2 uv;
            switch (corner) {
            case 1:
                pos = p0 + e0;
                uv = { 0.0F, 0.0F };
                break;
            case 2:
            case 4:
                pos = p1 + e1;
                pick = s1;
                uv = { 1.0F, 0.0F };
                break;
            case 5:
                pos = p1 - e1;
                pick = s1;
                uv = { 1.0F, 1.0F };
                break;
            default:
                pos = p0 - e0;
                uv = { 0.0F, 1.0F };
                break;
            }

            rec.copy_vertex(out, pick);
            rec.write_pos(out, off.position, pos);
            if (spec.synthesize_uv) {
                rec.write_uv(out, off.uv, uv);
            }
        }
    }

    return result;
}

} // namespace MayaFlux::Portal::Graphics
