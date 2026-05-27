#pragma once

#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct Vertex
 * @brief Type-neutral vertex carrying position, color, orientation, and a scalar.
 *
 * The common substrate for geometry construction and spatial sampling before
 * a concrete vertex type is chosen. All fields are independent of topology
 * and primitive type. Callers project into PointVertex, LineVertex, or
 * MeshVertex via to_point_vertex(), to_line_vertex(), to_mesh_vertex().
 *
 * normal and tangent default to the canonical Z-up / X-right frame.
 * Generation functions with a meaningful surface orientation (sphere surface,
 * torus, extruded path) should override them.
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 color { 1.0F };
    float scalar { 1.0F }; ///< Normalised scalar; maps to size (PointVertex) or thickness (LineVertex)
    glm::vec3 normal { 0.0F, 0.0F, 1.0F };
    glm::vec3 tangent { 1.0F, 0.0F, 0.0F };
};

/**
 * @struct PointVertex
 * @brief Vertex type for point primitives (POINT_LIST topology)
 *
 * Layout (60 bytes):
 *   offset  0 — position  vec3  (12)
 *   offset 12 — color     vec3  (12)
 *   offset 24 — size      float  (4)
 *   offset 28 — uv        vec2   (8)
 *   offset 36 — normal    vec3  (12)
 *   offset 48 — tangent   vec3  (12)
 */
struct PointVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float size = 10.0F;
    glm::vec2 uv = glm::vec2(0.0F);
    glm::vec3 normal = glm::vec3(0.0F, 0.0F, 1.0F);
    glm::vec3 tangent = glm::vec3(1.0F, 0.0F, 0.0F);
};

/**
 * @struct LineVertex
 * @brief Vertex type for line primitives (LINE_LIST / LINE_STRIP topology)
 *
 * Layout (60 bytes):
 *   offset  0 — position  vec3  (12)
 *   offset 12 — color     vec3  (12)
 *   offset 24 — thickness float  (4)
 *   offset 28 — uv        vec2   (8)
 *   offset 36 — normal    vec3  (12)
 *   offset 48 — tangent   vec3  (12)
 */
struct LineVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float thickness = 2.0F;
    glm::vec2 uv = glm::vec2(0.0F);
    glm::vec3 normal = glm::vec3(0.0F, 0.0F, 1.0F);
    glm::vec3 tangent = glm::vec3(1.0F, 0.0F, 0.0F);
};

/**
 * @struct MeshVertex
 * @brief Vertex type for indexed triangle mesh primitives (TRIANGLE_LIST topology)
 *
 * Shares the universal 60-byte layout with PointVertex and LineVertex.
 * The scalar field at offset 24 is interpreted as a per-vertex weight
 * (displacement magnitude, blend factor, or any user-defined scalar).
 * FieldOperator works identically on MeshVertex data: same stride,
 * same offsets, same raw byte buffer approach.
 *
 * Layout (60 bytes):
 *   offset  0  position  vec3  (12)
 *   offset 12  color     vec3  (12)
 *   offset 24  weight    float  (4)
 *   offset 28  uv        vec2   (8)
 *   offset 36  normal    vec3  (12)
 *   offset 48  tangent   vec3  (12)
 *
 * Intended for use with an external index buffer (uint32_t).
 * GeometryWriterNode::set_vertices<MeshVertex>() populates vertex data.
 * Index data is stored separately (see GeometryBuffer index support).
 */
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float weight = 0.0F;
    glm::vec2 uv = glm::vec2(0.0F);
    glm::vec3 normal = glm::vec3(0.0F, 0.0F, 1.0F);
    glm::vec3 tangent = glm::vec3(1.0F, 0.0F, 0.0F);
};

static_assert(sizeof(PointVertex) == 60,
    "PointVertex layout changed — update VertexLayout::for_points(), VertexAccess write helpers, and shaders");
static_assert(sizeof(LineVertex) == 60,
    "LineVertex layout changed — update VertexLayout::for_lines(), VertexAccess write helpers, and shaders");
static_assert(sizeof(MeshVertex) == 60,
    "MeshVertex layout changed — must match PointVertex/LineVertex stride for FieldOperator compatibility");

static_assert(offsetof(MeshVertex, position) == 0, "position must be at offset 0");
static_assert(offsetof(MeshVertex, color) == 12, "color must be at offset 12");
static_assert(offsetof(MeshVertex, weight) == 24, "weight must be at offset 24");
static_assert(offsetof(MeshVertex, uv) == 28, "uv must be at offset 28");
static_assert(offsetof(MeshVertex, normal) == 36, "normal must be at offset 36");
static_assert(offsetof(MeshVertex, tangent) == 48, "tangent must be at offset 48");

