#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "TextureProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class TextureWriteProcessor
 * @brief TextureProcessor subclass accepting externally-supplied pixel data
 *        via DataVariant each frame.
 *
 * Sets PixelSource::EXTERNAL_DATA so update_pixels_if_dirty() routes through
 * get_variant_source() rather than TextureBuffer::m_pixel_data. All other
 * behaviour — geometry updates, GPU upload, staging buffer management,
 * streaming mode — is inherited unchanged from TextureProcessor.
 *
 * Caller supplies data via set_data(). The dirty flag on the TextureBuffer
 * is set on each set_data() call and cleared by the parent after upload.
 * If no new data has been set since the last cycle, the upload is skipped
 * and the dirty flag is cleared — no stale re-upload.
 *
 * Thread safety: set_data() and the graphics thread may run concurrently.
 * Lock-free double-buffer swap via atomic_flag ensures the graphics thread
 * never blocks on the supplier thread.
 */
class MAYAFLUX_API TextureWriteProcessor : public TextureProcessor {
public:
    TextureWriteProcessor();
    ~TextureWriteProcessor() override = default;

    /**
     * @brief Supply pixel data for the next cycle.
     * @param variant Routed through Kakshya::as_texture_access() inside
     *                update_pixels_if_dirty() for type validation and promotion.
     *                Sets TextureBuffer dirty flag so upload is scheduled.
     */
    void set_data(Kakshya::DataVariant variant);

    /**
     * @brief Returns true if a snapshot has been set and not yet consumed.
     */
    [[nodiscard]] bool has_pending() const noexcept;

protected:
    std::optional<Kakshya::DataVariant> get_variant_source() override;

private:
    std::optional<Kakshya::DataVariant> m_pending;
    std::optional<Kakshya::DataVariant> m_active;
    std::atomic_flag m_dirty;
};

} // namespace MayaFlux::Buffers
