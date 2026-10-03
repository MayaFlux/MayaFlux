#pragma once

#include "Influence.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class RenderProcessor;
}

namespace MayaFlux::Nexus {

/**
 * @struct AttachConfig
 * @brief How an Agent attaches to a buffer that already exists.
 */
struct AttachConfig {
    /** @brief What a move of the Agent from outside does to the attachment. */
    enum class OnMove : uint8_t {
        Offset, ///< The move becomes the Agent's new offset from the buffer.
        Carry, ///< The buffer is moved along with the Agent.
    };

    /**
     * @brief Vertex whose position anchors the Agent. Unset anchors at the
     *        surface centroid, or the vertex mean for a mesh without faces.
     *
     * The geometry is read once, when attaching and on recenter(), by
     * read_anchor. A buffer it cannot read anchors at its local origin.
     */
    std::optional<uint32_t> index;

    /**
     * @brief Offset of the Agent from the anchor, in the Agent's own frame.
     *
     * Rotated by the Agent's orientation, so a look that sits in front of its
     * Agent stays in front as the Agent turns. Without an orientation it is a
     * world-space offset.
     */
    glm::vec3 offset {};

    OnMove on_move { OnMove::Offset };
};

/**
 * @struct PlacementStack
 * @brief The placements layered on one render processor, kept by its geometry transform source.
 */
struct PlacementStack;

/**
 * @struct Attachment
 * @brief State of one Agent attached to an existing buffer.
 *
 * The transform is applied to every render processor the buffer has, outside
 * whatever geometry transform that processor already had, which is kept
 * beside it and restored when the last placement on the processor is released.
 * Placements on one processor stack, later ones outside earlier ones, and each
 * is released on its own. Nothing is registered with a manager and nothing is
 * written to the buffer.
 */
struct Attachment {
    std::shared_ptr<Buffers::VKBuffer> buf;
    AttachConfig config;
    std::shared_ptr<glm::mat4> transform;
    std::vector<std::pair<std::shared_ptr<Buffers::RenderProcessor>, std::shared_ptr<PlacementStack>>> followed;
    glm::vec3 anchor {};
    glm::vec3 applied {};
};

/**
 * @brief Build an attachment to @p buf.
 * @param buf         Buffer to attach to. Must not be null.
 * @param config      Anchor vertex, offset and move behaviour.
 * @param orientation Orientation of the Agent, if it has one.
 */
MAYAFLUX_API Attachment make_attachment(
    std::shared_ptr<Buffers::VKBuffer> buf,
    const AttachConfig& config,
    const std::optional<glm::quat>& orientation);

/**
 * @brief Set @p position from the buffer, or fold an outside move into the attachment.
 * @param attachment  Attachment to follow.
 * @param position    Position of the Agent. Set if empty.
 * @param orientation Orientation of the Agent, if it has one.
 */
MAYAFLUX_API void follow_attachment(
    Attachment& attachment,
    std::optional<glm::vec3>& position,
    const std::optional<glm::quat>& orientation);

/**
 * @brief Read the buffer's geometry again and move the anchor to match.
 *
 * The Agent stays where it is: its offset from the new anchor absorbs the
 * difference. Blocks for a ComputeMeshBuffer, so call it off the graphics thread.
 *
 * @param attachment  Attachment to recenter.
 * @param position    Position of the Agent. Set if empty.
 * @param orientation Orientation of the Agent, if it has one.
 */
MAYAFLUX_API void recenter_attachment(
    Attachment& attachment,
    std::optional<glm::vec3>& position,
    const std::optional<glm::quat>& orientation);

/**
 * @brief Rebuild the transform from @p ctx and place every render processor of the buffer.
 * @param attachment Attachment to apply.
 * @param ctx        Current InfluenceContext.
 */
MAYAFLUX_API void apply_attachment(
    Attachment& attachment,
    const InfluenceContext& ctx);

/**
 * @brief Place every render processor of the buffer that is not placed yet.
 *
 * For an attachment whose transform is set by the caller. Processors already
 * followed are left alone; they read the same transform.
 * @param attachment Attachment to place.
 */
MAYAFLUX_API void place_attachment(Attachment& attachment);

/**
 * @brief Give every placed render processor back the geometry transform it had.
 * @param attachment Attachment to release.
 */
MAYAFLUX_API void release_attachment(Attachment& attachment);

} // namespace MayaFlux::Nexus
