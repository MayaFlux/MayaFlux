#include "MutationClaimProcessor.hpp"

#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/PhysicsOperator.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers {

void MutationConfig::declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const
{
    buffer->declare_state("mutation_claimed_by", hash.particle_count, sizeof(uint32_t), false);
    buffer->declare_state("mutation_swallow_count", hash.particle_count, sizeof(uint32_t), false);
    buffer->declare_state("mutation_claim_events", 1, sizeof(uint32_t), false);
    buffer->declare_state("mutation_accreted_mass", hash.particle_count, sizeof(float), false);
}

namespace {

    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelSource;
    using Portal::Graphics::ShaderSpec;

    /**
     * @brief Resolve colour and size word offsets or fail loudly.
     *
     * A free function rather than inline in a member-initialiser list, the
     * same shape HashDensityColorProcessor's require_color_offset uses: the
     * constructor needs both before it can build m_params.
     */
    std::pair<uint32_t, uint32_t> require_color_and_size_offset(
        const std::shared_ptr<Nodes::Network::GpuFieldOperator>& particle_op)
    {
        if (!particle_op) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "require_color_and_size_offset: null particle operator");
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
                "require_color_and_size_offset: vertex layout is missing a colour or "
                "word-aligned size attribute");
        }

        return { *color_offset, size_attr->offset_in_vertex / 4 };
    }

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
            .ssbo("claim_events", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    claimed_by[i] = i;\n";
        body += "    swallow_count[i] = 0u;\n";
        body += "    if (i == 0u) { claim_events[0] = 0u; }\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "claimed_by", "swallow_count", "claim_events", "particle_count", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

ClaimInitProcessor::ClaimInitProcessor(const MutationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "claimed_by", .binding = 0, .field = "mutation_claimed_by" },
              FieldBinding { .name = "swallow_count", .binding = 1, .field = "mutation_swallow_count" },
              FieldBinding { .name = "claim_events", .binding = 2, .field = "mutation_claim_events" } },
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

    ShaderSpec build_claim_spec(bool gate_alive)
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_start", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("particle_index", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("claimed_by", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .ssbo("claim_events", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .ssbo("accreted_mass", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cluster_id", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32);

        if (gate_alive) {
            assemble.ssbo("alive", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32);
        }

        assemble
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("absorb_radius", Kakshya::GpuDataFormat::FLOAT32)
            .pc("capture_growth", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_z", Kakshya::GpuDataFormat::FLOAT32)
            .pc("cell_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_x", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_y", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_z", Kakshya::GpuDataFormat::UINT32)
            .pc("cross_cluster", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        if (gate_alive) {
            body += "    if (alive[i] == 0u) { return; }\n";
        }
        body += "    uint b = i * stride_words;\n";
        body += "    vec3 p = vec3(vertices[b + position_offset], "
                 "vertices[b + position_offset + 1u], vertices[b + position_offset + 2u]);\n";
        body += "    float capture_radius = max(absorb_radius, "
                "pow(max(accreted_mass[i], 0.0001), 1.0 / 3.0) * capture_growth);\n";
        body += "    uint my_cluster = cluster_id[i];\n";
        body += "    vec3 gmin = vec3(grid_min_x, grid_min_y, grid_min_z);\n";
        body += "    uvec3 dims = uvec3(dim_x, dim_y, dim_z);\n";
        body += "    ivec3 base = ivec3(floor((p - gmin) / cell_size));\n";
        body += "    base = clamp(base, ivec3(0), ivec3(dims) - ivec3(1));\n";
        body += "\n";
        body += "    int reach = clamp(int(ceil(capture_radius / cell_size)), 1, 8);\n";
        body += "    for (int dz = -reach; dz <= reach; dz = dz + 1) {\n";
        body += "        for (int dy = -reach; dy <= reach; dy = dy + 1) {\n";
        body += "            for (int dx = -reach; dx <= reach; dx = dx + 1) {\n";
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
        body += "                    if (cross_cluster == 0u && cluster_id[j] != my_cluster) { continue; }\n";
        body += "                    uint bj = j * stride_words;\n";
        body += "                    vec3 pj = vec3(vertices[bj + position_offset], "
                 "vertices[bj + position_offset + 1u], vertices[bj + position_offset + 2u]);\n";
        body += "                    if (length(pj - p) < capture_radius) {\n";
        body += "                        atomicMin(claimed_by[j], i);\n";
        body += "                        atomicAdd(claim_events[0], 1u);\n";
        body += "                    }\n";
        body += "                }\n";
        body += "            }\n";
        body += "        }\n";
        body += "    }\n";

        std::vector<std::string> param_names {
            "vertices", "cell_start", "cell_count", "particle_index", "claimed_by",
            "claim_events", "accreted_mass", "cluster_id"
        };
        if (gate_alive) {
            param_names.emplace_back("alive");
        }
        param_names.insert(param_names.end(),
            { "particle_count", "stride_words", "position_offset", "absorb_radius", "capture_growth",
                "grid_min_x", "grid_min_y", "grid_min_z", "cell_size", "dim_x", "dim_y", "dim_z",
                "cross_cluster", "i" });

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = std::move(param_names),
            .body = std::move(body),
        });

        return assemble.build();
    }

    std::vector<NetworkStateFieldProcessor::FieldBinding> claim_bindings(bool gate_alive)
    {
        std::vector<NetworkStateFieldProcessor::FieldBinding> bindings {
            { .name = "vertices", .binding = 0, .field = {} },
            { .name = "cell_start", .binding = 1, .field = "hash_cell_start" },
            { .name = "cell_count", .binding = 2, .field = "hash_cell_count" },
            { .name = "particle_index", .binding = 3, .field = "hash_particle_index" },
            { .name = "claimed_by", .binding = 4, .field = "mutation_claimed_by" },
            { .name = "claim_events", .binding = 5, .field = "mutation_claim_events" },
            { .name = "accreted_mass", .binding = 6, .field = "mutation_accreted_mass" },
            { .name = "cluster_id", .binding = 7, .field = "hash_cluster_id" },
        };
        if (gate_alive) {
            bindings.push_back({ .name = "alive", .binding = 8, .field = "mutation_alive" });
        }
        return bindings;
    }

} // namespace

