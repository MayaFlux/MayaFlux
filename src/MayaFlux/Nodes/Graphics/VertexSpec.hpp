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

static_assert(sizeof(PointVertex) == 60,
    "PointVertex layout changed — update VertexLayout::for_points(), VertexAccess write helpers, and shaders");
static_assert(sizeof(LineVertex) == 60,
    "LineVertex layout changed — update VertexLayout::for_lines(), VertexAccess write helpers, and shaders");

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

} // namespace MayaFlux::Nodes
