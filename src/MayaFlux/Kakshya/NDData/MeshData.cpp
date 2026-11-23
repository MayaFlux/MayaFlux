#include "MeshData.hpp"

#include "MayaFlux/Kinesis/Polyhedral/Incidence.hpp"
#include "MayaFlux/Kinesis/Polyhedral/Triangulation.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

namespace MayaFlux::Kakshya {

bool MeshData::faces_are_valid() const noexcept
{
    if (!faces.has_value()) {
        return true;
    }
    return Kinesis::Incidence::create(faces->sizes, faces->corners, vertex_count()).has_value();
}

std::optional<MeshData> MeshData::extract_submesh(size_t index) const
{
    if (!submeshes.has_value() || index >= submeshes->regions.size()) {
        return std::nullopt;
    }

    const Region& region = submeshes->regions[index];
    const auto* sub = region.get<MeshSubrange>();
    const auto* vertices = std::get_if<std::vector<uint8_t>>(&vertex_variant);
    const auto* indices = std::get_if<std::vector<uint32_t>>(&index_variant);
    if (!sub || !vertices || !indices || layout.stride_bytes == 0
        || sub->vertex_count == 0 || sub->index_count == 0 || sub->index_count % 3 != 0) {
        return std::nullopt;
    }

    const uint64_t stride = layout.stride_bytes;
    const uint64_t vertex_end = static_cast<uint64_t>(sub->vertex_offset) + sub->vertex_count;
    const uint64_t index_end = static_cast<uint64_t>(sub->index_start) + sub->index_count;
    if (vertex_end * stride > vertices->size() || index_end > indices->size()) {
        return std::nullopt;
    }

    std::vector<uint32_t> local;
    local.reserve(sub->index_count);
    for (uint64_t k = sub->index_start; k < index_end; ++k) {
        const uint32_t vertex = (*indices)[k];
        if (vertex < sub->vertex_offset || vertex >= vertex_end) {
            return std::nullopt;
        }
        local.push_back(vertex - sub->vertex_offset);
    }

    MeshData out = MeshData::empty();
    out.layout = layout;
    out.layout.vertex_count = sub->vertex_count;
    out.vertex_variant = std::vector<uint8_t>(
        vertices->begin() + static_cast<std::ptrdiff_t>(sub->vertex_offset * stride),
        vertices->begin() + static_cast<std::ptrdiff_t>(vertex_end * stride));
    out.index_variant = std::move(local);

    MeshSubrange placed = *sub;
    placed.index_start = 0;
    placed.vertex_offset = 0;
    Region carried = region;
    carried.set(placed);
    carried.start_coordinates = { 0 };
    carried.end_coordinates = { static_cast<uint64_t>(placed.index_count) };

    RegionGroup group = *submeshes;
    group.clear_regions();
    group.add_region(carried);
    out.submeshes = std::move(group);
    return out;
}

std::optional<std::vector<uint32_t>> MeshData::assign_faces(FaceList polygons)
{
    return assign_faces(std::move(polygons), Kinesis::TriangulationOptions {});
}

std::optional<std::vector<uint32_t>> MeshData::assign_faces(
    FaceList polygons, const Kinesis::TriangulationOptions& options)
{
    const auto* vertices = std::get_if<std::vector<uint8_t>>(&vertex_variant);
    if (!vertices) {
        return std::nullopt;
    }

    const auto positions = Kinesis::read_vertex_positions(*vertices, layout);
    if (!positions) {
        return std::nullopt;
    }

    const auto incidence = Kinesis::Incidence::create(polygons.sizes, polygons.corners, positions->size());
    if (!incidence) {
        return std::nullopt;
    }

    auto triangulation = Kinesis::triangulate_faces(*positions, *incidence, options);
    if (!triangulation) {
        return std::nullopt;
    }

    index_variant = std::move(triangulation->indices);
    faces = std::move(polygons);
    submeshes.reset();
    return std::move(triangulation->failed_faces);
}

} // namespace MayaFlux::Kakshya
