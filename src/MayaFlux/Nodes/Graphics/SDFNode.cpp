#include "SDFNode.hpp"

#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

SDFNode::SDFNode(
    Kinesis::SpatialField field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float iso_level)
    : MeshWriterNode(static_cast<size_t>(res_x * res_y * res_z) * 3)
    , m_field(std::move(field))
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
    , m_iso_level(iso_level)
{
    rebuild();
}

void SDFNode::set_field(Kinesis::SpatialField field)
{
    m_field = std::move(field);
    m_dirty = true;
}

void SDFNode::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    m_bounds_min = bounds_min;
    m_bounds_max = bounds_max;
    m_dirty = true;
}

void SDFNode::set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z)
{
    m_res_x = std::max(res_x, 1U);
    m_res_y = std::max(res_y, 1U);
    m_res_z = std::max(res_z, 1U);
    m_dirty = true;
}

void SDFNode::set_iso_level(float iso_level)
{
    m_iso_level = iso_level;
    m_dirty = true;
}

void SDFNode::compute_frame()
{
    if (!m_dirty)
        return;
    rebuild();
}

void SDFNode::rebuild()
{
    auto data = Kinesis::generate_sdf_mesh(
        m_field, m_bounds_min, m_bounds_max,
        m_res_x, m_res_y, m_res_z, m_iso_level);

    if (!data.is_valid()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "SDFNode: no isosurface crossings at iso_level={}", m_iso_level);
        clear_mesh();
        m_dirty = false;
        return;
    }

    set_mesh(data);
    MeshWriterNode::compute_frame();
    m_dirty = false;
}

} // namespace MayaFlux::Nodes::GpuSync
