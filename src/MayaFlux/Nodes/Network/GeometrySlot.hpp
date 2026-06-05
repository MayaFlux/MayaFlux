#pragma once

#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct GeometrySlot
 * @brief Peer unit within an InstanceNetwork.
 *
 * Holds any GeometryWriterNode subclass. All per-vertex data (color, normals,
 * UVs, tangents) lives in the node's vertex buffer. The slot carries only
 * the per-instance world transform driven by operators or set directly.
 * dirty signals the buffer layer that the SSBO entry for this slot needs
 * re-upload.
 */
struct GeometrySlot {
    uint32_t index {};
    std::string name;
    std::shared_ptr<GpuSync::GeometryWriterNode> node;
    glm::mat4 transform { 1.0F };
    bool dirty { true };
};

} // namespace MayaFlux::Nodes::Network
