#include "PopulationProcessor.hpp"

#include "NeighbourWalkHelper.hpp"
#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers {

void PopulationConfig::declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const
{
    buffer->declare_state("mutation_alive", hash.particle_count, sizeof(uint32_t), false);
    buffer->declare_state("mutation_spawn_cursor", 1, sizeof(uint32_t), false);
}

//=============================================================================
// PopulationInitProcessor
//=============================================================================

namespace {

    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelSource;
    using Portal::Graphics::ShaderSpec;

    ShaderSpec build_population_init_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("alive", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .ssbo("spawn_cursor", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("live_count", Kakshya::GpuDataFormat::UINT32)
            .pc("total_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= total_count) { return; }\n";
        body += "    if (i < live_count) {\n";
        body += "        alive[i] = 1u;\n";
        body += "    } else {\n";
        body += "        alive[i] = 0u;\n";
        body += "        uint b = i * stride_words;\n";
        body += "        for (uint w = 0u; w < stride_words; w = w + 1u) {\n";
        body += "            vertices[b + w] = 0.0f;\n";
        body += "        }\n";
        body += "    }\n";
        body += "    if (i == 0u) {\n";
        body += "        spawn_cursor[0] = live_count;\n";
        body += "    }\n";

        assemble.kernel(KernelSource { .body = std::move(body) });

        return assemble.build();
    }

} // namespace

PopulationInitProcessor::PopulationInitProcessor(const PopulationConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "alive", .binding = 1, .field = "mutation_alive" },
              FieldBinding { .name = "spawn_cursor", .binding = 2, .field = "mutation_spawn_cursor" } },
          build_population_init_spec())
    , m_params {
        .live_count = config.live_count,
        .total_count = config.hash.particle_count,
        .stride_words = config.hash.stride_words,
    }
{
}

void PopulationInitProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.total_count, m_params);
}

bool PopulationInitProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!NetworkStateFieldProcessor::on_before_execute(cmd_id, buffer)) {
        return false;
    }

    if (m_done) {
        return false;
    }

    m_done = true;
    return true;
}

//=============================================================================
// PopulationSpawnProcessor
//=============================================================================

namespace {

    ShaderSpec build_population_spawn_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_start", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("particle_index", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("alive", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cluster_id", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .ssbo("spawn_cursor", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("total_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("grid_min_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_z", Kakshya::GpuDataFormat::FLOAT32)
            .pc("cell_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_x", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_y", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_z", Kakshya::GpuDataFormat::UINT32)
            .pc("spawn_density_threshold", Kakshya::GpuDataFormat::FLOAT32)
            .workgroup(256);

        std::string body;
        body += "    if (i >= total_count) { return; }\n";
        body += "    if (alive[i] == 0u) { return; }\n";
        body += "    uint b = i * stride_words;\n";
        body += "    vec3 p = vec3(vertices[b + position_offset], "
                "vertices[b + position_offset + 1u], vertices[b + position_offset + 2u]);\n";
        body += "\n";
        body += "    uint neighbor_count = 0u;\n";
        detail::append_neighbour_walk(body, {
            .on_hit = "neighbor_count = neighbor_count + 1u;",
        });
        body += "\n";
        body += "    if (float(neighbor_count) < spawn_density_threshold) { return; }\n";
        body += "\n";
        body += "    uint slot = atomicAdd(spawn_cursor[0], 1u);\n";
        body += "    if (slot >= total_count) { return; }\n";
        body += "    uint sb = slot * stride_words;\n";
        body += "    for (uint w = 0u; w < stride_words; w = w + 1u) {\n";
        body += "        vertices[sb + w] = vertices[b + w];\n";
        body += "    }\n";
        body += "    alive[slot] = 1u;\n";
        body += "    cluster_id[slot] = cluster_id[i];\n";

        assemble.kernel(KernelSource { .body = std::move(body) });

        return assemble.build();
    }

} // namespace

PopulationSpawnProcessor::PopulationSpawnProcessor(
    const PopulationConfig& config,
    std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "cell_start", .binding = 1, .field = "hash_cell_start" },
              FieldBinding { .name = "cell_count", .binding = 2, .field = "hash_cell_count" },
              FieldBinding { .name = "particle_index", .binding = 3, .field = "hash_particle_index" },
              FieldBinding { .name = "alive", .binding = 4, .field = "mutation_alive" },
              FieldBinding { .name = "cluster_id", .binding = 5, .field = "hash_cluster_id" },
              FieldBinding { .name = "spawn_cursor", .binding = 6, .field = "mutation_spawn_cursor" } },
          build_population_spawn_spec())
    , m_params {
        .grid = make_grid_push_constants(config.hash),
        .spawn_density_threshold = particle_op->get_field_config().spawn_density_threshold,
    }
    , m_particle_op(std::move(particle_op))
    , m_built_revision(m_particle_op->revision())
{
}

void PopulationSpawnProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.grid.particle_count, m_params);
}

bool PopulationSpawnProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return guard_and_resync(cmd_id, buffer, m_particle_op->revision(), m_built_revision, [this] {
        m_params.spawn_density_threshold = m_particle_op->get_field_config().spawn_density_threshold;
        set_push_constant_data(m_params);
    });
}

} // namespace MayaFlux::Buffers
