#pragma once

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Nodes {

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
Kakshya::VertexLayout vertex_layout_for()
{
    if constexpr (std::is_same_v<T, PointVertex>) {
        return Kakshya::VertexLayout::for_points();
    } else if constexpr (std::is_same_v<T, LineVertex>) {
        return Kakshya::VertexLayout::for_lines();
    } else if constexpr (std::is_same_v<T, MeshVertex>) {
        return Kakshya::VertexLayout::for_meshes();
    } else {
        static_assert(!std::is_same_v<T, T>,
            "vertex_layout_for: unrecognised vertex type");
    }
}

} // namespace MayaFlux::Nodes
