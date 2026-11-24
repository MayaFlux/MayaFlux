#include "MeshData.hpp"

#include "MeshInsertion.hpp"

#include "MayaFlux/Kinesis/Polyhedral/Incidence.hpp"
#include "MayaFlux/Kinesis/Polyhedral/Triangulation.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    constexpr uint32_t k_canonical_stride = 68;

    /**
     * Records the caller owns, or ones built here, and the layout that describes them.
     */
    struct Records {
        std::vector<uint8_t> built;
        std::span<const uint8_t> borrowed;
        VertexLayout layout;

        [[nodiscard]] std::span<const uint8_t> bytes() const noexcept
        {
            return built.empty() ? borrowed : std::span<const uint8_t>(built);
        }
    };

    /**
     * Triangle indices the caller owns, or ones widened here.
     */
    struct Triangles {
        std::vector<uint32_t> widened;
        std::span<const uint32_t> borrowed;

        [[nodiscard]] std::span<const uint32_t> indices() const noexcept
        {
            return widened.empty() ? borrowed : std::span<const uint32_t>(widened);
        }
    };

    std::vector<uint8_t> position_records(std::span<const glm::vec3> positions)
    {
        std::vector<MeshVertex> vertices(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            vertices[i].position = positions[i];
        }
        std::vector<uint8_t> bytes(vertices.size() * sizeof(MeshVertex));
        std::memcpy(bytes.data(), vertices.data(), bytes.size());
        return bytes;
    }

    template <typename T>
    std::vector<uint8_t> flat_position_records(const std::vector<T>& flat)
    {
        std::vector<glm::vec3> positions(flat.size() / 3);
        for (size_t i = 0; i < positions.size(); ++i) {
            positions[i] = glm::vec3(
                static_cast<float>(flat[3 * i]),
                static_cast<float>(flat[3 * i + 1]),
                static_cast<float>(flat[3 * i + 2]));
        }
        return position_records(positions);
    }

    std::optional<Records> records_of(const DataVariant& vertices, const VertexLayout& layout)
    {
        Records records;
        records.layout = VertexLayout::for_meshes(k_canonical_stride);

        if (const auto* bytes = std::get_if<std::vector<uint8_t>>(&vertices)) {
            records.borrowed = *bytes;
            records.layout = layout;
            return records;
        }
        if (const auto* positions = std::get_if<std::vector<glm::vec3>>(&vertices)) {
            records.built = position_records(*positions);
            return records;
        }
        if (const auto* flat = std::get_if<std::vector<float>>(&vertices)) {
            if (flat->size() % 3 == 0) {
                records.built = flat_position_records(*flat);
                return records;
            }
        } else if (const auto* wide = std::get_if<std::vector<double>>(&vertices)) {
            if (wide->size() % 3 == 0) {
                records.built = flat_position_records(*wide);
                return records;
            }
        }

        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshData: vertices must be vec3 positions, flat float or double triples, or "
            "uint8 records, and a flat array must hold whole triples");
        return std::nullopt;
    }

    std::optional<Triangles> triangles_of(const DataVariant& indices)
    {
        Triangles triangles;
        if (const auto* wide = std::get_if<std::vector<uint32_t>>(&indices)) {
            triangles.borrowed = *wide;
            return triangles;
        }
        if (const auto* half = std::get_if<std::vector<uint16_t>>(&indices)) {
            triangles.widened.assign(half->begin(), half->end());
            return triangles;
        }
        if (const auto* narrow = std::get_if<std::vector<uint8_t>>(&indices)) {
            triangles.widened.assign(narrow->begin(), narrow->end());
            return triangles;
        }

        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "MeshData: triangle indices must be uint32, uint16 or uint8");
        return std::nullopt;
    }

    bool records_are_whole(std::span<const uint8_t> records, const VertexLayout& layout)
    {
        if (layout.stride_bytes == 0 || records.empty() || records.size() % layout.stride_bytes != 0) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "MeshData: {} bytes of records are not a whole number of {} byte vertices",
                records.size(), layout.stride_bytes);
            return false;
        }
        return true;
    }

    std::optional<MeshData> with_triangles(
        std::span<const uint8_t> records, const VertexLayout& layout, std::span<const uint32_t> indices)
    {
        if (!records_are_whole(records, layout)) {
            return std::nullopt;
        }

        const size_t count = records.size() / layout.stride_bytes;
        if (indices.empty() || indices.size() % 3 != 0
            || std::ranges::any_of(indices, [count](uint32_t i) { return i >= count; })) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "MeshData: {} indices are not whole triangles over {} vertices", indices.size(), count);
            return std::nullopt;
        }

        auto data = MeshData::empty();
        MeshInsertion insertion(data.vertex_variant, data.index_variant);
        insertion.insert_flat(records, indices, layout);
        const auto access = insertion.build();
        if (!access) {
            return std::nullopt;
        }
        data.layout = access->layout;
        return data;
    }

    std::optional<MeshData> with_faces(
        std::span<const uint8_t> records,
        const VertexLayout& layout,
        FaceList polygons,
        bool shorter_quad_diagonal,
        float convexity_tolerance)
    {
        if (!records_are_whole(records, layout)) {
            return std::nullopt;
        }

        auto data = MeshData::empty();
        data.vertex_variant = std::vector<uint8_t>(records.begin(), records.end());
        data.layout = layout;
        data.layout.vertex_count = static_cast<uint32_t>(records.size() / layout.stride_bytes);

        const auto failed = data.assign_faces(std::move(polygons), shorter_quad_diagonal, convexity_tolerance);
        if (!failed) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "MeshData: the polygons are not well formed over {} vertices, or the layout has no position",
                data.layout.vertex_count);
            return std::nullopt;
        }
        if (!failed->empty()) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "MeshData: {} polygons could not be triangulated and have no triangles", failed->size());
        }
        if (!data.is_valid()) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "MeshData: no polygon could be triangulated");
            return std::nullopt;
        }
        return data;
    }

} // namespace

