#include "MutationClaimProcessor.hpp"

#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Nodes/Network/Operators/ParticleFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers {

void MutationConfig::declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const
{
    buffer->declare_state("mutation_claimed_by", hash.particle_count, sizeof(uint32_t), false);
    buffer->declare_state("mutation_swallow_count", hash.particle_count, sizeof(uint32_t), false);
}

namespace {

    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelSource;
    using Portal::Graphics::ShaderSpec;

} // namespace

//=============================================================================
// ClaimInitProcessor
//=============================================================================

namespace {

    ShaderSpec build_claim_init_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("claimed_by", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .ssbo("swallow_count", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    claimed_by[i] = i;\n";
        body += "    swallow_count[i] = 0u;\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "claimed_by", "swallow_count", "particle_count", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

ClaimInitProcessor::ClaimInitProcessor(const MutationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "claimed_by", .binding = 0, .field = "mutation_claimed_by" },
              FieldBinding { .name = "swallow_count", .binding = 1, .field = "mutation_swallow_count" } },
          build_claim_init_spec())
    , m_params { .particle_count = config.hash.particle_count }
{
}

void ClaimInitProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

//=============================================================================
// ClaimProcessor
//=============================================================================

namespace {

    ShaderSpec build_claim_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_start", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("particle_index", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("claimed_by", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("absorb_radius", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_z", Kakshya::GpuDataFormat::FLOAT32)
            .pc("cell_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_x", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_y", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_z", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    uint b = i * stride_words;\n";
        body += "    vec3 p = vec3(vertices[b + position_offset], "
                 "vertices[b + position_offset + 1u], vertices[b + position_offset + 2u]);\n";
        body += "    vec3 gmin = vec3(grid_min_x, grid_min_y, grid_min_z);\n";
        body += "    uvec3 dims = uvec3(dim_x, dim_y, dim_z);\n";
        body += "    ivec3 base = ivec3(floor((p - gmin) / cell_size));\n";
        body += "    base = clamp(base, ivec3(0), ivec3(dims) - ivec3(1));\n";
        body += "\n";
        body += "    for (int dz = -1; dz <= 1; dz = dz + 1) {\n";
        body += "        for (int dy = -1; dy <= 1; dy = dy + 1) {\n";
        body += "            for (int dx = -1; dx <= 1; dx = dx + 1) {\n";
        body += "                ivec3 nc = base + ivec3(dx, dy, dz);\n";
        body += "                if (nc.x < 0 || nc.y < 0 || nc.z < 0 || "
                 "nc.x >= int(dim_x) || nc.y >= int(dim_y) || nc.z >= int(dim_z)) {\n";
        body += "                    continue;\n";
        body += "                }\n";
        body += "                uint cell = uint(nc.x) + uint(nc.y) * dim_x + uint(nc.z) * dim_x * dim_y;\n";
        body += "                uint start = cell_start[cell];\n";
        body += "                uint count = cell_count[cell];\n";
        body += "                for (uint k = 0u; k < count; k = k + 1u) {\n";
        body += "                    uint j = particle_index[start + k];\n";
        body += "                    if (j <= i) { continue; }\n";
        body += "                    uint bj = j * stride_words;\n";
        body += "                    vec3 pj = vec3(vertices[bj + position_offset], "
                 "vertices[bj + position_offset + 1u], vertices[bj + position_offset + 2u]);\n";
        body += "                    if (length(pj - p) < absorb_radius) {\n";
        body += "                        atomicMin(claimed_by[j], i);\n";
        body += "                    }\n";
        body += "                }\n";
        body += "            }\n";
        body += "        }\n";
        body += "    }\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "vertices", "cell_start", "cell_count", "particle_index", "claimed_by",
                "particle_count", "stride_words", "position_offset", "absorb_radius",
                "grid_min_x", "grid_min_y", "grid_min_z", "cell_size", "dim_x", "dim_y", "dim_z", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

ClaimProcessor::ClaimProcessor(const MutationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "cell_start", .binding = 1, .field = "hash_cell_start" },
              FieldBinding { .name = "cell_count", .binding = 2, .field = "hash_cell_count" },
              FieldBinding { .name = "particle_index", .binding = 3, .field = "hash_particle_index" },
              FieldBinding { .name = "claimed_by", .binding = 4, .field = "mutation_claimed_by" } },
          build_claim_spec())
    , m_params {
        .particle_count = config.hash.particle_count,
        .stride_words = config.hash.stride_words,
        .position_offset = config.hash.position_word_offset,
        .absorb_radius = config.absorb_radius,
        .grid_min_x = config.hash.grid_min.x,
        .grid_min_y = config.hash.grid_min.y,
        .grid_min_z = config.hash.grid_min.z,
        .cell_size = config.hash.cell_size,
        .dim_x = config.hash.grid_dims.x,
        .dim_y = config.hash.grid_dims.y,
        .dim_z = config.hash.grid_dims.z,
    }
{
}

void ClaimProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

//=============================================================================
// ClaimFlattenProcessor
//=============================================================================

namespace {

    ShaderSpec build_claim_flatten_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("claimed_by", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    claimed_by[i] = claimed_by[claimed_by[i]];\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "claimed_by", "particle_count", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

    /**
     * @brief Smallest k such that 2^k >= n, floored at 1.
     *
     * Number of pointer-jumping rounds sufficient to flatten any chain up to
     * length n: each round at most halves the distance from an element to
     * its root, and no chain can exceed particle_count links.
     */
    uint32_t rounds_to_flatten(uint32_t n)
    {
        uint32_t k = 0;
        while ((1U << k) < n) {
            ++k;
        }
        return std::max(1U, k);
    }

} // namespace

ClaimFlattenProcessor::ClaimFlattenProcessor(const MutationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "claimed_by", .binding = 0, .field = "mutation_claimed_by" } },
          build_claim_flatten_spec())
    , m_params { .particle_count = config.hash.particle_count }
{
}

void ClaimFlattenProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
    set_iteration_count(rounds_to_flatten(m_params.particle_count));
}

void ClaimFlattenProcessor::on_iteration_barrier(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t /*index*/)
{
    auto handle = get_network_buffer()->read_state_handle("mutation_claimed_by");

    Portal::Graphics::get_shader_foundry().buffer_barrier(
        cmd_id, handle,
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader);
}

//=============================================================================
// ClaimAccumulateProcessor
//=============================================================================

namespace {

    ShaderSpec build_claim_accumulate_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("claimed_by", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("swallow_count", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    uint root = claimed_by[i];\n";
        body += "    if (root != i) {\n";
        body += "        atomicAdd(swallow_count[root], 1u);\n";
        body += "    }\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "claimed_by", "swallow_count", "particle_count", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

ClaimAccumulateProcessor::ClaimAccumulateProcessor(const MutationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "claimed_by", .binding = 0, .field = "mutation_claimed_by" },
              FieldBinding { .name = "swallow_count", .binding = 1, .field = "mutation_swallow_count" } },
          build_claim_accumulate_spec())
    , m_params { .particle_count = config.hash.particle_count }
{
}

void ClaimAccumulateProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

//=============================================================================
// ClaimSwallowProcessor
//=============================================================================

namespace {

