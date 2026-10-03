#pragma once

#include "MayaFlux/Nexus/Expanse.hpp"
#include "MayaFlux/Nexus/Pheme/Attachment.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Mantle
 * @brief An Expanse that is a box with a buffer that already exists as its visible body.
 *
 * The box is the authority. The buffer's geometry is read once, when the Mantle
 * is built and on refit(), and the buffer is placed so that geometry fills the
 * box according to the fit mode. The placement is a geometry transform source on
 * every render processor the buffer has, so it follows set_bounds, and with it
 * bind_bounds, without being pushed. It is the reverse of Hull, where the buffer
 * is the authority and the region follows it.
 *
 * The fit modes are Portal::Graphics::FitMode read for a box. STRETCH scales each
 * axis independently. CONTAIN scales uniformly until the geometry fits inside
 * the box, COVER until it covers the box, and CENTER does not scale. Tiling has
 * no meaning for geometry and is treated as STRETCH.
 *
 * The buffer is neither registered nor written to. Releasing the Mantle gives
 * each processor back the transform it had. Two placements on one buffer stack,
 * as two attachments do. Only the box is encoded, so a decoded Mantle is a plain
 * boxed Expanse and the look must be made again.
 */
class MAYAFLUX_API Mantle : public Expanse {
public:
    /**
     * @brief Build from a box and a buffer. Blocks for a ComputeMeshBuffer.
     * @param bounds   World-space box.
     * @param buf      Buffer to place. Must not be null.
     * @param fit      How the buffer's geometry fills the box.
     * @param on_enter Fired with the entity id when it enters. May be empty.
     * @param on_exit  Fired with the entity id when it leaves. May be empty.
     */
    Mantle(const Kinesis::AABB3D& bounds,
        std::shared_ptr<Buffers::VKBuffer> buf,
        Portal::Graphics::FitMode fit = Portal::Graphics::FitMode::STRETCH,
        CrossingFn on_enter = {},
        CrossingFn on_exit = {});

    ~Mantle() override;

    Mantle(const Mantle&) = delete;
    Mantle& operator=(const Mantle&) = delete;
    Mantle(Mantle&&) = delete;
    Mantle& operator=(Mantle&&) = delete;

    /** @brief Set or replace the box and place the buffer to match. */
    void set_bounds(const Kinesis::AABB3D& bounds) override;

    /**
     * @brief Read the buffer's geometry again and place it to match.
     *
     * For a deforming mesh. Blocks for a ComputeMeshBuffer, so call it off the
     * graphics thread. The previous geometry is kept if nothing can be read.
     */
    void refit();

protected:
    void begin_evaluate() override;

private:
    void refresh();

    Attachment m_look;
    Portal::Graphics::FitMode m_fit;
    std::optional<Kinesis::AABB3D> m_local;
};

} // namespace MayaFlux::Nexus
