#pragma once

#include "Incidence.hpp"
#include "Normals.hpp"

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::Kinesis {

/**
 * @enum AttributePolicy
 * @brief Whether a derived attribute may replace one the record already holds.
 */
enum class AttributePolicy : uint8_t {
    IfAbsent, ///< Only when no vertex carries a value of its own.
    Replace ///< Always.
};

/**
 * @enum FillOutcome
 * @brief What a write into vertex records did.
 */
enum class FillOutcome : uint8_t {
    Written, ///< The attribute was derived and stored.
    Kept, ///< The records already carry the attribute and the policy was IfAbsent.
    Unsupported ///< The layout, the indices or the other attributes cannot support it.
};

/**
 * @brief Byte offset of an attribute within one record.
 * @return nullopt when the layout has no attribute of that modality.
 */
[[nodiscard]] MAYAFLUX_API std::optional<size_t> attribute_offset(
    const Kakshya::VertexLayout& layout, Kakshya::DataModality modality);

/**
 * @brief The position of every record.
 *
 * Reads the vec3 at the layout's position attribute, so any stride works.
 *
 * @return nullopt when the layout has no position attribute that fits in a record, the stride
 * is zero, or the bytes are not whole records.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::vector<glm::vec3>> read_vertex_positions(
    std::span<const uint8_t> records, const Kakshya::VertexLayout& layout);

/**
 * @brief Copy the records of the listed vertices, in that order.
 *
 * Turns an Identification into the records of its new vertices: every attribute the
 * source carries comes along, and scatter_corner_values then replaces the ones that
 * differ per corner.
 *
 * @param records Interleaved records of @p stride bytes.
 * @param source_vertex Vertex to copy for each output record.
 * @return Empty when the stride is zero, the bytes are not whole records, or a vertex is
 * out of range.
 */
[[nodiscard]] MAYAFLUX_API std::vector<uint8_t> gather_records(
    std::span<const uint8_t> records, size_t stride, std::span<const uint32_t> source_vertex);

/**
 * @brief Store one value per corner into the record of the vertex that corner now names.
 *
 * Every corner of a vertex holds the same value, as Identification guarantees, so the
 * record receives that value. Writes run in corner order.
 *
 * @param records Interleaved records of @p stride bytes.
 * @param byte_offset Offset of the attribute within a record.
 * @param corner_vertex The vertex of every corner.
 * @param values One value per corner.
 * @return false, writing nothing, when the counts differ, the attribute does not fit in
 * a record, or a vertex is out of range.
 */
template <typename T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] bool scatter_corner_values(
    std::span<uint8_t> records,
    size_t stride,
    size_t byte_offset,
    std::span<const uint32_t> corner_vertex,
    std::span<const T> values)
{
    if (stride == 0 || records.size() % stride != 0 || corner_vertex.size() != values.size()
        || byte_offset + sizeof(T) > stride) {
        return false;
    }
    const size_t count = records.size() / stride;
    if (std::ranges::any_of(corner_vertex, [count](uint32_t vertex) { return vertex >= count; })) {
        return false;
    }
    for (size_t corner = 0; corner < corner_vertex.size(); ++corner) {
        std::memcpy(records.data() + corner_vertex[corner] * stride + byte_offset, &values[corner], sizeof(T));
    }
    return true;
}

/**
 * @brief Derive vertex normals from faces and store them in the records.
 *
 * Meant for geometry whose creator did not supply normals. Under IfAbsent the records are
 * left alone when any vertex carries a normal other than zero or the default (0, 0, 1). A
 * vertex that no face names, or whose faces cancel, keeps the normal it has.
 *
 * @param records Interleaved records, in place.
 * @param layout Describes the records. Needs a position and a normal attribute.
 * @param incidence Faces over the vertices of @p records. Its vertex count must be the
 * number of records.
 */
[[nodiscard]] MAYAFLUX_API FillOutcome write_vertex_normals(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    const Incidence& incidence,
    AttributePolicy policy = AttributePolicy::IfAbsent,
    NormalWeighting weighting = NormalWeighting::Angle);

/** @brief As above, for a triangle list given as three vertex indices per triangle. */
[[nodiscard]] MAYAFLUX_API FillOutcome write_vertex_normals(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    std::span<const uint32_t> triangles,
    AttributePolicy policy = AttributePolicy::IfAbsent,
    NormalWeighting weighting = NormalWeighting::Angle);

/**
 * @brief Derive tangents from texture coordinates and store them in the records.
 *
 * The tangent follows the direction of increasing u on each triangle, averaged around the
 * vertex by interior angle and made perpendicular to the vertex's own normal. A four
 * component tangent also gets the handedness of the bitangent in w. Needs a position, a
 * texture coordinate, a normal and a tangent attribute, and texture coordinates that are
 * not all zero. The normals are read, never written, so derive them first if the creator
 * gave none.
 *
 * Under IfAbsent the records are left alone when any vertex carries a tangent other than
 * zero or the default (1, 0, 0). A vertex whose triangles have no usable parametrisation
 * keeps the tangent it has.
 *
 * @param triangles Three vertex indices per triangle.
 */
[[nodiscard]] MAYAFLUX_API FillOutcome write_vertex_tangents(
    std::span<uint8_t> records,
    const Kakshya::VertexLayout& layout,
    std::span<const uint32_t> triangles,
    AttributePolicy policy = AttributePolicy::IfAbsent);

/** @brief write_vertex_normals over a mesh's own records and triangle indices. */
[[nodiscard]] MAYAFLUX_API FillOutcome write_vertex_normals(
    Kakshya::MeshData& mesh,
    AttributePolicy policy = AttributePolicy::IfAbsent,
    NormalWeighting weighting = NormalWeighting::Angle);

/** @brief write_vertex_tangents over a mesh's own records and triangle indices. */
[[nodiscard]] MAYAFLUX_API FillOutcome write_vertex_tangents(
    Kakshya::MeshData& mesh,
    AttributePolicy policy = AttributePolicy::IfAbsent);

} // namespace MayaFlux::Kinesis
