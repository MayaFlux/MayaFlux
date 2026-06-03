#include "SDFMeshProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Kinesis/KMarchingTable.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// ============================================================================
// Construction
// ============================================================================

SDFMeshProcessor::SDFMeshProcessor(
    Kinesis::SpatialField field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float iso_level)
    : ComputeProcessor("mc_emit.comp.spv", 64)
    , m_field(std::move(field))
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
    , m_iso_level(iso_level)
{
    m_config.push_constant_size = sizeof(McPC);

    m_config.bindings["sdf_grid"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["edge_table"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["tri_table"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["vertices"] = ShaderBinding(0, 3, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["counter"] = ShaderBinding(0, 4, vk::DescriptorType::eStorageBuffer);

    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
}

// ============================================================================
// Setters
// ============================================================================

void SDFMeshProcessor::set_field(Kinesis::SpatialField field)
{
    m_field = std::move(field);
    m_dirty = true;
}

void SDFMeshProcessor::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    m_bounds_min = bounds_min;
    m_bounds_max = bounds_max;
    m_dirty = true;
}

void SDFMeshProcessor::set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z)
{
    m_res_x = std::max(res_x, 1U);
    m_res_y = std::max(res_y, 1U);
    m_res_z = std::max(res_z, 1U);
    rebuild_owned_buffers();
    m_needs_descriptor_rebuild = true;
    m_dirty = true;
}

void SDFMeshProcessor::set_iso_level(float iso_level)
{
    m_iso_level = iso_level;
    m_dirty = true;
}

// ============================================================================
// ShaderProcessor hooks
// ============================================================================

void SDFMeshProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SDFMeshProcessor requires a VKBuffer");
        return;
    }

    rebuild_owned_buffers();

    bind_buffer("sdf_grid", m_grid_buf);
    bind_buffer("edge_table", m_edge_buf);
    bind_buffer("tri_table", m_tri_buf);
    bind_buffer("vertices", vk_buf);
    bind_buffer("counter", m_counter_buf);
}

void SDFMeshProcessor::on_descriptors_created()
{
    std::vector<uint32_t> edge_flat(256);
    for (size_t i = 0; i < 256; ++i)
        edge_flat[i] = static_cast<uint32_t>(Kinesis::k_edge_table[i]);

    std::vector<int32_t> tri_flat(static_cast<long>(256) * 16);
    for (size_t i = 0; i < 256; ++i) {
        for (size_t j = 0; j < 16; ++j)
            tri_flat[i * 16 + j] = static_cast<int32_t>(Kinesis::k_tri_table[i][j]);
    }

    std::memcpy(m_edge_buf->get_mapped_ptr(), edge_flat.data(),
        edge_flat.size() * sizeof(uint32_t));
    std::memcpy(m_tri_buf->get_mapped_ptr(), tri_flat.data(),
        tri_flat.size() * sizeof(int32_t));
}

bool SDFMeshProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    if (!m_dirty)
        return false;

    evaluate_grid();

    std::memset(m_counter_buf->get_mapped_ptr(), 0, sizeof(uint32_t));

    const glm::vec3 extent = m_bounds_max - m_bounds_min;
    const McPC pc {
        .bounds_min = m_bounds_min,
        .iso_level = m_iso_level,
        .step = {
            extent.x / static_cast<float>(m_res_x),
            extent.y / static_cast<float>(m_res_y),
            extent.z / static_cast<float>(m_res_z),
        },
        .res_x = m_res_x,
        .res_y = m_res_y,
        .res_z = m_res_z,
    };
    set_push_constant_data(pc);

    const uint32_t groups = (voxel_count() + 63U) / 64U;
    set_manual_dispatch(groups, 1, 1);

    return true;
}

void SDFMeshProcessor::on_after_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    const auto n = *static_cast<const uint32_t*>(m_counter_buf->get_mapped_ptr());

    if (auto rp = buffer->get_render_processor()) {
        buffer->get_render_processor()->set_vertex_range(0, n);
    } else {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SDFMeshProcessor: no render processor attached to buffer, cannot set vertex count");
        return;
    }

    m_dirty = false;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "SDFMeshProcessor: {} vertices extracted", n);
}

// ============================================================================
// Private
// ============================================================================

void SDFMeshProcessor::rebuild_owned_buffers()
{
    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();

    m_grid_buf = std::make_shared<VKBuffer>(
        corner_count() * sizeof(float),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_grid_buf);

    m_edge_buf = std::make_shared<VKBuffer>(
        256 * sizeof(uint32_t),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_edge_buf);

    m_tri_buf = std::make_shared<VKBuffer>(
        static_cast<long>(256) * 16 * sizeof(int32_t),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_tri_buf);

    m_counter_buf = std::make_shared<VKBuffer>(
        sizeof(uint32_t),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_counter_buf);
}

void SDFMeshProcessor::evaluate_grid()
{
    const uint32_t nx = m_res_x + 1;
    const uint32_t ny = m_res_y + 1;
    const uint32_t nz = m_res_z + 1;
    const size_t total = static_cast<size_t>(nx) * ny * nz;

    const glm::vec3 extent = m_bounds_max - m_bounds_min;
    const glm::vec3 step {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };

    auto* grid = static_cast<float*>(m_grid_buf->get_mapped_ptr());

    MayaFlux::Parallel::for_each(std::execution::par_unseq,
        std::views::iota(0UZ, total).begin(),
        std::views::iota(0UZ, total).end(),
        [&](size_t idx) {
            const uint32_t gix = idx % nx;
            const uint32_t giy = (idx / nx) % ny;
            const uint32_t giz = idx / (static_cast<size_t>(nx) * ny);
            grid[idx] = m_field(glm::vec3(
                m_bounds_min.x + static_cast<float>(gix) * step.x,
                m_bounds_min.y + static_cast<float>(giy) * step.y,
                m_bounds_min.z + static_cast<float>(giz) * step.z));
        });
}

} // namespace MayaFlux::Buffers
