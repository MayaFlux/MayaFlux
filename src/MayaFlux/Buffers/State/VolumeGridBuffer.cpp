#include "VolumeGridBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/SDFMeshProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "AdvectProcessor.hpp"
#include "BuoyancyProcessor.hpp"
#include "DiffuseProcessor.hpp"
#include "DivergenceProcessor.hpp"
#include "PressureProcessor.hpp"
#include "SolenoidalProcessor.hpp"
#include "VolumeSurfaceProcessor.hpp"
#include "WallProcessor.hpp"

namespace MayaFlux::Buffers {

VolumeGridBuffer::VolumeGridBuffer(
    Kinesis::Lattice3D lattice,
    std::initializer_list<FieldDecl> fields,
    std::optional<SurfaceConfig> surface)
    : VolumeGridBuffer(lattice, std::vector<FieldDecl>(fields), std::move(surface))
{
}

VolumeGridBuffer::VolumeGridBuffer(
    Kinesis::Lattice3D lattice,
    std::vector<FieldDecl> fields,
    std::optional<SurfaceConfig> surface)
    : VKBuffer(surface_storage_bytes(surface), Usage::VERTEX, Kakshya::DataModality::VERTICES_3D)
    , m_lattice(lattice)
    , m_surface(std::move(surface))
{
    force_internal_usage(true);
    allocate_fields(fields);
}

VolumeGridBuffer::VolumeGridBuffer(
    Kinesis::Lattice3D lattice,
    std::optional<SurfaceConfig> surface)
    : VKBuffer(surface_storage_bytes(surface), Usage::VERTEX, Kakshya::DataModality::VERTICES_3D)
    , m_lattice(lattice)
    , m_surface(std::move(surface))
{
    force_internal_usage(true);
}

VolumeGridBuffer::~VolumeGridBuffer()
{
    resolve_transfer(m_pending_transfer);
}

bool VolumeGridBuffer::allocate_field(
    const std::string& name, size_t stride_bytes, bool double_buffered)
{
    if (name.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "VolumeGridBuffer: field declared with empty name, skipped");
        return false;
    }

    if (stride_bytes == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "VolumeGridBuffer: field '{}' declares zero stride, skipped", name);
        return false;
    }

    if (m_fields.contains(name)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "VolumeGridBuffer: duplicate field '{}', later declaration discarded", name);
        return false;
    }

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service || !buffer_service->allocate_raw_buffer) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "VolumeGridBuffer requires a valid buffer service");
    }

    const auto usage_flags = static_cast<uint32_t>(
        static_cast<VkBufferUsageFlags>(
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst));

    const auto memory_flags = static_cast<uint32_t>(
        static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal));

    auto& resources = get_buffer_resources();
    const size_t field_bytes = static_cast<size_t>(get_cell_count()) * stride_bytes;
    const uint32_t slot_count = double_buffered ? 2 : 1;

    Field field {
        .stride_bytes = stride_bytes,
        .slot_a = static_cast<uint32_t>(resources.back_buffers.size()),
        .slot_b = 0,
        .read_is_a = true,
    };

    for (uint32_t i = 0; i < slot_count; ++i) {
        void* out_buffer = nullptr;
        void* out_memory = nullptr;
        void* out_mapped = nullptr;

        buffer_service->allocate_raw_buffer(
            field_bytes, usage_flags, memory_flags, false,
            out_buffer, out_memory, out_mapped);

        VKBufferResources::GenerationSlot slot;
        slot.buffer = static_cast<vk::Buffer>(static_cast<VkBuffer>(out_buffer));
        slot.memory = static_cast<vk::DeviceMemory>(static_cast<VkDeviceMemory>(out_memory));
        slot.mapped_ptr = out_mapped;

        resources.back_buffers.push_back(slot);
    }

    field.slot_b = double_buffered ? field.slot_a + 1 : field.slot_a;

    m_fields.emplace(name, field);
    m_field_order.push_back(name);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: field '{}', stride {}, {} slot(s), {} bytes",
        name, stride_bytes, slot_count, field_bytes * slot_count);

    return true;
}

void VolumeGridBuffer::allocate_fields(const std::vector<FieldDecl>& decls)
{
    for (const auto& decl : decls) {
        allocate_field(decl.name, decl.stride_bytes, decl.double_buffered);
    }

    if (m_fields.empty()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "VolumeGridBuffer constructed with no valid fields");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: {}x{}x{} lattice, {} fields, {} slots",
        m_lattice.resolution.x, m_lattice.resolution.y, m_lattice.resolution.z,
        m_fields.size(), get_buffer_resources().back_buffers.size());
}