std::optional<MeshData> MeshData::from_records(
    std::span<const uint8_t> records, const VertexLayout& layout, std::span<const uint32_t> triangles)
{
    return with_triangles(records, layout, triangles);
}

std::optional<MeshData> MeshData::from_records(
    std::span<const uint8_t> records,
    const VertexLayout& layout,
    FaceList polygons,
    bool shorter_quad_diagonal,
    float convexity_tolerance)
{
    return with_faces(records, layout, std::move(polygons), shorter_quad_diagonal, convexity_tolerance);
}

std::optional<MeshData> MeshData::from_positions(
    std::span<const glm::vec3> positions, std::span<const uint32_t> triangles)
{
    const auto records = position_records(positions);
    return with_triangles(records, VertexLayout::for_meshes(k_canonical_stride), triangles);
}

std::optional<MeshData> MeshData::from_positions(
    std::span<const glm::vec3> positions, FaceList polygons, bool shorter_quad_diagonal, float convexity_tolerance)
{
    const auto records = position_records(positions);
    return with_faces(records, VertexLayout::for_meshes(k_canonical_stride), std::move(polygons),
        shorter_quad_diagonal, convexity_tolerance);
}

std::optional<MeshData> MeshData::from_variants(
    const DataVariant& vertices, const DataVariant& triangles, const VertexLayout& layout)
{
    const auto records = records_of(vertices, layout);
    const auto indices = triangles_of(triangles);
    if (!records || !indices) {
        return std::nullopt;
    }
    return with_triangles(records->bytes(), records->layout, indices->indices());
}

std::optional<MeshData> MeshData::from_variants(
    const DataVariant& vertices,
    FaceList polygons,
    bool shorter_quad_diagonal,
    float convexity_tolerance,
    const VertexLayout& layout)
{
    const auto records = records_of(vertices, layout);
    if (!records) {
        return std::nullopt;
    }
    return with_faces(records->bytes(), records->layout, std::move(polygons),
        shorter_quad_diagonal, convexity_tolerance);
}

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

std::optional<std::vector<uint32_t>> MeshData::assign_faces(
    FaceList polygons, bool shorter_quad_diagonal, float convexity_tolerance)
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

    auto triangulation = Kinesis::triangulate_faces(*positions, *incidence, shorter_quad_diagonal, convexity_tolerance);
    if (!triangulation) {
        return std::nullopt;
    }

    index_variant = std::move(triangulation->indices);
    faces = std::move(polygons);
    submeshes.reset();
    return std::move(triangulation->failed_faces);
}

} // namespace MayaFlux::Kakshya
