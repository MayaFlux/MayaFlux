#include "GpuSDFNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Kinesis/KMarchingTable.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

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
    m_emit_exec = nullptr;
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

    if (!m_emit_op) {
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
    const size_t n_vertices = worst_case_vertices();

    const std::vector<float> grid_placeholder(n_corners, 0.0F);
    const std::vector<uint32_t> counter_placeholder(1, 0U);

    m_emit_exec = std::make_shared<McExecutor>(
        Yantra::GpuShaderConfig {
            .shader_path = "mc_emit.comp",
            .workgroup_size = { 64, 1, 1 },
            .push_constant_size = sizeof(McPC) });

    m_emit_exec
        ->input(grid_placeholder)
        .input(edge_flat, Yantra::GpuBufferBinding::ElementType::UINT32)
        .input(tri_flat, Yantra::GpuBufferBinding::ElementType::INT32)
        .output(n_vertices * sizeof(float))
        .output(sizeof(uint32_t), Yantra::GpuBufferBinding::ElementType::UINT32);

    m_emit_exec->set_skip_readback(3, true);
    m_emit_exec->set_output_size(4, sizeof(uint32_t));

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
    const size_t total = static_cast<size_t>(nx) * ny * nz;

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

    const McPC pc {
        .bounds_min = m_bounds_min,
        .iso_level = m_iso_level,
        .step = step,
        .res_x = m_res_x,
        .res_y = m_res_y,
        .res_z = m_res_z,
    };

    const std::vector<uint32_t> zero { 0U };
    m_emit_exec->set_binding_data(0, grid);
    m_emit_exec->set_binding_data(4, zero);
    m_emit_exec->set_push_constants(pc);

    Yantra::Datum<std::vector<Kakshya::DataVariant>> input;
    input.data.emplace_back(std::vector<float> { 0.0F });
    const auto emit_result = m_emit_op->apply_operation(input);

    const auto counter = SEC::read_output<uint32_t>(emit_result, 4);
    if (counter.empty() || counter[0] == 0) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuSDFNode: no vertices at iso_level={}", m_iso_level);
        clear_mesh();
        m_dirty = false;
        return;
    }

    const uint32_t n_verts = counter[0];
    const size_t vertex_bytes = static_cast<size_t>(n_verts) * sizeof(Kakshya::MeshVertex);
    std::vector<Kakshya::MeshVertex> verts(n_verts);
    m_emit_exec->download_binding(3, verts.data(), vertex_bytes);

    std::vector<uint32_t> indices(n_verts);
    std::iota(indices.begin(), indices.end(), 0U);
    set_mesh(std::span { verts.data(), n_verts }, indices);

    MeshWriterNode::compute_frame();
    m_dirty = false;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GpuSDFNode: {} vertices extracted", n_verts);
}

} // namespace MayaFlux::Nodes::GpuSync