ScalarRef VolumeGridBuffer::declare_scalar(std::string name)
{
    if (!allocate_field(name, sizeof(float), true)) {
        return {};
    }

    return ScalarRef {
        .name = std::move(name),
        .owner = std::dynamic_pointer_cast<VolumeGridBuffer>(shared_from_this()),
    };
}

ScalarRef VolumeGridBuffer::declare_scratch(std::string name)
{
    if (!allocate_field(name, sizeof(float), false)) {
        return {};
    }

    return ScalarRef {
        .name = std::move(name),
        .owner = std::dynamic_pointer_cast<VolumeGridBuffer>(shared_from_this()),
    };
}

VectorRef VolumeGridBuffer::declare_vector(std::string name)
{
    if (!allocate_field(name, sizeof(glm::vec4), true)) {
        return {};
    }

    return VectorRef {
        .name = std::move(name),
        .owner = std::dynamic_pointer_cast<VolumeGridBuffer>(shared_from_this()),
    };
}

void VolumeGridBuffer::setup_processors(ProcessingToken token)
{
    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    if (!m_surface) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
            "VolumeGridBuffer: chain established, no extraction configured");
        return;
    }

    auto self = std::dynamic_pointer_cast<VolumeGridBuffer>(shared_from_this());

    m_surface_processor = std::make_shared<VolumeSurfaceProcessor>(
        self, m_surface->field_name,
        m_lattice.resampled(m_surface->resolution),
        m_surface->threshold);

    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();

    m_counter_buf = std::make_shared<VKBuffer>(
        sizeof(uint32_t), Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_counter_buf);

    m_mesh_processor = std::make_shared<SDFMeshProcessor>(
        m_surface_processor->grid_buf(), m_counter_buf,
        m_lattice.bounds.min, m_lattice.bounds.max,
        m_surface->resolution.x, m_surface->resolution.y, m_surface->resolution.z, 0.0F);

    const uint32_t max_vertices = m_surface_processor->worst_case_vertices();
    auto layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    layout.vertex_count = max_vertices;
    set_vertex_layout(layout);

    m_surface_processor->set_processing_token(token);
    m_mesh_processor->set_processing_token(token);

    set_default_processor(m_surface_processor);
    chain->add_processor(m_mesh_processor, self);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: surfacing '{}' at {}x{}x{}, {} max vertices",
        m_surface->field_name, m_surface->resolution.x, m_surface->resolution.y,
        m_surface->resolution.z, max_vertices);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: chain established, no stages attached");
}

void VolumeGridBuffer::setup_rendering(const RenderConfig& config)
{
    if (!m_surface) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "setup_rendering: no SurfaceConfig was supplied at construction");
        return;
    }

    RenderConfig resolved = config;
    resolved.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;

    if (resolved.vertex_shader.empty())
        resolved.vertex_shader = "triangle.vert.spv";
    if (resolved.fragment_shader.empty())
        resolved.fragment_shader = "triangle.frag.spv";

    apply_render_config(resolved, ShaderConfig { resolved.vertex_shader });

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    m_render_processor->set_vertex_range(0, 0);
    set_needs_depth_attachment(true);
}

