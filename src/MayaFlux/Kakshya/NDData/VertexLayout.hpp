#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct VertexAttributeLayout
 * @brief Semantic description of a single vertex attribute
 *
 * Describes one component of vertex data without exposing Vulkan types.
 * The modality encodes everything needed (3D position, 2D texture coords, etc.)
 */
struct VertexAttributeLayout {
    /**
     * Semantic type of this attribute
     * e.g., VERTEX_POSITIONS_3D → vec3, TEXTURE_COORDS_2D → vec2
     */
    DataModality component_modality;

    /**
     * Byte offset of this attribute within one vertex
     * e.g., position at 0, normal at 12, color at 24
     */
    uint32_t offset_in_vertex = 0;

    /**
     * Optional name for debugging/introspection
     * e.g., "position", "normal", "texCoord"
     */
    std::string name;
};

/**
 * @struct VertexLayout
 * @brief Complete description of vertex data layout in a buffer
 *
 * Fully semantic and backend-agnostic. Portal layer translates to Vulkan.
 * Derived from buffer's modality and dimensions.
 */
struct VertexLayout {
    /**
     * Total number of vertices in this buffer
     */
    uint32_t vertex_count = 0;

    /**
     * Total bytes per vertex (stride in Vulkan terms)
     * e.g., 3 floats (position) + 3 floats (normal) = 24 bytes
     */
    uint32_t stride_bytes = 0;

    /**
     * All attributes that make up one vertex
     * Ordered by shader location (0, 1, 2, ...)
     */
    std::vector<VertexAttributeLayout> attributes;

    /**
     * @brief Helper: compute stride from attributes if not explicitly set
     */
    void compute_stride()
    {
        if (stride_bytes == 0 && !attributes.empty()) {
            uint32_t max_offset = 0;
            uint32_t last_size = 0;

            for (const auto& attr : attributes) {
                uint32_t attr_size = modality_size_bytes(attr.component_modality);
                max_offset = std::max(max_offset, attr.offset_in_vertex);
                if (attr.offset_in_vertex == max_offset) {
                    last_size = attr_size;
                }
            }

            stride_bytes = max_offset + last_size;
        }
    }

    /**
     * @brief Factory: layout for PointVertex (position, color, size, uv, normal, tangent)
     *
     * Matches PointVertex field order exactly (60 bytes):
     *   loc 0  offset  0  VERTEX_POSITIONS_3D  position
     *   loc 1  offset 12  VERTEX_COLORS_RGB    color
     *   loc 2  offset 24  UNKNOWN              size
     *   loc 3  offset 28  TEXTURE_COORDS_2D    uv
     *   loc 4  offset 36  VERTEX_NORMALS_3D    normal
     *   loc 5  offset 48  VERTEX_TANGENTS_3D   tangent
     *
     * @param stride Override stride (default: sizeof(PointVertex) == 60)
     */
    static VertexLayout for_points(uint32_t stride = 60)
    {
        VertexLayout layout;
        layout.stride_bytes = stride;

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_COLORS_RGB,
            .offset_in_vertex = 12,
            .name = "color" });

        layout.attributes.push_back({ .component_modality = DataModality::SCALAR_F32,
            .offset_in_vertex = 24,
            .name = "size" });

        layout.attributes.push_back({ .component_modality = DataModality::TEXTURE_COORDS_2D,
            .offset_in_vertex = 28,
            .name = "uv" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_NORMALS_3D,
            .offset_in_vertex = 36,
            .name = "normal" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_TANGENTS_3D,
            .offset_in_vertex = 48,
            .name = "tangent" });

        return layout;
    }

    /**
     * @brief Factory: layout for LineVertex (position, color, thickness, uv, normal, tangent)
     *
     * Matches LineVertex field order exactly (60 bytes):
     *   loc 0  offset  0  VERTEX_POSITIONS_3D  position
     *   loc 1  offset 12  VERTEX_COLORS_RGB    color
     *   loc 2  offset 24  UNKNOWN              thickness
     *   loc 3  offset 28  TEXTURE_COORDS_2D    uv
     *   loc 4  offset 36  VERTEX_NORMALS_3D    normal
     *   loc 5  offset 48  VERTEX_TANGENTS_3D   tangent
     *
     * @param stride Override stride (default: sizeof(LineVertex) == 60)
     */
    static VertexLayout for_lines(uint32_t stride = 60)
    {
        VertexLayout layout;
        layout.stride_bytes = stride;

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_COLORS_RGB,
            .offset_in_vertex = 12,
            .name = "color" });

        layout.attributes.push_back({ .component_modality = DataModality::SCALAR_F32,
            .offset_in_vertex = 24,
            .name = "thickness" });

        layout.attributes.push_back({ .component_modality = DataModality::TEXTURE_COORDS_2D,
            .offset_in_vertex = 28,
            .name = "uv" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_NORMALS_3D,
            .offset_in_vertex = 36,
            .name = "normal" });

        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_TANGENTS_3D,
            .offset_in_vertex = 48,
            .name = "tangent" });
        return layout;
    }

    /**
     * @brief Factory: Create layout for textured quad primitives (position, texcoord).
     * @param vertex_count Number of vertices in the buffer (default: 4).
     * @return VertexLayout configured for Nodes::TextureQuadVertex.
     */
    static VertexLayout for_textured_quad(uint32_t vertex_count = 4)
    {
        VertexLayout layout;
        layout.vertex_count = vertex_count;
        layout.stride_bytes = static_cast<uint32_t>(sizeof(glm::vec3) + sizeof(glm::vec2)); // 20

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::TEXTURE_COORDS_2D,
            .offset_in_vertex = static_cast<uint32_t>(sizeof(glm::vec3)),
            .name = "texcoord" });

        return layout;
    }

private:
    /**
     * Get size in bytes for a given modality
     * Mirrors VKBuffer::get_format() logic
     */
    static uint32_t modality_size_bytes(DataModality mod)
    {
        switch (mod) {
        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
        case DataModality::VERTEX_COLORS_RGB:
            return sizeof(glm::vec3); // 12

        case DataModality::TEXTURE_COORDS_2D:
            return sizeof(glm::vec2); // 8

        case DataModality::VERTEX_COLORS_RGBA:
            return sizeof(glm::vec4); // 16

        case DataModality::AUDIO_1D:
        case DataModality::AUDIO_MULTICHANNEL:
            return sizeof(double); // 8

        default:
            return 4; // Conservative default
        }
    }
};

} // namespace MayaFlux::Buffers
