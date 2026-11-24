#pragma once

#include <glm/gtc/matrix_inverse.hpp>

namespace MayaFlux::Kinesis {

/**
 * @brief The matrix that carries a point of a bound mesh to where its joint has moved it, in the
 *        mesh's own space.
 * @param mesh_placement Composed placement of the row that shows the mesh.
 * @param joint_placements Composed placement of every joint, in the order of the skin's joints.
 * @param inverse_bind The skin's inverse bind matrices, one for each joint.
 * @return One matrix for each joint: inverse(mesh_placement) * joint_placement * inverse_bind.
 *         Empty when the sizes differ or the mesh placement cannot be inverted, so the caller keeps
 *         the palette it had.
 *
 * Leaving the mesh's own space as the output space is what makes the result drawn with the row's
 * placement exactly as an unskinned mesh is, and what makes a file's rule that a skinned node's own
 * transform is ignored need no case of its own: the factor cancels it.
 */
[[nodiscard]] inline std::vector<glm::mat4> skin_palette(
    const glm::mat4& mesh_placement,
    std::span<const glm::mat4> joint_placements,
    std::span<const glm::mat4> inverse_bind)
{
    if (joint_placements.size() != inverse_bind.size()) {
        return {};
    }

    const float determinant = glm::determinant(mesh_placement);
    if (!std::isfinite(determinant) || determinant == 0.0F) {
        return {};
    }

    const glm::mat4 into_mesh = glm::inverse(mesh_placement);
    std::vector<glm::mat4> palette;
    palette.reserve(joint_placements.size());
    for (size_t j = 0; j < joint_placements.size(); ++j) {
        palette.push_back(into_mesh * joint_placements[j] * inverse_bind[j]);
    }
    return palette;
}

} // namespace MayaFlux::Kinesis
