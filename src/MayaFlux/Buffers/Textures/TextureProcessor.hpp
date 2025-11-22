#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

class TextureBuffer;

/**
 * @class TextureProcessor
 * @brief Manages CPU→GPU texture uploads and updates
 *
 * Default processor for TextureBuffer. Handles:
 * - Initial pixel data upload to GPU VKImage
 * - Detecting buffer modifications and re-uploading
 * - Using TextureLoom for VKImage creation/upload (like RenderFlow for rendering)
 *
 * Automatically attached to TextureBuffer in initialize().
 * Users typically don't create this manually.
 */
class MAYAFLUX_API TextureProcessor : public VKBufferProcessor {
public:
    TextureProcessor();
    ~TextureProcessor() override;

    /**
     * @brief Bind a texture buffer for processing
     * @param texture TextureBuffer to manage
     *
     * Creates GPU VKImage if not exists, uploads initial data.
     */
    void bind_texture_buffer(const std::shared_ptr<TextureBuffer>& texture);

    /**
     * @brief Process texture updates
     * @param buffer Buffer being processed
     *
     * Checks if buffer dirty, re-uploads to GPU if needed.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Get the managed texture buffer
     */
    [[nodiscard]] std::shared_ptr<TextureBuffer> get_texture_buffer() const
    {
        return m_texture_buffer;
    }

protected:
    void on_attach(std::shared_ptr<Buffer> buffer) override;
    void on_detach(std::shared_ptr<Buffer> buffer) override;

private:
    std::shared_ptr<TextureBuffer> m_texture_buffer;

    /**
     * @brief Create GPU VKImage from buffer metadata
     * @return Initialized VKImage
     *
     * Uses TextureLoom to create GPU texture.
     */
    std::shared_ptr<Core::VKImage> create_gpu_texture();

    /**
     * @brief Upload pixel data from VKBuffer to VKImage
     *
     * Uses TextureLoom for transfer.
     */
    void upload_pixels_to_gpu();

    /**
     * @brief Check if buffer data has changed
     */
    bool needs_upload() const;
};

} // namespace MayaFlux::Buffers
