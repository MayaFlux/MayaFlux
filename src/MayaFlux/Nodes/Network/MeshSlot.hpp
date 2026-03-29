#pragma once

#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::Network {

/**
 * @struct MeshSlot
 * @brief Named, independently transformable mesh unit within a MeshNetwork.
 *
 * Each slot owns a MeshWriterNode holding its CPU-side geometry and carries
 * a local-space transform. Parent indices form a DAG; MeshNetwork resolves
 * topological order so parents are always processed before children.
 *
 * World transform composes as: world = parent_world * local.
 *
 * slot.dirty signals the buffer layer that slot-level configuration changed
 * (transform, texture binding, parent reassignment) independently of whether
 * the node's vertex data changed. The processor checks both:
 *   slot.dirty || slot.node->needs_gpu_update()
 *
 * diffuse_texture is optional. When set, MeshNetworkBuffer binds it to the
 * shader's diffuse slot for this slot's draw range. Slots sharing the same
 * texture share one descriptor. Slots with distinct textures require the
 * texture array path (deferred).
 */
struct MeshSlot {
    /// @brief Position of this slot in MeshNetwork::m_slots. Stable after insertion.
    uint32_t index {};

    /// @brief Logical name. Used for lookup and logging only.
    std::string name;

    /// @brief Geometry node for this slot.
    std::shared_ptr<GpuSync::MeshWriterNode> node;

    /// @brief Local-space transform relative to parent (or world if root).
    glm::mat4 local_transform { 1.0F };

    /// @brief World-space transform, recomputed each cycle from the DAG.
    glm::mat4 world_transform { 1.0F };

    /// @brief Index of the parent slot, or nullopt for a root slot.
    std::optional<uint32_t> parent_index;

    /// @brief Indices of child slots. Populated by MeshNetwork::add_slot().
    std::vector<uint32_t> child_indices;

    /// @brief Set when local_transform or slot config changes since last upload.
    bool dirty { true };

    /// @brief Optional diffuse texture. Null means untextured for this slot.
    std::shared_ptr<Core::VKImage> diffuse_texture;
};

} // namespace MayaFlux::Nodes::Network
