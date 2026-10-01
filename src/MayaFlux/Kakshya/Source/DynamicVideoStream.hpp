#pragma once

#include "VideoStreamContainer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Kakshya {

struct ImageData;

/**
 * @class DynamicVideoStream
 * @brief Video container that frames are appended to at runtime, the counterpart
 *        of DynamicSoundStream.
 *
 * Frames arrive through append_frame instead of being decoded from a file. In
 * linear mode the stream grows with each frame. In circular mode
 * (enable_circular_buffer) it keeps only the most recent frames.
 *
 * Frame indices are absolute. Frame i is stored in slot i % capacity and stays
 * readable until a later frame overwrites it, so get_write_head keeps counting
 * past the capacity and is_frame_available says whether an older frame is still
 * there. Reading is unchanged: get_frame_pixels and FrameAccessProcessor work as
 * for any VideoStreamContainer. A FrameAccessProcessor playhead that falls more
 * than the capacity behind the head finds its frames overwritten.
 *
 * Pixels are stored as bytes whatever the format.
 */
class MAYAFLUX_API DynamicVideoStream : public VideoStreamContainer {
public:
    /**
     * @param width      Frame width in pixels.
     * @param height     Frame height in pixels.
     * @param format     Pixel format of appended frames.
     * @param frame_rate Frames per second, used by readers that auto advance.
     */
    DynamicVideoStream(uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8,
        double frame_rate = 0.0);

    /**
     * @brief Write one frame at the write head and advance it.
     * @param pixels Exactly get_frame_byte_size() bytes.
     * @return True if the frame was stored.
     */
    bool append_frame(std::span<const uint8_t> pixels);

    /**
     * @brief Write one image, for example one read back from a GPU image.
     *
     * Width, height and format must match the stream.
     *
     * @return True if the frame was stored.
     */
    bool append_frame(const ImageData& image);

    /**
     * @brief Download a GPU image straight into the next frame slot.
     *
     * Blocking, like TextureContainer::from_image. The image must match the
     * stream's width, height and format. Pass a persistent staging buffer
     * (Buffers::create_image_staging_buffer(get_frame_byte_size())) to avoid a
     * per-call Vulkan allocation.
     *
     * @param image   Initialised source image.
     * @param staging Optional host-visible staging buffer.
     * @return True if the frame was stored.
     */
    bool append_frame(const std::shared_ptr<Core::VKImage>& image,
        const std::shared_ptr<Buffers::VKBuffer>& staging = nullptr);

    /**
     * @brief Index of the next frame append_frame will write.
     *
     * Absolute: in circular mode it keeps counting past the capacity.
     */
    [[nodiscard]] uint64_t get_write_head() const { return m_write_head; }

    /**
     * @brief Keep only the most recent @p capacity frames.
     *
     * Allocates the ring and discards any frames already held, so call it before
     * appending. Use is_ring_mode and get_ring_capacity to query it.
     *
     * @param capacity Number of frames to keep.
     */
    void enable_circular_buffer(uint64_t capacity);

    /**
     * @brief Pre-allocate room for @p frames in linear mode.
     *
     * Appends then reuse that storage instead of growing. Does nothing in
     * circular mode, which allocates its ring up front.
     */
    void ensure_capacity(uint64_t frames);

    /**
     * @brief Copy the most recent frames into a new linear stream, oldest first.
     *
     * Frames that have already been overwritten come out black. The copy is
     * independent of this stream.
     *
     * @param frames Frames to keep, capped at the ring capacity, or at the
     *        frames written for a linear stream.
     * @return New stream with the same size, format and rate, or null if empty.
     */
    [[nodiscard]] std::shared_ptr<DynamicVideoStream> snapshot(uint64_t frames) const;

    /**
     * @brief Drop every frame and restart the write head at zero.
     *
     * A circular stream keeps its capacity.
     */
    void clear() override;

    /**
     * @brief Install a FrameSeekProcessor as the default processor.
     *
     * It reads like FrameAccessProcessor until a time map is set on it.
     */
    void create_default_processor() override;

private:
    uint64_t m_write_head {};

    std::vector<uint8_t>& flat_bytes();
    uint8_t* claim_frame_slot();
    void finish_frame();
    void publish_frame_count();
};

} // namespace MayaFlux::Kakshya
