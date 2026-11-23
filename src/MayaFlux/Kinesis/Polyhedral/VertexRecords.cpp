#include "VertexRecords.hpp"

namespace MayaFlux::Kinesis {

namespace {

    using Kakshya::DataModality;
    using glm::dvec3;

    struct Columns {
        size_t stride {};
        size_t count {};
    };

    std::optional<Columns> columns_of(std::span<const uint8_t> records, const Kakshya::VertexLayout& layout)
    {
        const size_t stride = layout.stride_bytes;
        if (stride == 0 || records.size() % stride != 0) {
            return std::nullopt;
        }
        return Columns { .stride = stride, .count = records.size() / stride };
    }

    std::optional<size_t> offset_fitting(
        const Kakshya::VertexLayout& layout, DataModality modality, size_t width)
    {
        const auto offset = attribute_offset(layout, modality);
        if (!offset || *offset + width > layout.stride_bytes) {
            return std::nullopt;
        }
        return offset;
    }

    template <typename T>
    T load(std::span<const uint8_t> records, size_t stride, size_t record, size_t offset) noexcept
    {
        T value {};
        std::memcpy(&value, records.data() + record * stride + offset, sizeof(T));
        return value;
    }

    template <typename T>
    void store(std::span<uint8_t> records, size_t stride, size_t record, size_t offset, const T& value) noexcept
    {
        std::memcpy(records.data() + record * stride + offset, &value, sizeof(T));
    }

    template <typename T>
    std::vector<T> decode(std::span<const uint8_t> records, const Columns& columns, size_t offset)
    {
        std::vector<T> values(columns.count);
        detail::for_each_indexed(std::execution::par_unseq, std::span<T>(values),
            [&](size_t record, T& out) { out = load<T>(records, columns.stride, record, offset); });
        return values;
    }

    bool all_in(std::span<const uint8_t> records, const Columns& columns, size_t offset,
        const glm::vec3& zero, const glm::vec3& fallback)
    {
        for (size_t record = 0; record < columns.count; ++record) {
            const auto value = load<glm::vec3>(records, columns.stride, record, offset);
            if (value != zero && value != fallback) {
                return false;
            }
        }
        return true;
    }

    bool tangents_absent(std::span<const uint8_t> records, const Columns& columns, size_t offset, bool handed)
    {
        for (size_t record = 0; record < columns.count; ++record) {
            const auto xyz = load<glm::vec3>(records, columns.stride, record, offset);
            if (xyz == glm::vec3(0.0F)) {
                continue;
            }
            if (xyz != glm::vec3(1.0F, 0.0F, 0.0F)) {
                return false;
            }
            if (handed && load<float>(records, columns.stride, record, offset + sizeof(glm::vec3)) != 1.0F) {
                return false;
            }
        }
        return true;
    }

    std::optional<Incidence> triangle_incidence(
        std::span<const uint32_t> triangles, std::vector<uint32_t>& sizes, size_t vertex_count)
    {
        if (triangles.size() % 3 != 0) {
            return std::nullopt;
        }
        sizes.assign(triangles.size() / 3, 3U);
        return Incidence::create(sizes, triangles, vertex_count);
    }

    FillOutcome write_normals(
        std::span<uint8_t> records,
        const Kakshya::VertexLayout& layout,
        const Incidence& incidence,
        AttributePolicy policy,
        NormalWeighting weighting)
    {
        const auto columns = columns_of(records, layout);
        const auto position = offset_fitting(layout, DataModality::VERTEX_POSITIONS_3D, sizeof(glm::vec3));
        const auto normal = offset_fitting(layout, DataModality::VERTEX_NORMALS_3D, sizeof(glm::vec3));
        if (!columns || !position || !normal || incidence.vertex_count() != columns->count) {
            return FillOutcome::Unsupported;
        }

        if (policy == AttributePolicy::IfAbsent
            && !all_in(records, *columns, *normal, glm::vec3(0.0F), glm::vec3(0.0F, 0.0F, 1.0F))) {
            return FillOutcome::Kept;
        }

        const auto positions = decode<glm::vec3>(records, *columns, *position);
        const auto normals = weighted_vertex_normals(positions, incidence, weighting);
        if (!normals) {
            return FillOutcome::Unsupported;
        }

        const size_t stride = columns->stride;
        const size_t at = *normal;
        detail::for_each_indexed(std::execution::par_unseq, std::span<const glm::vec3>(*normals),
            [&](size_t record, const glm::vec3& value) {
                if (glm::dot(value, value) > 0.0F) {
                    store(records, stride, record, at, value);
                }
            });
        return FillOutcome::Written;
    }

