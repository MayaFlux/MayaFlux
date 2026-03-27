#include "MeshInsertion.hpp"
#include "MeshAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

// =============================================================================
// Construction
// =============================================================================

MeshInsertion::MeshInsertion(DataVariant& vertex_variant, DataVariant& index_variant)
    : m_vertex_variant(vertex_variant)
    , m_index_variant(index_variant)
{
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshInsertion::ensure_vertex_storage()
{
    if (!std::holds_alternative<std::vector<uint8_t>>(m_vertex_variant)) {
        m_vertex_variant = std::vector<uint8_t> {};
    }
}

void MeshInsertion::ensure_index_storage()
{
    if (!std::holds_alternative<std::vector<uint32_t>>(m_index_variant)) {
        m_index_variant = std::vector<uint32_t> {};
    }
}

bool MeshInsertion::validate_layout(const VertexLayout& incoming) const
{
    if (incoming.stride_bytes != m_layout.stride_bytes) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion: submesh stride {} does not match established stride {}; "
            "batch skipped",
            incoming.stride_bytes, m_layout.stride_bytes);
        return false;
    }
    if (incoming.attributes.size() != m_layout.attributes.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion: submesh attribute count {} does not match established count {}; "
            "batch skipped",
            incoming.attributes.size(), m_layout.attributes.size());
        return false;
    }
    return true;
}

// =============================================================================
// insert_flat
// =============================================================================

void MeshInsertion::insert_flat(
    std::span<const uint8_t> vertex_bytes,
    std::span<const uint32_t> index_data,
    const VertexLayout& layout)
{
    if (layout.stride_bytes == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_flat: layout stride_bytes is zero");
        return;
    }
    if (vertex_bytes.size() % layout.stride_bytes != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_flat: vertex_bytes {} not a multiple of stride {}",
            vertex_bytes.size(), layout.stride_bytes);
        return;
    }
    if (index_data.size() % 3 != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_flat: index count {} is not a multiple of 3",
            index_data.size());
        return;
    }

    m_vertex_variant = std::vector<uint8_t>(vertex_bytes.begin(), vertex_bytes.end());
    m_index_variant = std::vector<uint32_t>(index_data.begin(), index_data.end());

    m_layout = layout;
    m_vertex_count = static_cast<uint32_t>(vertex_bytes.size() / layout.stride_bytes);
    m_index_count = static_cast<uint32_t>(index_data.size());
    m_layout.vertex_count = m_vertex_count;
    m_layout_set = true;
    m_submeshes = std::nullopt;

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::Runtime,
        "MeshInsertion::insert_flat: {} vertices, {} indices ({} faces)",
        m_vertex_count, m_index_count, m_index_count / 3);
}

// =============================================================================
// insert_submesh
// =============================================================================

void MeshInsertion::insert_submesh(
    std::span<const uint8_t> vertex_bytes,
    std::span<const uint32_t> index_data,
    std::string_view name,
    std::string_view material_name,
    const VertexLayout& layout)
{
    if (layout.stride_bytes == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_submesh: layout stride_bytes is zero");
        return;
    }
    if (vertex_bytes.size() % layout.stride_bytes != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_submesh: vertex_bytes {} not a multiple of stride {}",
            vertex_bytes.size(), layout.stride_bytes);
        return;
    }
    if (index_data.size() % 3 != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::insert_submesh: index count {} is not a multiple of 3",
            index_data.size());
        return;
    }

    if (!m_layout_set) {
        m_layout = layout;
        m_layout_set = true;
    } else if (!validate_layout(layout)) {
        return;
    }

    ensure_vertex_storage();
    ensure_index_storage();

    auto& vbuf = std::get<std::vector<uint8_t>>(m_vertex_variant);
    auto& ibuf = std::get<std::vector<uint32_t>>(m_index_variant);

    const auto submesh_vcount = static_cast<uint32_t>(vertex_bytes.size() / layout.stride_bytes);
    const uint32_t index_start = m_index_count;

    vbuf.insert(vbuf.end(), vertex_bytes.begin(), vertex_bytes.end());

    ibuf.reserve(ibuf.size() + index_data.size());
    for (const uint32_t idx : index_data) {
        ibuf.push_back(idx + m_vertex_count);
    }

    MeshSubrange sub;
    sub.index_start = index_start;
    sub.index_count = static_cast<uint32_t>(index_data.size());
    sub.vertex_offset = m_vertex_count;
    sub.name = std::string(name);
    sub.material_name = std::string(material_name);

    if (!m_submeshes.has_value()) {
        m_submeshes = RegionGroup("submeshes");
    }
    m_submeshes->add_region(sub.to_region());

    m_vertex_count += submesh_vcount;
    m_index_count += static_cast<uint32_t>(index_data.size());
    m_layout.vertex_count = m_vertex_count;

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::Runtime,
        "MeshInsertion::insert_submesh: '{}' +{} vertices, +{} indices, "
        "running totals: {} vertices / {} indices",
        name.empty() ? "<unnamed>" : name,
        submesh_vcount, index_data.size(),
        m_vertex_count, m_index_count);
}

// =============================================================================
// clear
// =============================================================================

void MeshInsertion::clear()
{
    m_vertex_variant = std::vector<uint8_t> {};
    m_index_variant = std::vector<uint32_t> {};
    m_layout = {};
    m_layout_set = false;
    m_vertex_count = 0;
    m_index_count = 0;
    m_submeshes = std::nullopt;
}

// =============================================================================
// build
// =============================================================================

std::optional<MeshAccess> MeshInsertion::build() const
{
    if (!m_layout_set || m_vertex_count == 0 || m_index_count == 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshInsertion::build: no data inserted");
        return std::nullopt;
    }

    return as_mesh_access(
        m_vertex_variant,
        m_index_variant,
        m_layout,
        m_submeshes);
}

} // namespace MayaFlux::Kakshya
