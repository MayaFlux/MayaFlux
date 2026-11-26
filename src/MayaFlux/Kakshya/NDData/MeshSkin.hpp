#pragma once

namespace MayaFlux::Kakshya {

/**
 * @struct MeshSkin
 * @brief How the vertices of a mesh follow joints: how each joint starts relative to the mesh,
 *        and up to four joints with weights for every vertex.
 *
 * Kept in the facets of the source whose mesh it skins, as a shared pointer to a whole struct, so the vertex stays the
 * engine's one 68 byte record. It says nothing of which things the joints are: a joint is a number, and
 * whatever shows the mesh names them, as a MeshAssemblyPlacement does through its skeleton.
 * inverse_bind[j] takes a point of the mesh as it was bound to joint j's own space. Vertex v is
 * influenced by the joints influence_joints[v] holds, by the weights influence_weights[v] holds, in
 * matching components. A weight of zero means that component is unused, and the weights of a vertex
 * sum to one, or are all zero for a vertex the joints do not move.
 */
struct MeshSkin {
    std::vector<glm::mat4> inverse_bind;
    std::vector<glm::uvec4> influence_joints;
    std::vector<glm::vec4> influence_weights;

    /** @brief Joints the skin is bound to. */
    [[nodiscard]] size_t joint_count() const noexcept { return inverse_bind.size(); }

    /** @brief Vertices the skin covers. */
    [[nodiscard]] size_t vertex_count() const noexcept { return influence_joints.size(); }

    /**
     * @brief Whether the arrays agree and every weighted component names a joint.
     * @param vertices Vertex count of the mesh the skin is for, or 0 to skip that check.
     */
    [[nodiscard]] bool valid(size_t vertices = 0) const noexcept
    {
        if (inverse_bind.empty() || influence_joints.size() != influence_weights.size()
            || (vertices != 0 && influence_joints.size() != vertices)) {
            return false;
        }
        for (size_t v = 0; v < influence_joints.size(); ++v) {
            for (glm::length_t c = 0; c < 4; ++c) {
                if (influence_weights[v][c] != 0.0F && influence_joints[v][c] >= inverse_bind.size()) {
                    return false;
                }
            }
        }
        return true;
    }
};

} // namespace MayaFlux::Kakshya
