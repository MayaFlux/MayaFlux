#pragma once

#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct MeshSlot
 * @brief Named, independently transformable mesh unit within a MeshNetwork.
 *
 * The unit of composition in MeshNetwork. Each slot owns a MeshWriterNode
 * holding its CPU-side geometry and carries a local-space transform. Parent
 * indices form a DAG; MeshNetwork resolves topological order so parents are
 * always processed before children.
 *
 * Transforms compose as: world = parent_world * local.
 * Operators read local, write back to local or directly to the MeshWriterNode
 * vertices. World transform is recomputed each cycle by MeshNetwork.
 *
 * No analog vocabulary: there are no bones, pivots, rigs, or joints.
 * A slot is a named matrix slot with geometry attached.
 */
struct MeshSlot {
    /// @brief Logical name for this slot. Used for lookup and logging only.
    std::string name;

    /// @brief The geometry node for this slot.
    std::shared_ptr<GpuSync::MeshWriterNode> node;

    /// @brief Local-space transform relative to parent (or world if no parent).
    glm::mat4 local_transform { 1.0F };

    /// @brief World-space transform, recomputed each cycle from the DAG.
    glm::mat4 world_transform { 1.0F };

    /// @brief Index of the parent slot, or nullopt for a root slot.
    std::optional<uint32_t> parent_index;

    /// @brief Indices of child slots. Populated by MeshNetwork::add_slot().
    std::vector<uint32_t> child_indices;

    /// @brief Set when local_transform or vertex data changes since last upload.
    bool dirty { true };
};

} // namespace MayaFlux::Nodes::Network
