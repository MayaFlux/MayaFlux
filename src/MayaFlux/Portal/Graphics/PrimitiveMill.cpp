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
        uint32_t tangent_offset;
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

    static_assert(sizeof(MillPC) == 15 * sizeof(uint32_t),
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

        assemble.function("vec3", "read_tangent", "uint v, uint sw, uint to",
            "    if (to == 0xffffffffu) { return vec3(0.0); }\n"
            "    uint b = v * sw + to;\n"
            "    return vec3(uintBitsToFloat(src[b]), uintBitsToFloat(src[b + 1u]), "
            "uintBitsToFloat(src[b + 2u]));\n");

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
         * Side vector at one ribbon endpoint. Taken from the vertex's own
         * tangent when it carries one, so two segments sharing a vertex agree
         * and the ribbon joins without either reading its neighbour. Falls back
         * to the segment direction, which gives butt ends.
         */
        assemble.function("vec3", "side_at",
            "vec3 p, vec3 t, vec3 seg, vec3 eye, float half_w, uint mode",
            "    vec3 dir = length(t) < 1e-6 ? seg : t;\n"
            "    float dl = length(dir);\n"
            "    if (dl < 1e-8) { return vec3(0.0); }\n"
            "    dir = dir / dl;\n"
            "    vec3 s = mode == 1u ? vec3(-dir.y, dir.x, 0.0)\n"
            "                        : cross(dir, view_normal(p, eye));\n"
            "    float sl = length(s);\n"
            "    return (sl < 1e-6 ? vec3(0.0, 1.0, 0.0) : s / sl) * half_w;\n");
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
            .pc("tangent_offset", Kakshya::GpuDataFormat::UINT32)
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
        body += "    if (length(d) < 1e-8) {\n";
        body += "        write_pos(i, sw, po, p0);\n";
        body += "        return;\n";
        body += "    }\n";
        body += "    float h0 = read_extent(s0, sw, scalar_offset, fallback_extent)\n";
        body += "            * width_scale * 0.5;\n";
        body += "    float h1 = read_extent(s1, sw, scalar_offset, fallback_extent)\n";
        body += "            * width_scale * 0.5;\n";
        body += "    vec3 e0 = side_at(p0, read_tangent(s0, sw, tangent_offset), d, eye, h0, mode);\n";
        body += "    vec3 e1 = side_at(p1, read_tangent(s1, sw, tangent_offset), d, eye, h1, mode);\n";
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
            .scalar_offset = off.scalar,
            .tangent_offset = off.tangent,
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

uint32_t PrimitiveMill::mill(
    const std::shared_ptr<VKBuffer>& source,
    std::span<const DrawRun> runs,
    const MillView& view)
{
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

    press.bind_all(cmd_id, m_pipeline, m_sets, &pc, sizeof(MillPC));
    press.dispatch(cmd_id, (total + WORKGROUP - 1) / WORKGROUP, 1, 1);

    foundry.buffer_barrier(
        cmd_id,
        m_output->get_buffer(),
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eVertexInput);

    foundry.submit_and_wait(cmd_id);

    m_milled_count = total;

    MF_RT_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "PrimitiveMill: milled {} runs into {} vertices", runs.size(), total);

    return total;
}

void PrimitiveMill::release()
{
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

        [[nodiscard]] glm::vec3 read_vec3(uint32_t v, uint32_t offset) const
        {
            if (offset == ABSENT) {
                return glm::vec3(0.0F);
            }
            return read_pos(v, offset);
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
        const glm::vec3& t,
        const glm::vec3& seg,
        const glm::vec3& eye,
        float half_w,
        bool world_plane)
    {
        glm::vec3 dir = glm::length(t) < 1e-6F ? seg : t;
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
                const float h = rec.read_extent(s, off.scalar, spec.fallback_extent)
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

            if (glm::length(d) < 1e-8F) {
                rec.write_pos(out, off.position, p0);
                continue;
            }

            const float h0 = rec.read_extent(s0, off.scalar, spec.fallback_extent)
                * spec.width_scale * 0.5F;
            const float h1 = rec.read_extent(s1, off.scalar, spec.fallback_extent)
                * spec.width_scale * 0.5F;

            const glm::vec3 e0 = host_side_at(
                p0, rec.read_vec3(s0, off.tangent), d, view.eye, h0, world_plane);
            const glm::vec3 e1 = host_side_at(
                p1, rec.read_vec3(s1, off.tangent), d, view.eye, h1, world_plane);

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