ClaimProcessor::ClaimProcessor(
    const MutationConfig& config,
    std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op,
    bool gate_alive)
    : NetworkStateFieldProcessor(claim_bindings(gate_alive), build_claim_spec(gate_alive))
    , m_params {
        .particle_count = config.hash.particle_count,
        .stride_words = config.hash.stride_words,
        .position_offset = config.hash.position_word_offset,
        .absorb_radius = config.absorb_radius,
        .capture_growth = particle_op->get_field_config().capture_growth,
        .grid_min_x = config.hash.grid_min.x,
        .grid_min_y = config.hash.grid_min.y,
        .grid_min_z = config.hash.grid_min.z,
        .cell_size = config.hash.cell_size,
        .dim_x = config.hash.grid_dims.x,
        .dim_y = config.hash.grid_dims.y,
        .dim_z = config.hash.grid_dims.z,
        .cross_cluster = particle_op->get_field_config().cross_cluster ? 1U : 0U,
    }
    , m_particle_op(std::move(particle_op))
    , m_built_revision(m_particle_op->revision())
{
}

void ClaimProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

bool ClaimProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!NetworkStateFieldProcessor::on_before_execute(cmd_id, buffer)) {
        return false;
    }

    if (m_particle_op->revision() != m_built_revision) {
        const auto& pconfig = m_particle_op->get_field_config();
        m_params.capture_growth = pconfig.capture_growth;
        m_params.cross_cluster = pconfig.cross_cluster ? 1U : 0U;
        set_push_constant_data(m_params);
        m_built_revision = m_particle_op->revision();
    }

    return true;
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

    ShaderSpec build_claim_accumulate_spec(bool gate_alive)
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("claimed_by", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("swallow_count", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32);

        if (gate_alive) {
            assemble
                .ssbo("alive", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
                .ssbo("vertices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32);
        }

        assemble.pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("size_offset", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        if (gate_alive) {
            body += "    if (alive[i] == 0u) {\n";
            body += "        vertices[i * stride_words + size_offset] = 0.0;\n";
            body += "        return;\n";
            body += "    }\n";
        }
        body += "    uint root = claimed_by[i];\n";
        body += "    if (root != i) {\n";
        body += "        atomicAdd(swallow_count[root], 1u);\n";
        if (gate_alive) {
            body += "        alive[i] = 0u;\n";
            body += "        vertices[i * stride_words + size_offset] = 0.0;\n";
        }
        body += "    }\n";

        std::vector<std::string> param_names { "claimed_by", "swallow_count" };
        if (gate_alive) {
            param_names.emplace_back("alive");
            param_names.emplace_back("vertices");
        }
        param_names.emplace_back("particle_count");
        param_names.emplace_back("stride_words");
        param_names.emplace_back("size_offset");
        param_names.emplace_back("i");

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = std::move(param_names),
            .body = std::move(body),
        });

        return assemble.build();
    }

    std::vector<NetworkStateFieldProcessor::FieldBinding> claim_accumulate_bindings(bool gate_alive)
    {
        std::vector<NetworkStateFieldProcessor::FieldBinding> bindings {
            { .name = "claimed_by", .binding = 0, .field = "mutation_claimed_by" },
            { .name = "swallow_count", .binding = 1, .field = "mutation_swallow_count" },
        };
        if (gate_alive) {
            bindings.push_back({ .name = "alive", .binding = 2, .field = "mutation_alive" });
            bindings.push_back({ .name = "vertices", .binding = 3, .field = {} });
        }
        return bindings;
    }

} // namespace

