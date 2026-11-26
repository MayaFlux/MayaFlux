#pragma once

#include <glm/gtc/matrix_inverse.hpp>

namespace MayaFlux::Kinesis {

/**
 * @brief For a blend of affine transforms over a point set: each transform's motion from where it
 *        was bound, expressed in the point set's own frame.
 * @param frame Composed placement of the point set.
 * @param transforms Composed placement of every transform blended.
 * @param bind_inverses For each, the matrix taking a point of the set into that transform's space
 *        as bound.
 * @return inverse(frame) * transforms[i] * bind_inverses[i]. Empty when the sizes differ or the
 *         frame cannot be inverted, so the caller keeps what it had.
 *
 * Because the result is in the set's own frame, it is drawn with the set's placement exactly as an
 * unblended set is, and the set's own transform cancels.
 */
[[nodiscard]] inline std::vector<glm::mat4> relative_transforms(
    const glm::mat4& frame,
    std::span<const glm::mat4> transforms,
    std::span<const glm::mat4> bind_inverses)
{
    if (transforms.size() != bind_inverses.size()) {
        return {};
    }

    const float determinant = glm::determinant(frame);
    if (!std::isfinite(determinant) || determinant == 0.0F) {
        return {};
    }

    const glm::mat4 into_frame = glm::inverse(frame);
    std::vector<glm::mat4> out;
    out.reserve(transforms.size());
    for (size_t i = 0; i < transforms.size(); ++i) {
        out.push_back(into_frame * transforms[i] * bind_inverses[i]);
    }
    return out;
}

} // namespace MayaFlux::Kinesis
