#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Viewport/ViewTransform.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {
class ImageTarget;
}

namespace MayaFlux::Portal::Forma {

/**
 * @class PickImage
 * @brief Finds what the pointer is on by drawing it, for geometry the CPU does
 *        not hold: deformed, instanced or generated on the GPU.
 *
 * Added buffers are drawn a second time into an image holding, per texel, which
 * buffer is nearest there, its UV and its depth. Added buffers hide each other;
 * what was not added hides nothing. A buffer's vertex shader has to pass its UV
 * at location 1, as triangle.vert does.
 *
 * The texel under the pointer is read back without waiting, so an answer is the
 * last one that has arrived, about a frame behind.
 */
class MAYAFLUX_API PickImage {
public:
    /**
     * @brief An image of the given size. Match the aspect of the view the
     *        buffers are seen with; the size only sets how finely it picks.
     */
    PickImage(uint32_t width, uint32_t height);
    ~PickImage();

    PickImage(const PickImage&) = delete;
    PickImage& operator=(const PickImage&) = delete;
    PickImage(PickImage&&) = delete;
    PickImage& operator=(PickImage&&) = delete;

    /**
     * @brief Also draw @p buffer into the image. It has to be set up for rendering.
     * @return False when it has no render processor.
     */
    bool add(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief Draw @p buffer into the image so it hides added ones, without being
     *        picked itself. It has to meet what add() asks. add() of it makes it
     *        pickable.
     * @return False when it has no render processor.
     */
    bool occlude(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /** @brief Stop drawing @p buffer into the image. */
    void remove(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief An Element::to_local for an added buffer.
     *
     * Hits where @p buffer is the nearest added buffer. Its UV is mapped into
     * @p bounds, v running down, as mesh_to_local does, and z is the distance
     * along the pointer ray. The function does not keep the PickImage alive.
     *
     * @param buffer A buffer given to add().
     * @param view   The view the buffer is drawn with.
     * @param bounds The host element's rectangle.
     */
    [[nodiscard]] std::function<std::optional<glm::vec3>(glm::vec2)> to_local(
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        std::function<Kinesis::ViewTransform()> view,
        Kinesis::AABB2D bounds) const;

    /** @brief The image drawn into. */
    [[nodiscard]] const std::shared_ptr<Graphics::ImageTarget>& image() const noexcept;

private:
    struct State;
    std::shared_ptr<State> m_state;
};

} // namespace MayaFlux::Portal::Forma