VolumeGridBuffer::FlowStages VolumeGridBuffer::setup_flow(const FlowConfig& config)
{
    FlowStages stages;

    auto self = std::dynamic_pointer_cast<VolumeGridBuffer>(shared_from_this());
    auto chain = get_processing_chain();

    if (!chain) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "setup_flow: no processing chain, call after registration");
        return stages;
    }

    const auto require = [this](const std::string& name, const char* role) {
        if (name.empty()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "setup_flow: no field supplied for '{}'", role);
            return false;
        }
        if (!has_field(name)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "setup_flow: '{}' names no field on this volume, supplied as '{}'",
                name, role);
            return false;
        }
        return true;
    };

    if (!require(config.velocity, "velocity")
        || !require(config.divergence, "divergence")
        || !require(config.pressure, "pressure")) {
        return stages;
    }

    if (config.viscosity > 0.0F && !require(config.scratch, "scratch")) {
        return stages;
    }

    stages.self_advect = std::make_shared<AdvectProcessor>(
        config.velocity, config.velocity, "volume_advect_vector.comp.spv");
    stages.self_advect->set_time_step(config.time_step);
    chain->add_processor(stages.self_advect, self);

    if (config.buoyancy) {
        const auto& b = *config.buoyancy;

        if (!require(b.temperature, "buoyancy.temperature")
            || !require(b.density, "buoyancy.density")) {
            return stages;
        }

        stages.buoyancy = std::make_shared<BuoyancyProcessor>(
            b.temperature, b.density, config.velocity,
            b.direction, "volume_buoyancy.comp.spv");
        stages.buoyancy->set_time_step(config.time_step);
        stages.buoyancy->set_temperature_gain(b.temperature_gain);
        stages.buoyancy->set_density_gain(b.density_gain);
        stages.buoyancy->set_ambient(b.ambient);
        chain->add_processor(stages.buoyancy, self);
    }

    if (config.viscosity > 0.0F) {
        stages.diffuse = std::make_shared<DiffuseProcessor>(
            config.velocity, config.scratch, "volume_diffuse_vector.comp.spv");
        stages.diffuse->set_rate(config.viscosity);
        stages.diffuse->set_time_step(config.time_step);
        chain->add_processor(stages.diffuse, self);
    }

    if (config.walls) {
        stages.wall_advected = std::make_shared<WallProcessor>(
            config.velocity, "volume_wall.comp.spv");
        chain->add_processor(stages.wall_advected, self);
    }

    stages.divergence = std::make_shared<DivergenceProcessor>(
        config.velocity, config.divergence, "volume_divergence.comp.spv");
    chain->add_processor(stages.divergence, self);

    stages.pressure = std::make_shared<PressureProcessor>(
        config.divergence, config.pressure, "volume_pressure_jacobi.comp.spv");
    stages.pressure->set_iteration_count(config.jacobi_iterations);
    chain->add_processor(stages.pressure, self);

    stages.solenoidal = std::make_shared<SolenoidalProcessor>(
        config.pressure, config.velocity, "volume_solenoidal.comp.spv");
    chain->add_processor(stages.solenoidal, self);

    if (config.walls) {
        stages.wall_projected = std::make_shared<WallProcessor>(
            config.velocity, "volume_wall.comp.spv");
        chain->add_processor(stages.wall_projected, self);
    }

    for (const auto& carried : config.carried) {
        if (!require(carried.field, "carried")) {
            continue;
        }

        auto advect = std::make_shared<AdvectProcessor>(
            config.velocity, carried.field, "volume_advect_scalar.comp.spv");
        advect->set_time_step(config.time_step);
        advect->set_dissipation(carried.dissipation);
        chain->add_processor(advect, self);

        stages.carriers.push_back(std::move(advect));
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer::setup_flow: {} carried, buoyancy {}, viscosity {}, walls {}",
        stages.carriers.size(), config.buoyancy ? "on" : "off",
        config.viscosity, config.walls ? "on" : "off");

    return stages;
}

const VolumeGridBuffer::Field* VolumeGridBuffer::find_field(
    const std::string& name, const char* context) const
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::{}: no field named '{}'", context, name);
        return nullptr;
    }
    return &it->second;
}

bool VolumeGridBuffer::has_field(const std::string& name) const
{
    return m_fields.contains(name);
}

size_t VolumeGridBuffer::get_field_bytes(const std::string& name) const
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        return 0;
    }
    return static_cast<size_t>(get_cell_count()) * it->second.stride_bytes;
}

std::vector<std::string> VolumeGridBuffer::get_field_names() const
{
    return m_field_order;
}

vk::Buffer VolumeGridBuffer::read_handle(const std::string& name) const
{
    const auto* field = find_field(name, "read_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot].buffer;
}

vk::Buffer VolumeGridBuffer::write_handle(const std::string& name) const
{
    const auto* field = find_field(name, "write_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot].buffer;
}

void VolumeGridBuffer::swap_field(const std::string& name)
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::swap_field: no field named '{}'", name);
        return;
    }

    if (it->second.slot_a == it->second.slot_b) {
        return;
    }

    it->second.read_is_a = !it->second.read_is_a;
}

