#include "SpatialHashProcessor.hpp"

#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Network/ParticleNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Buffers {

std::optional<SpatialHashConfig> SpatialHashConfig::from_network(
    const std::shared_ptr<NetworkGeometryBuffer>& buffer, float cell_size)
{
    if (cell_size <= 0.0F) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SpatialHashConfig::from_network: cell_size {} is not positive", cell_size);
        return std::nullopt;
    }

    auto particle_net = std::dynamic_pointer_cast<Nodes::Network::ParticleNetwork>(buffer->get_network());
    if (!particle_net) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SpatialHashConfig::from_network: buffer's network is not a ParticleNetwork");
        return std::nullopt;
    }

    auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(particle_net->get_operator());
    if (!graphics_op) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SpatialHashConfig::from_network: network's primary operator is not a "
            "GraphicsOperator, or none is set. create_operator<PhysicsOperator>() (or "
            "equivalent) must run before this.");
        return std::nullopt;
    }

    const auto layout = graphics_op->get_vertex_layout();

    if (layout.stride_bytes == 0 || layout.stride_bytes % 4 != 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SpatialHashConfig::from_network: stride {} is not a nonzero multiple of 4",
            layout.stride_bytes);
        return std::nullopt;
    }

    const auto position_offset = layout.find_word_offset(Kakshya::DataModality::VERTEX_POSITIONS_3D);
    if (!position_offset.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SpatialHashConfig::from_network: vertex layout carries no word-aligned "
            "position attribute");
        return std::nullopt;
    }

    const auto bounds = particle_net->get_bounds();
    const glm::vec3 extent = bounds.max - bounds.min;

    const glm::uvec3 dims {
        std::max(1U, static_cast<uint32_t>(std::ceil(extent.x / cell_size))),
        std::max(1U, static_cast<uint32_t>(std::ceil(extent.y / cell_size))),
        std::max(1U, static_cast<uint32_t>(std::ceil(extent.z / cell_size))),
    };

    return SpatialHashConfig {
        .grid_min = bounds.min,
        .grid_dims = dims,
        .cell_size = cell_size,
        .particle_count = static_cast<uint32_t>(graphics_op->get_point_count()),
        .stride_words = layout.stride_bytes / 4,
        .position_word_offset = *position_offset,
    };
}

void SpatialHashConfig::declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const
{
    const uint32_t cells = cell_count();
    buffer->declare_state("hash_cell_count", cells, sizeof(uint32_t), false);
    buffer->declare_state("hash_cell_start", cells, sizeof(uint32_t), false);
    buffer->declare_state("hash_cell_cursor", cells, sizeof(uint32_t), false);
    buffer->declare_state("hash_particle_index", particle_count, sizeof(uint32_t), false);
}

//=============================================================================
// NetworkStateFieldProcessor
//=============================================================================

NetworkStateFieldProcessor::NetworkStateFieldProcessor(
    std::vector<FieldBinding> bindings, const Portal::Graphics::ShaderSpec& spec)
    : ComputeProcessor(spec)
    , m_bindings(std::move(bindings))
{
    register_bindings();
}

void NetworkStateFieldProcessor::register_bindings()
{
    for (const auto& entry : m_bindings) {
        m_config.bindings[entry.name]
            = ShaderBinding(0, entry.binding, vk::DescriptorType::eStorageBuffer);
    }
}

bool NetworkStateFieldProcessor::validate_fields()
{
    return std::ranges::all_of(m_bindings, [this](const FieldBinding& entry) {
        if (entry.field.empty()) {
            return true;
        }
        if (!m_buffer->has_state(entry.field)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "NetworkStateFieldProcessor: buffer has no state field '{}' for binding '{}'",
                entry.field, entry.name);
            return false;
        }
        return true;
    });
}

void NetworkStateFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    m_buffer = std::dynamic_pointer_cast<NetworkGeometryBuffer>(buffer);
    if (!m_buffer) {
        return;
    }

    if (!validate_fields()) {
        m_buffer.reset();
        return;
    }

    on_buffer_ready();
}

