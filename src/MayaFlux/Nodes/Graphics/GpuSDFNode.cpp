#include "GpuSDFNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Kinesis/KMarchingTable.hpp"

namespace MayaFlux::Nodes::GpuSync {

// ============================================================================
// Construction
// ============================================================================

GpuSDFNode::GpuSDFNode(
    Kinesis::SpatialField field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float iso_level)
    : MeshWriterNode(static_cast<size_t>(res_x) * res_y * res_z * 15U)
    , m_field(std::move(field))
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
    , m_iso_level(iso_level)
    , m_dirty(true)
{
}

// ============================================================================
// Setters
// ============================================================================

void GpuSDFNode::set_field(Kinesis::SpatialField field)
{
    m_field = std::move(field);
    m_dirty = true;
}

void GpuSDFNode::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    m_bounds_min = bounds_min;
    m_bounds_max = bounds_max;
    m_dirty = true;
}

void GpuSDFNode::set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z)
{
    m_res_x = std::max(res_x, 1U);
    m_res_y = std::max(res_y, 1U);
    m_res_z = std::max(res_z, 1U);
    m_count_exec = nullptr;
    m_emit_exec = nullptr;
    m_count_op = nullptr;
    m_emit_op = nullptr;
    m_dirty = true;
}

void GpuSDFNode::set_iso_level(float iso_level)
{
    m_iso_level = iso_level;
    m_dirty = true;
}

// ============================================================================
// GpuSync interface
// ============================================================================

void GpuSDFNode::compute_frame()
{
    if (!m_dirty)
        return;
    if (!m_count_op) {
        std::vector<uint32_t> edge_flat(256);
        for (size_t i = 0; i < 256; ++i)
            edge_flat[i] = static_cast<uint32_t>(Kinesis::k_edge_table[i]);

        std::vector<int32_t> tri_flat(static_cast<long>(256) * 16);
        for (size_t i = 0; i < 256; ++i) {
            for (size_t j = 0; j < 16; ++j)
                tri_flat[i * 16 + j] = static_cast<int32_t>(Kinesis::k_tri_table[i][j]);
        }

        build_operations(edge_flat, tri_flat);
    }
    rebuild();
}

// ============================================================================
// Private
// ============================================================================

void GpuSDFNode::build_operations(
    const std::vector<uint32_t>& edge_flat,
    const std::vector<int32_t>& tri_flat)
{
    const size_t n_corners = corner_count();
    const size_t n_voxels = voxel_count();
    const size_t n_vertices = worst_case_vertices();

    const std::vector<float> grid_placeholder(n_corners, 0.0F);
    const std::vector<uint32_t> prefix_placeholder(n_voxels, 0U);

    m_count_exec = std::make_shared<McExecutor>(
        Yantra::GpuShaderConfig {
            .shader_path = "mc_count.comp",
            .workgroup_size = { 64, 1, 1 },
            .push_constant_size = sizeof(McPC) });

    m_count_exec
        ->input(grid_placeholder)
        .input(edge_flat, Yantra::GpuBufferBinding::ElementType::UINT32)
        .input(tri_flat, Yantra::GpuBufferBinding::ElementType::INT32)
        .output(n_voxels * sizeof(uint32_t), Yantra::GpuBufferBinding::ElementType::UINT32);

    m_count_op = std::make_shared<McTransformer>(m_count_exec);

    m_emit_exec = std::make_shared<McExecutor>(
        Yantra::GpuShaderConfig {
            .shader_path = "mc_emit.comp",
            .workgroup_size = { 64, 1, 1 },
            .push_constant_size = sizeof(McPC) });

    m_emit_exec
        ->input(grid_placeholder)
        .input(edge_flat, Yantra::GpuBufferBinding::ElementType::UINT32)
        .input(tri_flat, Yantra::GpuBufferBinding::ElementType::INT32)
        .input(prefix_placeholder, Yantra::GpuBufferBinding::ElementType::UINT32)
        .output(n_vertices * sizeof(float))
        .output(n_vertices * sizeof(uint32_t), Yantra::GpuBufferBinding::ElementType::UINT32);

    m_emit_op = std::make_shared<McTransformer>(m_emit_exec);
}

void GpuSDFNode::rebuild()
{
    using SEC = Yantra::ShaderExecutionContext<>;

    const glm::vec3 extent = m_bounds_max - m_bounds_min;
    const glm::vec3 step {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };

    const uint32_t nx = m_res_x + 1;
    const uint32_t ny = m_res_y + 1;
    const uint32_t nz = m_res_z + 1;

    std::vector<float> grid(corner_count());
    for (uint32_t iz = 0; iz < nz; ++iz) {
        const float z = m_bounds_min.z + static_cast<float>(iz) * step.z;
        for (uint32_t iy = 0; iy < ny; ++iy) {
            const float y = m_bounds_min.y + static_cast<float>(iy) * step.y;
            const size_t base = (static_cast<size_t>(iz) * ny + iy) * nx;
            for (uint32_t ix = 0; ix < nx; ++ix) {
                const float x = m_bounds_min.x + static_cast<float>(ix) * step.x;
                grid[base + ix] = m_field(glm::vec3(x, y, z));
            }
        }
    }

    const McPC pc {
        .bounds_min = m_bounds_min,
        .iso_level = m_iso_level,
        .step = step,
        .res_x = m_res_x,
        .res_y = m_res_y,
        .res_z = m_res_z,
    };

    m_count_exec->set_binding_data(0, grid);
    m_count_exec->set_push_constants(pc);

    Yantra::Datum<std::vector<Kakshya::DataVariant>> dummy;
    const auto count_result = m_count_op->apply_operation(dummy);
    const auto voxel_counts = SEC::read_output<uint32_t>(count_result, 3);
    const uint32_t total_tris = std::reduce(voxel_counts.begin(), voxel_counts.end(), 0U);

    if (total_tris == 0) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuSDFNode: no triangles at iso_level={}", m_iso_level);
        clear_mesh();
        m_dirty = false;
        return;
    }

    std::vector<uint32_t> prefix(voxel_counts.size());
    std::exclusive_scan(voxel_counts.begin(), voxel_counts.end(), prefix.begin(), 0U);

    m_emit_exec->set_binding_data(0, grid);
    m_emit_exec->set_binding_data(3, prefix);
    m_emit_exec->set_push_constants(pc);

    const auto emit_result = m_emit_op->apply_operation(dummy);
    const auto vertex_floats = SEC::read_output<float>(emit_result, 4);
    const auto index_data = SEC::read_output<uint32_t>(emit_result, 5);

    const uint32_t n_verts = total_tris * 3U;
    const size_t expected_floats = static_cast<size_t>(n_verts) * (sizeof(Kakshya::MeshVertex) / sizeof(float));

    if (vertex_floats.size() < expected_floats) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuSDFNode: vertex readback too small ({} floats, expected {})",
            vertex_floats.size(), expected_floats);
        m_dirty = false;
        return;
    }

    set_mesh(
        std::span { reinterpret_cast<const Kakshya::MeshVertex*>(vertex_floats.data()), n_verts },
        std::span { index_data.data(), n_verts });

    MeshWriterNode::compute_frame();
    m_dirty = false;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GpuSDFNode: {} triangles extracted", total_tris);
}

} // namespace MayaFlux::Nodes::GpuSync