void VolumeGridBuffer::seed(const std::string& name, const Kinesis::SpatialField& field)
{
    const auto* decl = find_field(name, "seed");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(float)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed: field '{}' has stride {}, SpatialField requires {}",
            name, decl->stride_bytes, sizeof(float));
        return;
    }

    const glm::uvec3 res = m_lattice.resolution;
    std::vector<float> values(m_lattice.cell_count());

    size_t i = 0;
    for (uint32_t z = 0; z < res.z; ++z) {
        for (uint32_t y = 0; y < res.y; ++y) {
            for (uint32_t x = 0; x < res.x; ++x) {
                values[i++] = field(m_lattice.cell_center({ x, y, z }));
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(float));
}

void VolumeGridBuffer::seed(const std::string& name, const Kinesis::VectorField& field)
{
    const auto* decl = find_field(name, "seed");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(glm::vec4)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed: field '{}' has stride {}, VectorField requires {}",
            name, decl->stride_bytes, sizeof(glm::vec4));
        return;
    }

    const glm::uvec3 res = m_lattice.resolution;
    std::vector<glm::vec4> values(m_lattice.cell_count());

    size_t i = 0;
    for (uint32_t z = 0; z < res.z; ++z) {
        for (uint32_t y = 0; y < res.y; ++y) {
            for (uint32_t x = 0; x < res.x; ++x) {
                values[i++] = glm::vec4(field(m_lattice.cell_center({ x, y, z })), 0.0F);
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(glm::vec4));
}

void VolumeGridBuffer::seed_raw(const std::string& name, const void* data, size_t size)
{
    const auto* field = find_field(name, "seed_raw");
    if (!field) {
        return;
    }

    const size_t expected = static_cast<size_t>(get_cell_count()) * field->stride_bytes;
    if (size != expected) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed_raw: size {} does not match expected {} for '{}'",
            size, expected, name);
        return;
    }

    resolve_transfer(m_pending_transfer);

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    m_pending_transfer = upload_back_buffer_async(
        get_buffer_resources().back_buffers[slot], data, size, m_transfer_staging);
}

void VolumeGridBuffer::accumulate(const std::string& name, const Kinesis::SpatialField& field)
{
    const auto* decl = find_field(name, "accumulate");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(float)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::accumulate: field '{}' has stride {}, SpatialField requires {}",
            name, decl->stride_bytes, sizeof(float));
        return;
    }

    const glm::uvec3 res = m_lattice.resolution;
    std::vector<float> values(m_lattice.cell_count());

    read_field(name, values.data(), values.size() * sizeof(float));

    size_t i = 0;
    for (uint32_t z = 0; z < res.z; ++z) {
        for (uint32_t y = 0; y < res.y; ++y) {
            for (uint32_t x = 0; x < res.x; ++x) {
                values[i++] += field(m_lattice.cell_center({ x, y, z }));
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(float));
}

void VolumeGridBuffer::accumulate(const std::string& name, const Kinesis::VectorField& field)
{
    const auto* decl = find_field(name, "accumulate");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(glm::vec4)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::accumulate: field '{}' has stride {}, VectorField requires {}",
            name, decl->stride_bytes, sizeof(glm::vec4));
        return;
    }

    const glm::uvec3 res = m_lattice.resolution;
    std::vector<glm::vec4> values(m_lattice.cell_count());

    read_field(name, values.data(), values.size() * sizeof(glm::vec4));

    size_t i = 0;
    for (uint32_t z = 0; z < res.z; ++z) {
        for (uint32_t y = 0; y < res.y; ++y) {
            for (uint32_t x = 0; x < res.x; ++x) {
                const glm::vec3 v = field(m_lattice.cell_center({ x, y, z }));
                values[i].x += v.x;
                values[i].y += v.y;
                values[i].z += v.z;
                ++i;
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(glm::vec4));
}

void VolumeGridBuffer::read_field(const std::string& name, void* data, size_t size)
{
    const auto* field = find_field(name, "read_field");
    if (!field) {
        return;
    }

    const size_t expected = static_cast<size_t>(get_cell_count()) * field->stride_bytes;
    if (size != expected) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::read_field: size {} does not match expected {} for '{}'",
            size, expected, name);
        return;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    resolve_transfer(m_pending_transfer);
    download_back_buffer(get_buffer_resources().back_buffers[slot], data, size, m_transfer_staging);
}

size_t VolumeGridBuffer::surface_storage_bytes(const std::optional<SurfaceConfig>& surface)
{
    if (!surface) {
        return 1;
    }

    const glm::uvec3 res = glm::max(surface->resolution, glm::uvec3(1U));
    const uint64_t voxels = static_cast<uint64_t>(res.x) * res.y * res.z;
    return static_cast<size_t>(voxels * 15U * sizeof(Kakshya::MeshVertex));
}

} // namespace MayaFlux::Buffers
