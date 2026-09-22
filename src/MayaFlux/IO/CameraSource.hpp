#pragma once

namespace MayaFlux::Kakshya {
class CameraContainer;
}

namespace MayaFlux::IO {

/**
 * @class CameraSource
 * @brief Abstract interface for a live camera backend hosted by IOManager.
 *
 * Covers only the post-open lifecycle: negotiated parameters, frame
 * delivery, and IOService wiring. Deliberately excludes device opening and
 * configuration — those are backend-specific by nature, and forcing them
 * into a shared signature would constrain every future backend to one
 * config shape. A concrete backend owns its own open()-equivalent and is
 * handed to IOManager already open, via IOManager::register_camera_source().
 *
 * A camera source is a live, unbounded stream: no ring buffer, no seek, no
 * batch decode. One frame is pulled per process cycle, demand-driven by
 * CameraContainer::process_default(), never pushed speculatively ahead of
 * the consumer.
 *
 * Two integration paths apply to any implementation:
 *   - Managed:    IOManager owns construction and opening (backend-specific),
 *                 then calls register_camera_source(), which assigns a
 *                 reader_id and wires the container's IOService callback.
 *   - Standalone: the caller drives the same sequence manually —
 *                 create_container() → setup_io_service(id) →
 *                 set_container() → close() — without IOManager.
 *
 * FFmpegCameraReader is the first concrete implementation.
 */
class MAYAFLUX_API CameraSource {
public:
    virtual ~CameraSource() = default;

    /**
     * @brief Release device, decode, and scratch resources.
     */
    virtual void close() = 0;

    /**
     * @brief True if the device is open and ready to deliver frames.
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Create a CameraContainer sized to the negotiated device resolution.
     * @return Initialised container, ready for pull_frame().
     */
    [[nodiscard]] virtual std::shared_ptr<Kakshya::CameraContainer> create_container() const = 0;

    /**
     * @brief Produce one frame into the container.
     *
     * Writes pixel data into the container's frame buffer and marks it ready
     * for processing. Returning false means no frame was available this
     * cycle, which is not necessarily an error.
     *
     * @param container Target CameraContainer.
     * @return True if a new frame was written.
     */
    virtual bool pull_frame(const std::shared_ptr<Kakshya::CameraContainer>& container) = 0;

    /** @brief Negotiated output width in pixels. */
    [[nodiscard]] virtual uint32_t width() const = 0;

    /** @brief Negotiated output height in pixels. */
    [[nodiscard]] virtual uint32_t height() const = 0;

    /** @brief Negotiated frame rate in fps. */
    [[nodiscard]] virtual double frame_rate() const = 0;

    /**
     * @brief Store a weak reference to the container for IOService dispatch.
     * @param container CameraContainer created by this source.
     */
    virtual void set_container(const std::shared_ptr<Kakshya::CameraContainer>& container) = 0;

    /**
     * @brief Signal the source to pull one frame.
     *
     * Non-blocking. Called by IOManager's dispatch, or a standalone
     * IOService lambda, when a frame request arrives for this source's
     * reader_id.
     */
    virtual void pull_frame_all() = 0;

    /** @brief Last error string, empty if no error. */
    [[nodiscard]] virtual const std::string& last_error() const = 0;

    /**
     * @brief Wire this source into an IOService under the given reader_id.
     * @param reader_id Id assigned by IOManager, or self-assigned for
     *                   standalone use.
     */
    virtual void setup_io_service(uint64_t reader_id) = 0;
};

} // namespace MayaFlux::IO