ClaimAccumulateProcessor::ClaimAccumulateProcessor(
    const MutationConfig& config,
    Nodes::Network::PhysicsOperator* physics_op,
    uint32_t live_count,
    const std::shared_ptr<Nodes::Network::GpuFieldOperator>& particle_op)
    : NetworkStateFieldProcessor(
          claim_accumulate_bindings(live_count != 0),
          build_claim_accumulate_spec(live_count != 0))
    , m_params {
        .particle_count = config.hash.particle_count,
        .stride_words = config.hash.stride_words,
        .size_offset = live_count != 0 ? require_color_and_size_offset(particle_op).second : 0U,
    }
    , m_physics_op(physics_op)
    , m_live_count(live_count != 0 ? live_count : config.hash.particle_count)
{
}

void ClaimAccumulateProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

void ClaimAccumulateProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    NetworkStateFieldProcessor::processing_function(buffer);

    auto network_buffer = get_network_buffer();
    if (!network_buffer || !m_physics_op) {
        return;
    }

    uint32_t claim_events = 0;
    download_back_buffer(
        network_buffer->read_state_slot("mutation_claim_events"),
        &claim_events, sizeof(claim_events), m_readback_staging);

    if (claim_events == 0u) {
        m_physics_op->clear_bonds();
    } else {
        m_claimed_by_readback.resize(m_params.particle_count);
        download_back_buffer(
            network_buffer->read_state_slot("mutation_claimed_by"),
            m_claimed_by_readback.data(),
            m_claimed_by_readback.size() * sizeof(uint32_t),
            m_readback_staging);

        m_physics_op->sync_bonds_from_claims(
            std::span<const uint32_t>(m_claimed_by_readback).first(m_live_count));
    }

    auto accreted_mass = m_physics_op->get_accreted_mass_span();
    if (!accreted_mass.empty()) {
        if (m_live_count == m_params.particle_count) {
            upload_back_buffer(
                network_buffer->write_state_slot("mutation_accreted_mass"),
                accreted_mass.data(),
                accreted_mass.size() * sizeof(float),
                m_upload_staging);
        } else {
            m_accreted_mass_padded.assign(m_params.particle_count, 0.0F);
            std::copy_n(accreted_mass.begin(),
                std::min<size_t>(accreted_mass.size(), m_live_count),
                m_accreted_mass_padded.begin());
            upload_back_buffer(
                network_buffer->write_state_slot("mutation_accreted_mass"),
                m_accreted_mass_padded.data(),
                m_accreted_mass_padded.size() * sizeof(float),
                m_upload_staging);
        }
    }
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
        body += "    uint rb = root * stride_words;\n";
        body += "\n";
        body += "    float count = float(swallow_count[root]);\n";
        body += "    float size = clamp(base_size + count * growth_rate, base_size, max_size);\n";
        body += "    float t = clamp((size - base_size) / max(max_size - base_size, 0.001), 0.0, 1.0);\n";
        body += "    vec3 ember = vec3(0.03, 0.0, 0.06);\n";
        body += "    vec3 fire = vec3(0.95, 0.25, 0.02);\n";
        body += "    vec3 white_hot = vec3(1.0, 0.95, 0.7);\n";
        body += "    vec3 heat = t < 0.5\n";
        body += "        ? mix(ember, fire, t * 2.0)\n";
        body += "        : mix(fire, white_hot, (t - 0.5) * 2.0);\n";
        body += "\n";
        body += "    if (root == i) {\n";
        body += "        vertices[b + size_offset] = size;\n";
        body += "        vertices[b + color_offset] = heat.x;\n";
        body += "        vertices[b + color_offset + 1u] = heat.y;\n";
        body += "        vertices[b + color_offset + 2u] = heat.z;\n";
        body += "        return;\n";
        body += "    }\n";
        body += "\n";
        body += "    vec3 root_pos = vec3(vertices[rb + position_offset], "
                 "vertices[rb + position_offset + 1u], vertices[rb + position_offset + 2u]);\n";
        body += "    vertices[b + position_offset] = root_pos.x;\n";
        body += "    vertices[b + position_offset + 1u] = root_pos.y;\n";
        body += "    vertices[b + position_offset + 2u] = root_pos.z;\n";
        body += "\n";
        body += "    vec3 dim_col = heat * dim_factor;\n";
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

} // namespace

ClaimSwallowProcessor::ClaimSwallowProcessor(
    const MutationConfig& config,
    std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op)
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
        .base_size = particle_op->get_field_config().swallow_base_size,
        .growth_rate = particle_op->get_field_config().swallow_growth_rate,
        .max_size = particle_op->get_field_config().swallow_max_size,
        .dim_factor = particle_op->get_field_config().swallow_dim_factor,
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
        const auto& pconfig = m_particle_op->get_field_config();
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