void NetworkStateFieldProcessor::write_field_descriptors()
{
    if (!m_buffer || m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    for (const auto& entry : m_bindings) {
        vk::Buffer handle {};
        size_t bytes = 0;

        if (entry.field.empty()) {
            handle = m_buffer->get_buffer();
            bytes = m_buffer->get_size_bytes();
        } else {
            handle = m_buffer->read_state_handle(entry.field);
            bytes = m_buffer->get_state_bytes(entry.field);
        }

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[0], entry.binding, vk::DescriptorType::eStorageBuffer,
            handle, 0, bytes);
    }
}

void NetworkStateFieldProcessor::on_descriptors_created()
{
    write_field_descriptors();
}

void NetworkStateFieldProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_buffer && are_descriptors_ready()) {
        write_field_descriptors();
    }

    ComputeProcessor::processing_function(buffer);
}

bool NetworkStateFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return m_buffer && std::dynamic_pointer_cast<NetworkGeometryBuffer>(buffer) != nullptr;
}

//=============================================================================
// Shared cell-index function, emitted identically into any spec that needs it
//=============================================================================

namespace {

    using Portal::Graphics::BindingDirection;
    using Portal::Graphics::KernelSource;
    using Portal::Graphics::ShaderSpec;

    /**
     * @brief Emit the shared "which cell does this position fall in" function.
     *
     * Takes grid_min/cell_size/dims as arguments rather than reading them from
     * push-constant-aliased locals: prelude functions are emitted above main()
     * and cannot see main()'s locals, only pc.<field> directly, which would
     * make this function shader-specific. Passing them as arguments keeps it
     * identical text in both HashCountProcessor and HashScatterProcessor.
     */
    void add_cell_of_function(ShaderSpec::Assemble& assemble)
    {
        std::string body;
        body += "    ivec3 c = ivec3(floor((p - grid_min) / cell_size));\n";
        body += "    ivec3 d = ivec3(dims) - ivec3(1);\n";
        body += "    c = clamp(c, ivec3(0), d);\n";
        body += "    return uint(c.x) + uint(c.y) * dims.x + uint(c.z) * dims.x * dims.y;\n";

        assemble.function("uint", "cell_of",
            "vec3 p, vec3 grid_min, float cell_size, uvec3 dims", std::move(body));
    }

} // namespace

//=============================================================================
// HashClearProcessor
//=============================================================================

namespace {

    ShaderSpec build_clear_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("cell_count", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("cell_count_total", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        std::string body;
        body += "    if (i < cell_count_total) {\n";
        body += "        cell_count[i] = 0u;\n";
        body += "    }\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "cell_count", "cell_count_total", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

HashClearProcessor::HashClearProcessor(const SpatialHashConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "cell_count", .binding = 0, .field = "hash_cell_count" } },
          build_clear_spec())
    , m_params { .cell_count_total = config.cell_count() }
{
}

void HashClearProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.cell_count_total, m_params);
}

//=============================================================================
// HashCountProcessor
//=============================================================================

namespace {

    ShaderSpec build_count_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_count", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("grid_min_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_z", Kakshya::GpuDataFormat::FLOAT32)
            .pc("cell_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_x", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_y", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_z", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        add_cell_of_function(assemble);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    uint b = i * stride_words;\n";
        body += "    vec3 p = vec3(vertices[b + position_offset], "
                 "vertices[b + position_offset + 1u], vertices[b + position_offset + 2u]);\n";
        body += "    vec3 gmin = vec3(grid_min_x, grid_min_y, grid_min_z);\n";
        body += "    uvec3 dims = uvec3(dim_x, dim_y, dim_z);\n";
        body += "    uint cell = cell_of(p, gmin, cell_size, dims);\n";
        body += "    atomicAdd(cell_count[cell], 1u);\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "vertices", "cell_count", "particle_count", "stride_words",
                "position_offset", "grid_min_x", "grid_min_y", "grid_min_z", "cell_size",
                "dim_x", "dim_y", "dim_z", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

HashCountProcessor::HashCountProcessor(const SpatialHashConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "cell_count", .binding = 1, .field = "hash_cell_count" } },
          build_count_spec())
    , m_params {
        .particle_count = config.particle_count,
        .stride_words = config.stride_words,
        .position_offset = config.position_word_offset,
        .grid_min_x = config.grid_min.x,
        .grid_min_y = config.grid_min.y,
        .grid_min_z = config.grid_min.z,
        .cell_size = config.cell_size,
        .dim_x = config.grid_dims.x,
        .dim_y = config.grid_dims.y,
        .dim_z = config.grid_dims.z,
    }
{
}

void HashCountProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

//=============================================================================
// HashScanProcessor
//=============================================================================

namespace {

    ShaderSpec build_scan_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("cell_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_start", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_cursor", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("cell_count_total", Kakshya::GpuDataFormat::UINT32)
            .workgroup(1);

        std::string body;
        body += "    if (i == 0u) {\n";
        body += "        uint running = 0u;\n";
        body += "        for (uint c = 0u; c < cell_count_total; c = c + 1u) {\n";
        body += "            cell_start[c] = running;\n";
        body += "            cell_cursor[c] = running;\n";
        body += "            running = running + cell_count[c];\n";
        body += "        }\n";
        body += "    }\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "cell_count", "cell_start", "cell_cursor", "cell_count_total", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

HashScanProcessor::HashScanProcessor(const SpatialHashConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "cell_count", .binding = 0, .field = "hash_cell_count" },
              FieldBinding { .name = "cell_start", .binding = 1, .field = "hash_cell_start" },
              FieldBinding { .name = "cell_cursor", .binding = 2, .field = "hash_cell_cursor" } },
          build_scan_spec())
    , m_params { .cell_count_total = config.cell_count() }
{
}

void HashScanProcessor::on_buffer_ready()
{
    set_manual_dispatch(1, 1, 1);
    set_push_constant_data(m_params);
}

//=============================================================================
// HashScatterProcessor
//=============================================================================

namespace {

    ShaderSpec build_scatter_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_cursor", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .ssbo("particle_index", BindingDirection::Output, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("grid_min_x", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_y", Kakshya::GpuDataFormat::FLOAT32)
            .pc("grid_min_z", Kakshya::GpuDataFormat::FLOAT32)
            .pc("cell_size", Kakshya::GpuDataFormat::FLOAT32)
            .pc("dim_x", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_y", Kakshya::GpuDataFormat::UINT32)
            .pc("dim_z", Kakshya::GpuDataFormat::UINT32)
            .workgroup(256);

        add_cell_of_function(assemble);

        std::string body;
        body += "    if (i >= particle_count) { return; }\n";
        body += "    uint b = i * stride_words;\n";
        body += "    vec3 p = vec3(vertices[b + position_offset], "
                 "vertices[b + position_offset + 1u], vertices[b + position_offset + 2u]);\n";
        body += "    vec3 gmin = vec3(grid_min_x, grid_min_y, grid_min_z);\n";
        body += "    uvec3 dims = uvec3(dim_x, dim_y, dim_z);\n";
        body += "    uint cell = cell_of(p, gmin, cell_size, dims);\n";
        body += "    uint slot = atomicAdd(cell_cursor[cell], 1u);\n";
        body += "    particle_index[slot] = i;\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "vertices", "cell_cursor", "particle_index", "particle_count",
                "stride_words", "position_offset", "grid_min_x", "grid_min_y", "grid_min_z",
                "cell_size", "dim_x", "dim_y", "dim_z", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

HashScatterProcessor::HashScatterProcessor(const SpatialHashConfig& config)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "cell_cursor", .binding = 1, .field = "hash_cell_cursor" },
              FieldBinding { .name = "particle_index", .binding = 2, .field = "hash_particle_index" } },
          build_scatter_spec())
    , m_params {
        .particle_count = config.particle_count,
        .stride_words = config.stride_words,
        .position_offset = config.position_word_offset,
        .grid_min_x = config.grid_min.x,
        .grid_min_y = config.grid_min.y,
        .grid_min_z = config.grid_min.z,
        .cell_size = config.cell_size,
        .dim_x = config.grid_dims.x,
        .dim_y = config.grid_dims.y,
        .dim_z = config.grid_dims.z,
    }
{
}

void HashScatterProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

//=============================================================================
// HashDensityColorProcessor
//=============================================================================

namespace {

    ShaderSpec build_density_spec()
    {
        ShaderSpec::Assemble assemble;
        assemble
            .ssbo("vertices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("cell_start", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("cell_count", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .ssbo("particle_index", BindingDirection::Input, Kakshya::GpuDataFormat::UINT32)
            .pc("particle_count", Kakshya::GpuDataFormat::UINT32)
            .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
            .pc("position_offset", Kakshya::GpuDataFormat::UINT32)
            .pc("color_offset", Kakshya::GpuDataFormat::UINT32)
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
        body += "    uint neighbor_count = 0u;\n";
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
        body += "                    if (j == i) { continue; }\n";
        body += "                    uint bj = j * stride_words;\n";
        body += "                    vec3 pj = vec3(vertices[bj + position_offset], "
                 "vertices[bj + position_offset + 1u], vertices[bj + position_offset + 2u]);\n";
        body += "                    if (length(pj - p) < cell_size) {\n";
        body += "                        neighbor_count = neighbor_count + 1u;\n";
        body += "                    }\n";
        body += "                }\n";
        body += "            }\n";
        body += "        }\n";
        body += "    }\n";
        body += "\n";
        body += "    float density = clamp(float(neighbor_count) / 24.0, 0.0, 1.0);\n";
        body += "    vec3 ember = vec3(0.03, 0.0, 0.06);\n";
        body += "    vec3 fire = vec3(0.95, 0.25, 0.02);\n";
        body += "    vec3 white_hot = vec3(1.0, 0.95, 0.7);\n";
        body += "    vec3 col = density < 0.5\n";
        body += "        ? mix(ember, fire, density * 2.0)\n";
        body += "        : mix(fire, white_hot, (density - 0.5) * 2.0);\n";
        body += "    vertices[b + color_offset] = col.x;\n";
        body += "    vertices[b + color_offset + 1u] = col.y;\n";
        body += "    vertices[b + color_offset + 2u] = col.z;\n";

        assemble.kernel(KernelSource {
            .raw = {},
            .param_names = { "vertices", "cell_start", "cell_count", "particle_index",
                "particle_count", "stride_words", "position_offset", "color_offset",
                "grid_min_x", "grid_min_y", "grid_min_z", "cell_size", "dim_x", "dim_y", "dim_z", "i" },
            .body = std::move(body),
        });

        return assemble.build();
    }

} // namespace

HashDensityColorProcessor::HashDensityColorProcessor(
    const SpatialHashConfig& config, uint32_t color_word_offset)
    : NetworkStateFieldProcessor(
          { FieldBinding { .name = "vertices", .binding = 0, .field = {} },
              FieldBinding { .name = "cell_start", .binding = 1, .field = "hash_cell_start" },
              FieldBinding { .name = "cell_count", .binding = 2, .field = "hash_cell_count" },
              FieldBinding { .name = "particle_index", .binding = 3, .field = "hash_particle_index" } },
          build_density_spec())
    , m_params {
        .particle_count = config.particle_count,
        .stride_words = config.stride_words,
        .position_offset = config.position_word_offset,
        .color_offset = color_word_offset,
        .grid_min_x = config.grid_min.x,
        .grid_min_y = config.grid_min.y,
        .grid_min_z = config.grid_min.z,
        .cell_size = config.cell_size,
        .dim_x = config.grid_dims.x,
        .dim_y = config.grid_dims.y,
        .dim_z = config.grid_dims.z,
    }
{
}

void HashDensityColorProcessor::on_buffer_ready()
{
    dispatch_one_thread_per(m_params.particle_count, m_params);
}

} // namespace MayaFlux::Buffers
