#pragma once

#include "TextureContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct TextureCollectionSpec
 * @brief Describes a TextureCollection so it can be built by designated
 *        initializer.
 *
 * Sizes and format are those of every layer. @c max_bytes caps the GPU memory
 * the layers may take, 512 MiB by default; 0 removes the cap. A ring size above
 * zero enables circular mode, where the collection keeps only the most recent
 * layers and @c max_bytes does not apply.
 */
struct TextureCollectionSpec {
    uint32_t width {};
    uint32_t height {};
    ImageFormat format { ImageFormat::RGBA8 };
    uint64_t max_bytes { 512ULL << 20 };
    uint32_t ring_layers {};
};

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
 * In linear mode appends stop once the next layer would exceed the byte cap.
 * In circular mode append i is written to layer i % ring_layers, replacing
 * the oldest; get_write_head() keeps counting past the ring size.
 *
 * Every layer is a GPU image that stays allocated for the life of the
 * process, even after the collection is destroyed. Appending is not safe
 * while another thread reads the collection.
 */
class MAYAFLUX_API TextureCollection : public TextureContainer {
public:
    explicit TextureCollection(const TextureCollectionSpec& spec);

    /**
     * @brief Copy @p image into the next layer.
     *
     * An image of another size is scaled to fit inside the layer, keeping its
     * aspect ratio, centred on a zeroed background. The format is converted.
     *
     * @return True when the image was stored. False for a null image, when
     *         the device cannot blit its format into the collection's, or
     *         when a linear collection is full.
     */
    bool append(const std::shared_ptr<Core::VKImage>& image);

    /** @brief Number of images appended so far, including overwritten ones. */
    [[nodiscard]] uint64_t get_write_head() const { return m_write_head; }

    /** @brief True when a linear collection cannot take another layer. */
    [[nodiscard]] bool is_full() const;

    [[nodiscard]] const TextureCollectionSpec& get_spec() const { return m_spec; }

private:
    bool copy_into(const std::shared_ptr<Core::VKImage>& image, const std::shared_ptr<Core::VKImage>& target);

    TextureCollectionSpec m_spec;
    uint64_t m_write_head {};
    bool m_full_reported {};
    std::shared_ptr<Core::VKImage> m_blank;
};

} // namespace MayaFlux::Kakshya
