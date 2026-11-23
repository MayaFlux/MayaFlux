#pragma once

#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct GeometrySlot
 * @brief Peer unit within an InstanceNetwork.
 *
 * Holds any GeometryWriterNode subclass. All per-vertex data (color, normals,
 * UVs, tangents) lives in the node's vertex buffer. The slot carries the
 * per-instance world transform and a small payload, driven by operators or set
 * directly. dirty signals the buffer layer that the SSBO entries for this slot
 * need re-upload.
 *
 * The payload is what makes instances of one template differ besides where they
 * are. tint multiplies the template's vertex colour, texture picks one of the
 * buffer's instance textures and must stay below their count, and scalar is a free parameter: instance_point.vert scales the point
 * size by it and every instance vertex shader passes it on to the fragment
 * stage. The defaults (white, 0, 1) leave a template as authored.
 */
struct GeometrySlot {
    uint32_t index {};
    std::string name;
    std::shared_ptr<GpuSync::GeometryWriterNode> node;
    glm::mat4 transform { 1.0F };
    glm::vec3 tint { 1.0F };
    float scalar { 1.0F };
    uint32_t texture {};
    bool dirty { true };
};

} // namespace MayaFlux::Nodes::Network
