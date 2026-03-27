#include "MeshAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

// =============================================================================
// MeshSubrange
// =============================================================================

Region MeshSubrange::to_region() const
{
    Region r(
        std::vector { static_cast<uint64_t>(index_start) },
        std::vector { static_cast<uint64_t>(index_start + index_count) });
    r.set_attribute("name", name);
    r.set_attribute("material_name", material_name);
    r.set_attribute("vertex_offset", static_cast<uint64_t>(vertex_offset));
    r.set_attribute("index_count", static_cast<uint64_t>(index_count));
    r.set_attribute("diffuse_path", diffuse_path);
    r.set_attribute("diffuse_embedded", diffuse_embedded);
    return r;
}

// =============================================================================
// as_mesh_access
// =============================================================================

std::optional<MeshAccess> as_mesh_access(
    const DataVariant& vertex_variant,
    const DataVariant& index_variant,
    const VertexLayout& layout,
    std::optional<RegionGroup> submeshes)
{
    const auto* vbytes = std::get_if<std::vector<uint8_t>>(&vertex_variant);
    if (!vbytes || vbytes->empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_access: vertex_variant must hold a non-empty vector<uint8_t>");
        return std::nullopt;
    }

    const auto* ibuf = std::get_if<std::vector<uint32_t>>(&index_variant);
    if (!ibuf || ibuf->empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_access: index_variant must hold a non-empty vector<uint32_t>");
        return std::nullopt;
    }

    if (layout.stride_bytes == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_access: layout stride_bytes is zero");
        return std::nullopt;
    }

    if (layout.attributes.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_access: layout has no attributes");
        return std::nullopt;
    }

    if (vbytes->size() % layout.stride_bytes != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_access: vertex byte count {} is not a multiple of stride {}",
            vbytes->size(), layout.stride_bytes);
        return std::nullopt;
    }

    MeshAccess ma;
    ma.vertex_ptr = vbytes->data();
    ma.vertex_bytes = vbytes->size();
    ma.index_ptr = ibuf->data();
    ma.index_count = static_cast<uint32_t>(ibuf->size());
    ma.layout = layout;
    ma.layout.vertex_count = static_cast<uint32_t>(vbytes->size() / layout.stride_bytes);
    ma.submeshes = std::move(submeshes);
    return ma;
}

} // namespace MayaFlux::Kakshya