    ShaderSpec build_claim_swallow_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("claimed_by", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("swallow_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("color_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("size_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("base_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("growth_rate", Kakshya::GpuDataFormat::FLOAT32)
            .pc("max_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_factor", Kakshya::GpuDataFormat::FLOAT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    uint root = claimed_by[i];\n";
        body += "    uint b = i * stride_words;\n";
        body += "\n";
        body += "    vec3 palette[6] = vec3[6](\n";
        body += "        vec3(0.20, 0.90, 0.90),\n";
        body += "        vec3(0.95, 0.25, 0.75),\n";
        body += "        vec3(0.95, 0.85, 0.15),\n";
        body += "        vec3(0.95, 0.45, 0.10),\n";
        body += "        vec3(0.40, 0.95, 0.25),\n";
        body += "        vec3(0.55, 0.35, 0.95));\n";
        body += "    vec3 base_col = palette[root % 6u];\n";
        body += "\n";
        body += "    if (root == i) {\n";
        body += "        float count = float(swallow_count[i]);\n";
        body += "        float size = clamp(base_size + count * growth_rate, base_size, max_size);\n";
        body += "        vertices[b + size_offset] = size;\n";
        body += "        vertices[b + color_offset] = base_col.x;\n";
        body += "        vertices[b + color_offset + 1u] = base_col.y;\n";
        body += "        vertices[b + color_offset + 2u] = base_col.z;\n";
        body += "        return;\n";
        body += "    }\n";
        body += "\n";
        body += "    uint rb = root * stride_words;\n";
        body += "    vec3 root_pos = vec3(vertices[rb + position_offset], "
                 "vertices[rb + position_offset + 1u], vertices[rb + position_offset + 2u]);\n";
        body += "    vertices[b + position_offset] = root_pos.x;\n";
        body += "    vertices[b + position_offset + 1u] = root_pos.y;\n";
        body += "    vertices[b + position_offset + 2u] = root_pos.z;\n";
        body += "\n";
        body += "    vec3 dim_col = base_col * dim_factor;\n";
        body += "    vertices[b + color_offset] = dim_col.x;\n";
        body += "    vertices[b + color_offset + 1u] = dim_col.y;\n";
        body += "    vertices[b + color_offset + 2u] = dim_col.z;\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "vertices", "claimed_by", "swallow_count", "particle_count",
                "stride_words", "position_offset", "color_offset", "size_offset",
                "base_size", "growth_rate", "max_size", "dim_factor", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

    /**
     * @brief Resolve colour and size word offsets or fail loudly.
     *
     * A free function rather than inline in the member-initialiser list,
     * the same shape HashDensityColorProcessor's require_color_offset
     * uses: the constructor needs both before it can build m_params.
     */
    std::pair<uint32_t, uint32_t> require_color_and_size_offset(
        const std::shared_ptr<Nodes::Network::ParticleFieldOperator>& particle_op)
    {
        if (!particle_op) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "ClaimSwallowProcessor: null particle operator");
        }

        const auto& layout = particle_op->get_layout();

        const auto color_offset = layout.find_word_offset(Kakshya::DataModality::VERTEX_COLORS_RGB);
        const auto size_attr = std::ranges::find_if(layout.attributes,
            [](const auto& attr) { return attr.name == "size"; });
        const bool have_size = size_attr != layout.attributes.end() && size_attr->offset_in_vertex % 4 == 0;

        if (!color_offset.has_value() || !have_size) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "ClaimSwallowProcessor: vertex layout is missing a colour or word-aligned size attribute");
        }

        return { *color_offset, size_attr->offset_in_vertex / 4 };
    }

} // namespace

ClaimSwallowProcessor::ClaimSwallowProcessor(
    const MutationConfig& config,
    std::shared_ptr<Nodes::Network::ParticleFieldOperator> particle_op)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "claimed_by", .binding = 1, .field = "mutation_claimed_by" },
              FieldBinding { .name = "swallow_count", .binding = 2, .field = "mutation_swallow_count" } },
          build_claim_swallow_spec())
    , m_params {
        .particle_count = config.hash.particle_count,
        .stride_words = config.hash.stride_words,
        .position_offset = config.hash.position_word_offset,
        .color_offset = require_color_and_size_offset(particle_op).first,
        .size_offset = require_color_and_size_offset(particle_op).second,
        .base_size = particle_op->get_particle_config().swallow_base_size,
        .growth_rate = particle_op->get_particle_config().swallow_growth_rate,
        .max_size = particle_op->get_particle_config().swallow_max_size,
        .dim_factor = particle_op->get_particle_config().swallow_dim_factor,
    }
    , m_particle_op(std::move(particle_op))
    , m_built_revision(m_particle_op->revision())
{
}

void ClaimSwallowProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

bool ClaimSwallowProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!NetworkStateFieldProcessor::on_before_execute(cmd_id, buffer)) {
        return false;
    }

    if (m_particle_op->revision() != m_built_revision) {
        const auto& pconfig = m_particle_op->get_particle_config();
        m_params.base_size = pconfig.swallow_base_size;
        m_params.growth_rate = pconfig.swallow_growth_rate;
        m_params.max_size = pconfig.swallow_max_size;
        m_params.dim_factor = pconfig.swallow_dim_factor;
        set_push_constant_data(m_params);
        m_built_revision = m_particle_op->revision();
    }

    return true;
}

} // namespace MayaFlux::Buffers
