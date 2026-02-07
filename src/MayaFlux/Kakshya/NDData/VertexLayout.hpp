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
     * @brief Factory: Create layout for point primitives (position, color, size)
     * @return VertexLayout configured for PointVertex
     */
    static VertexLayout for_points(uint32_t stride = 28)
    {
        VertexLayout layout;
        layout.stride_bytes = stride;

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::VERTEX_COLORS_RGB,
            .offset_in_vertex = 12,
            .name = "color" });

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::UNKNOWN,
            .offset_in_vertex = 24,
            .name = "size" });

        return layout;
    }

    /**
     * @brief Factory: Create layout for line primitives (position, color, thickness)
     * @return VertexLayout configured for LineVertex
     */
    static VertexLayout for_lines(uint32_t stride = 28)
    {
        VertexLayout layout;
        layout.stride_bytes = stride;

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::VERTEX_COLORS_RGB,
            .offset_in_vertex = 12,
            .name = "color" });

        layout.attributes.push_back(VertexAttributeLayout {
            .component_modality = DataModality::UNKNOWN,
            .offset_in_vertex = 24,
            .name = "thickness" });

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
            return sizeof(glm::vec3);

        case DataModality::TEXTURE_COORDS_2D:
            return sizeof(glm::vec2);

        case DataModality::VERTEX_COLORS_RGBA:
            return sizeof(glm::vec4);

        case DataModality::AUDIO_1D:
        case DataModality::AUDIO_MULTICHANNEL:
            return sizeof(double);

        default:
            return 4; // Conservative default
        }
    }
};

} // namespace MayaFlux::Buffers
