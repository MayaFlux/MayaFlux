#pragma once

#include "TextureContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class TextureCollection
 * @brief TextureContainer that starts empty and gains one layer per appended
 *        GPU image.
 *
 * The layers are independent images of one size and format, such as one
 * result per frame. Each append copies the image on the GPU, so the source
 * can be reused or overwritten right after. Layers stay on the GPU:
 * to_image() returns a layer's own image, and a CPU accessor downloads a
 * layer once, the first time it reads it.
 *
 * Every layer is a GPU image that stays allocated for the life of the
 * process, even after the collection is destroyed. Appending is not safe
 * while another thread reads the collection.
 */
class MAYAFLUX_API TextureCollection : public TextureContainer {
public:
    /**
     * @brief Empty collection whose layers are @p width x @p height in
     *        @p format.
     */
    TextureCollection(uint32_t width, uint32_t height, ImageFormat format);

    /**
     * @brief Copy @p image into a new layer.
     *
     * An image of another size is scaled to fit inside the layer, keeping its
     * aspect ratio, centred on a zeroed background. The format is converted.
     *
     * @return True when the layer was added. False for a null image or when
     *         the device cannot blit its format into the collection's.
     */
    bool append(const std::shared_ptr<Core::VKImage>& image);

private:
    std::shared_ptr<Core::VKImage> m_blank;
};

} // namespace MayaFlux::Kakshya
