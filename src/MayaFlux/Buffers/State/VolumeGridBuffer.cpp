#include "VolumeGridBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/SDFMeshProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/State/VolumeSurfaceProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

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

void VolumeGridBuffer::allocate_fields(const std::vector<FieldDecl>& decls)
{
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
    const uint32_t cells = get_cell_count();

    size_t total_bytes = 0;
    size_t largest_field = 0;

    for (const auto& decl : decls) {
        if (decl.stride_bytes == 0) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "VolumeGridBuffer: field '{}' declares zero stride, skipped", decl.name);
            continue;
        }

        if (m_fields.contains(decl.name)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "VolumeGridBuffer: duplicate field '{}', later declaration discarded", decl.name);
            continue;
        }

        const size_t field_bytes = static_cast<size_t>(cells) * decl.stride_bytes;
        const uint32_t slot_count = decl.double_buffered ? 2 : 1;

        Field field {
            .stride_bytes = decl.stride_bytes,
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

        field.slot_b = decl.double_buffered ? field.slot_a + 1 : field.slot_a;

        m_fields.emplace(decl.name, field);
        m_field_order.push_back(decl.name);

        total_bytes += field_bytes * slot_count;
        largest_field = std::max(largest_field, field_bytes);
    }

    if (m_fields.empty()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "VolumeGridBuffer constructed with no valid fields");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: {}x{}x{} lattice, {} fields, {} slots, {} bytes total",
        m_lattice.resolution.x, m_lattice.resolution.y, m_lattice.resolution.z,
        m_fields.size(), resources.back_buffers.size(), total_bytes);
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

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    upload_back_buffer(get_buffer_resources().back_buffers[slot], data, size, m_transfer_staging);
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
