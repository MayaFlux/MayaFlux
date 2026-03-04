#pragma once

#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class VideoStreamReader
 * @brief Adapter bridging VideoStreamContainer processed data to TextureBuffer pixel storage.
 *
 * VideoStreamReader is the video counterpart of SoundStreamReader. Where
 * SoundStreamReader extracts per-channel audio samples from a container's
 * processed_data into an AudioBuffer, VideoStreamReader extracts full-frame
 * RGBA pixel data from a container's processed_data into a TextureBuffer's
 * pixel storage, marking it dirty for GPU upload.
 *
 * The pipeline mirrors audio exactly:
 *
 *   VideoStreamContainer
 *     → FrameAccessProcessor  (extracts frame into processed_data)
 *     → VideoStreamReader     (copies processed_data into TextureBuffer pixels)
 *     → TextureProcessor      (uploads dirty pixels to GPU VKImage)
 *     → RenderProcessor       (draws textured quad)
 *
 * VideoStreamReader triggers the container's default processor
 * (FrameAccessProcessor) if the container is in READY state, then copies
 * the resulting uint8_t RGBA data into the TextureBuffer via
 * set_pixel_data(). This marks the texture dirty, causing TextureProcessor
 * to re-upload on the next frame.
 *
 * Unlike SoundStreamReader, there is no channel selection — video frames are
 * atomic spatial units. The reader always extracts the entire frame surface.
 *
 * @see SoundStreamReader, FrameAccessProcessor, TextureBuffer, VideoStreamContainer
 */
class MAYAFLUX_API VideoStreamReader : public BufferProcessor {
public:
    /**
     * @brief Construct a VideoStreamReader for the given container.
     * @param container The video StreamContainer to read from.
     */
    explicit VideoStreamReader(const std::shared_ptr<Kakshya::StreamContainer>& container);

    /**
     * @brief Extract the current frame from the container into the TextureBuffer.
     *
     * Triggers the container's default processor if needed, extracts the
     * processed uint8_t RGBA data, and copies it into the TextureBuffer's
     * pixel storage via set_pixel_data().
     *
     * @param buffer The TextureBuffer to write pixels into.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Attach the reader to a TextureBuffer.
     *
     * Registers as a dimension reader on the container, validates that the
     * container is ready, and performs an initial frame extraction.
     *
     * @param buffer The TextureBuffer to attach to.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Detach the reader from its TextureBuffer.
     *
     * Unregisters from the container and cleans up state.
     *
     * @param buffer The TextureBuffer to detach from.
     */
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Replace the backing container.
     * @param container New StreamContainer to read from.
     */
    void set_container(const std::shared_ptr<Kakshya::StreamContainer>& container);

    /**
     * @brief Get the backing container.
     * @return Current StreamContainer.
     */
    std::shared_ptr<Kakshya::StreamContainer> get_container() const { return m_container; }

    /**
     * @brief Enable or disable automatic buffer state flag updates.
     *
     * When enabled, the reader will mark the buffer for processing or
     * removal based on container state transitions.
     *
     * @param update true to enable state flag updates.
     */
    void set_update_flags(bool update) { m_update_flags = update; }

    /**
     * @brief Get the current state flag update setting.
     * @return true if state flag updates are enabled.
     */
    [[nodiscard]] bool get_update_flags() const { return m_update_flags; }

private:
    std::shared_ptr<Kakshya::StreamContainer> m_container;

    uint32_t m_reader_id {};
    bool m_update_flags { true };

    /**
     * @brief Extract frame pixel data from processed_data into the TextureBuffer.
     * @param texture_buffer The TextureBuffer to write into.
     */
    void extract_frame_data(const std::shared_ptr<TextureBuffer>& texture_buffer);

    /**
     * @brief Respond to container state changes.
     *
     * Mirrors SoundStreamReader's state callback pattern for lifecycle
     * synchronisation between container and buffer.
     *
     * @param container The container whose state changed.
     * @param state The new processing state.
     */
    void on_container_state_change(const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        Kakshya::ProcessingState state);
};

/**
 * @class VideoContainerBuffer
 * @brief TextureBuffer implementation backed by a VideoStreamContainer.
 *
 * VideoContainerBuffer is the video counterpart of SoundContainerBuffer. It
 * bridges the Kakshya container system and the Vulkan rendering pipeline by
 * wiring a VideoStreamReader as the default processor and repositioning the
 * inherited TextureProcessor as the chain preprocessor.
 *
 * Execution order per frame cycle (via process_complete):
 *   1. Default processor  → VideoStreamReader pulls frame from container,
 *                            writes RGBA pixels into TextureBuffer via
 *                            set_pixel_data(), marks texture dirty.
 *   2. Preprocessor       → TextureProcessor detects dirty flag, uploads
 *                            pixels to GPU VKImage, updates quad geometry.
 *   3. Chain processors   → User-added effects, compute shaders, etc.
 *   4. Postprocessor      → (available for user)
 *   5. Final processor    → RenderProcessor draws textured quad to window.
 *
 * This mirrors the audio pipeline exactly:
 *
 *   Audio:  SoundStreamContainer → ContiguousAccessProcessor
 *             → SoundStreamReader → AudioBuffer → speakers
 *
 *   Video:  VideoStreamContainer → FrameAccessProcessor
 *             → VideoStreamReader → VideoContainerBuffer
 *             → TextureProcessor → RenderProcessor → window
 *
 * The buffer inherits all TextureBuffer capabilities: pixel storage with
 * dirty tracking, quad geometry with transform support, GPU texture access,
 * and RenderProcessor integration via setup_rendering().
 *
 * @see SoundContainerBuffer, VideoStreamReader, FrameAccessProcessor, TextureBuffer
 */
class MAYAFLUX_API VideoContainerBuffer : public TextureBuffer {
public:
    /**
     * @brief Construct a VideoContainerBuffer from a video container.
     *
     * Extracts width, height, and pixel format from the container's structure
     * and initialises the TextureBuffer base with matching dimensions.
     * Creates a pending VideoStreamReader that is wired on initialize().
     *
     * @param container    Backing video StreamContainer (must carry VIDEO_COLOR data).
     * @param format       Pixel format (default RGBA8 matching container convention).
     */
    VideoContainerBuffer(
        const std::shared_ptr<Kakshya::StreamContainer>& container,
        Portal::Graphics::ImageFormat format = Portal::Graphics::ImageFormat::RGBA8);

    ~VideoContainerBuffer() override = default;

    /**
     * @brief Override to wire VideoStreamReader as default and
     *        TextureProcessor as preprocessor.
     *
     * @param token Processing token (typically GRAPHICS_BACKEND).
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Get the backing video container.
     * @return Current StreamContainer.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::StreamContainer> get_container() const { return m_container; }

    /**
     * @brief Replace the backing container at runtime.
     *
     * Updates the VideoStreamReader's source. Does not resize the
     * TextureBuffer — caller is responsible for ensuring dimension
     * compatibility or recreating the buffer.
     *
     * @param container New StreamContainer to read from.
     */
    void set_container(const std::shared_ptr<Kakshya::StreamContainer>& container);

    /**
     * @brief Get the VideoStreamReader processor.
     * @return The reader processor, or nullptr if not yet initialised.
     */
    [[nodiscard]] std::shared_ptr<VideoStreamReader> get_video_reader() const { return m_video_reader; }

private:
    std::shared_ptr<Kakshya::StreamContainer> m_container;
    std::shared_ptr<VideoStreamReader> m_video_reader;
};

} // namespace MayaFlux::Buffers
