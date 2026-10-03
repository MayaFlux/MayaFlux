#pragma once

#include "MayaFlux/Nexus/Expanse.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Nexus {

/**
 * @class Hull
 * @brief An Expanse whose region is the bounds of a buffer that already exists.
 *
 * The buffer's geometry is read once, when the Hull is built and on refit(),
 * and the box is kept in the buffer's stored coordinates. Containment takes a
 * world position back through the transform the buffer is drawn with, so the
 * region follows the buffer wherever it is placed, for example by an Agent
 * attached to it, and stays an exact box under rotation and scale. The buffer
 * is neither registered nor written to, and nothing is encoded for a Hull.
 *
 * Only the buffer's primary render processor is read. A buffer whose geometry
 * cannot be read contains nothing. Reading a buffer that a GpuFieldOperator drives
 * downloads it from the device, so refit() belongs on a FRAME_ACCURATE metro.
 */
class MAYAFLUX_API Hull : public Expanse {
public:
    /**
     * @brief Build from a buffer. Blocks for a ComputeMeshBuffer.
     * @param buf        Buffer to wrap. Must not be null.
     * @param on_enter   Fired with the entity id when it enters. May be empty.
     * @param on_exit    Fired with the entity id when it leaves. May be empty.
     * @param collection One collection of a NetworkGeometryBuffer to wrap, for a
     *                   buffer that holds several bodies. Unset wraps all of it.
     */
    explicit Hull(std::shared_ptr<Buffers::VKBuffer> buf,
        CrossingFn on_enter = {},
        CrossingFn on_exit = {},
        std::optional<uint32_t> collection = std::nullopt);

    /** @brief Test whether a world position lies within the buffer's bounds as drawn. */
    [[nodiscard]] bool contains(const glm::vec3& p) const override;

    /**
     * @brief Read the buffer's geometry again and take its bounds.
     *
     * For a deforming mesh or a drifting cloud. Blocks for a ComputeMeshBuffer or a
     * buffer a GpuFieldOperator drives, so call it off the graphics thread. The previous bounds are kept if nothing can be read.
     */
    void refit();

protected:
    /** @brief The centre of the bounds as drawn, and the distance to their farthest corner. */
    [[nodiscard]] std::pair<glm::vec3, float> reach() const override;

private:
    std::shared_ptr<Buffers::VKBuffer> m_buf;
    std::optional<uint32_t> m_collection;
    std::optional<Kinesis::AABB3D> m_local;
};

} // namespace MayaFlux::Nexus