/**
 * @struct TextureQuadVertex
 * @brief Vertex layout for textured quad geometry (position + UV).
 *
 * Matches the vertex input expected by texture.vert.spv:
 * location 0 → vec3 position, location 1 → vec2 texcoord.
 */
struct TextureQuadVertex {
    glm::vec3 position;
    glm::vec2 texcoord;
};

/**
 * @brief Deduce a VertexLayout from a vertex struct type.
 */
template <typename T>
VertexLayout vertex_layout_for()
{
    if constexpr (std::is_same_v<T, PointVertex>) {
        return VertexLayout::for_points();
    } else if constexpr (std::is_same_v<T, LineVertex>) {
        return VertexLayout::for_lines();
    } else if constexpr (std::is_same_v<T, MeshVertex>) {
        return VertexLayout::for_meshes();
    } else {
        static_assert(!std::is_same_v<T, T>,
            "vertex_layout_for: unrecognised vertex type");
    }
}

//-----------------------------------------------------------------------------
// Vertex projection — convert raw Vertex to concrete vertex types.
// These are the ONLY places that touch PointVertex / LineVertex fields.
// Adding a new vertex type means adding one function here, nothing else.
//-----------------------------------------------------------------------------

/**
 * @brief Project raw Vertex to PointVertex.
 * @param s          Source vertex
 * @param size_range Min/max point size; scalar linearly interpolates within it
 * @return PointVertex with all fields populated from vertex
 */
[[nodiscard]] inline PointVertex to_point_vertex(
    const Vertex& s,
    glm::vec2 size_range = { 8.0F, 12.0F }) noexcept
{
    return {
        .position = s.position,
        .color = s.color,
        .size = glm::mix(size_range.x, size_range.y, s.scalar),
        .uv = glm::vec2(0.0F),
        .normal = s.normal,
        .tangent = s.tangent,
    };
}

/**
 * @brief Project raw Vertex to LineVertex.
 * @param s               Source vertex
 * @param thickness_range Min/max line thickness; scalar linearly interpolates within it
 * @return LineVertex with all fields populated from vertex
 */
[[nodiscard]] inline LineVertex to_line_vertex(
    const Vertex& s,
    glm::vec2 thickness_range = { 1.0F, 2.0F }) noexcept
{
    return {
        .position = s.position,
        .color = s.color,
        .thickness = glm::mix(thickness_range.x, thickness_range.y, s.scalar),
        .uv = glm::vec2(0.0F),
        .normal = s.normal,
        .tangent = s.tangent,
    };
}

/**
 * @brief Project raw Vertex to MeshVertex.
 * @param s Source vertex
 * @return MeshVertex with all fields populated from vertex
 *
 * MeshVertex doesn't have a size/thickness field, so scalar is passed through
 * as weight for potential use in shader-based deformation or other effects.
 */
[[nodiscard]] inline MeshVertex to_mesh_vertex(
    const Vertex& s,
    glm::vec2 weight_range = { 0.0F, 1.0F }) noexcept
{
    return {
        .position = s.position,
        .color = s.color,
        .weight = glm::mix(weight_range.x, weight_range.y, s.scalar),
        .uv = glm::vec2(0.0F),
        .normal = s.normal,
        .tangent = s.tangent,
    };
}

/**
 * @brief Batch-project raw Vertex vector to PointVertex.
 * @param vertices    Source vertices
 * @param size_range Size range passed to to_point_vertex
 * @return PointVertex vector of equal length
 */
[[nodiscard]] MAYAFLUX_API std::vector<PointVertex> to_point_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 size_range = { 8.0F, 12.0F });

/**
 * @brief Batch-project raw Vertex vector to LineVertex.
 * @param vertices         Source vertices
 * @param thickness_range Thickness range passed to to_line_vertex
 * @return LineVertex vector of equal length
 */
[[nodiscard]] MAYAFLUX_API std::vector<LineVertex> to_line_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 thickness_range = { 1.0F, 2.0F });

/**
 * @brief Batch-project raw Vertex vector to MeshVertex.
 * @param vertices Source vertices
 * @param weight_range Weight range passed to to_mesh_vertex (for potential shader use)
 * @return MeshVertex vector of equal length
 */
[[nodiscard]] MAYAFLUX_API std::vector<MeshVertex> to_mesh_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 weight_range = { 0.0F, 1.0F });

} // namespace MayaFlux::Kakshya