    double corner_angle(const dvec3 (&corners)[3], size_t slot) noexcept
    {
        const dvec3 before = corners[(slot + 2) % 3] - corners[slot];
        const dvec3 after = corners[(slot + 1) % 3] - corners[slot];
        return std::atan2(glm::length(glm::cross(before, after)), glm::dot(before, after));
    }

} // namespace

std::optional<size_t> attribute_offset(const Kakshya::VertexLayout& layout, Kakshya::DataModality modality)
{
    const auto it = std::ranges::find(layout.attributes, modality, &Kakshya::VertexAttributeLayout::component_modality);
    if (it == layout.attributes.end()) {
        return std::nullopt;
    }
    return it->offset_in_vertex;
}

std::optional<std::vector<glm::vec3>> read_vertex_positions(
    std::span<const uint8_t> records, const Kakshya::VertexLayout& layout)
{
    const auto columns = columns_of(records, layout);
    const auto position = offset_fitting(layout, DataModality::VERTEX_POSITIONS_3D, sizeof(glm::vec3));
    if (!columns || !position) {
        return std::nullopt;
    }
    return decode<glm::vec3>(records, *columns, *position);
}

std::vector<uint8_t> gather_records(
    std::span<const uint8_t> records, size_t stride, std::span<const uint32_t> source_vertex)
{
    if (stride == 0 || records.size() % stride != 0) {
        return {};
    }
    const size_t count = records.size() / stride;
    if (std::ranges::any_of(source_vertex, [count](uint32_t vertex) { return vertex >= count; })) {
        return {};
    }

    std::vector<uint8_t> gathered(source_vertex.size() * stride);
    detail::for_each_indexed(std::execution::par_unseq, source_vertex,
        [&](size_t record, const uint32_t& vertex) {
            std::memcpy(gathered.data() + record * stride, records.data() + static_cast<size_t>(vertex) * stride, stride);
        });
    return gathered;
}

FillOutcome write_vertex_normals(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    const Incidence& incidence,
    AttributePolicy policy,
    NormalWeighting weighting)
{
    return write_normals(records, layout, incidence, policy, weighting);
}

FillOutcome write_vertex_normals(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    std::span<const uint32_t> triangles,
    AttributePolicy policy,
    NormalWeighting weighting)
{
    const auto columns = columns_of(records, layout);
    if (!columns) {
        return FillOutcome::Unsupported;
    }
    std::vector<uint32_t> sizes;
    const auto incidence = triangle_incidence(triangles, sizes, columns->count);
    if (!incidence) {
        return FillOutcome::Unsupported;
    }
    return write_normals(records, layout, *incidence, policy, weighting);
}

FillOutcome write_vertex_tangents(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    std::span<const uint32_t> triangles,
    AttributePolicy policy)
{
    const auto columns = columns_of(records, layout);
    const auto position = offset_fitting(layout, DataModality::VERTEX_POSITIONS_3D, sizeof(glm::vec3));
    const auto coordinate = offset_fitting(layout, DataModality::TEXTURE_COORDS_2D, sizeof(glm::vec2));
    const auto normal = offset_fitting(layout, DataModality::VERTEX_NORMALS_3D, sizeof(glm::vec3));
    if (!columns || !position || !coordinate || !normal) {
        return FillOutcome::Unsupported;
    }

    const bool handed = offset_fitting(layout, DataModality::VERTEX_TANGENTS_4D, sizeof(glm::vec4)).has_value();
    const auto tangent = handed
        ? offset_fitting(layout, DataModality::VERTEX_TANGENTS_4D, sizeof(glm::vec4))
        : offset_fitting(layout, DataModality::VERTEX_TANGENTS_3D, sizeof(glm::vec3));
    if (!tangent) {
        return FillOutcome::Unsupported;
    }

    std::vector<uint32_t> sizes;
    const auto incidence = triangle_incidence(triangles, sizes, columns->count);
    if (!incidence) {
        return FillOutcome::Unsupported;
    }

    if (policy == AttributePolicy::IfAbsent && !tangents_absent(records, *columns, *tangent, handed)) {
        return FillOutcome::Kept;
    }

    const auto coordinates = decode<glm::vec2>(records, *columns, *coordinate);
    const bool parametrised = std::ranges::any_of(coordinates, [](const glm::vec2& uv) { return uv != glm::vec2(0.0F); });
    if (!parametrised) {
        return FillOutcome::Unsupported;
    }

    const auto positions = decode<glm::vec3>(records, *columns, *position);
    const auto normals = decode<glm::vec3>(records, *columns, *normal);
    const VertexCorners around = incidence->vertex_corners();

    const size_t stride = columns->stride;
    const size_t at = *tangent;
    detail::for_each_indexed(std::execution::par_unseq, std::span<const glm::vec3>(normals),
        [&](size_t vertex, const glm::vec3& vertex_normal) {
            dvec3 along(0.0);
            dvec3 across(0.0);

            for (const uint32_t corner : around.around(vertex)) {
                const size_t slot = corner % 3;
                const uint32_t ids[3] = { triangles[corner - slot], triangles[corner - slot + 1], triangles[corner - slot + 2] };
                const dvec3 p[3] = { dvec3(positions[ids[0]]), dvec3(positions[ids[1]]), dvec3(positions[ids[2]]) };
                const glm::dvec2 t[3] = { glm::dvec2(coordinates[ids[0]]), glm::dvec2(coordinates[ids[1]]), glm::dvec2(coordinates[ids[2]]) };

                const dvec3 e1 = p[1] - p[0];
                const dvec3 e2 = p[2] - p[0];
                const glm::dvec2 d1 = t[1] - t[0];
                const glm::dvec2 d2 = t[2] - t[0];
                const double determinant = d1.x * d2.y - d2.x * d1.y;
                if (!(std::abs(determinant) > 0.0)) {
                    continue;
                }

                const dvec3 toward_u = (e1 * d2.y - e2 * d1.y) / determinant;
                const dvec3 toward_v = (e2 * d1.x - e1 * d2.x) / determinant;
                const double along_length = glm::length(toward_u);
                const double across_length = glm::length(toward_v);
                if (!(along_length > 0.0) || !(across_length > 0.0)) {
                    continue;
                }

                const double weight = corner_angle(p, slot);
                along += weight * toward_u / along_length;
                across += weight * toward_v / across_length;
            }

            const dvec3 n(vertex_normal);
            const dvec3 flat = along - n * glm::dot(n, along);
            const double length = glm::length(flat);
            if (!(length > 0.0)) {
                return;
            }

            const dvec3 axis = flat / length;
            const glm::vec3 xyz(axis);
            if (handed) {
                const float sign = glm::dot(glm::cross(n, axis), across) < 0.0 ? -1.0F : 1.0F;
                store(records, stride, vertex, at, glm::vec4(xyz, sign));
            } else {
                store(records, stride, vertex, at, xyz);
            }
        });
    return FillOutcome::Written;
}

FillOutcome write_vertex_normals(Kakshya::MeshData& mesh, AttributePolicy policy, NormalWeighting weighting)
{
    auto* vertices = std::get_if<std::vector<uint8_t>>(&mesh.vertex_variant);
    const auto* indices = std::get_if<std::vector<uint32_t>>(&mesh.index_variant);
    if (!vertices || !indices) {
        return FillOutcome::Unsupported;
    }
    return write_vertex_normals(std::span<uint8_t>(*vertices), mesh.layout, std::span<const uint32_t>(*indices), policy, weighting);
}

FillOutcome write_vertex_tangents(Kakshya::MeshData& mesh, AttributePolicy policy)
{
    auto* vertices = std::get_if<std::vector<uint8_t>>(&mesh.vertex_variant);
    const auto* indices = std::get_if<std::vector<uint32_t>>(&mesh.index_variant);
    if (!vertices || !indices) {
        return FillOutcome::Unsupported;
    }
    return write_vertex_tangents(std::span<uint8_t>(*vertices), mesh.layout, std::span<const uint32_t>(*indices), policy);
}

} // namespace MayaFlux::Kinesis
