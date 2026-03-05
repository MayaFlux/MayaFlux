#pragma once

#include "VideoStreamContainer.hpp"

namespace MayaFlux::Registry::Service {
struct IOService;
}

namespace MayaFlux::Kakshya {

/**
 * @class CameraContainer
 * @brief Single-slot live video container for camera device streams.
 *
 * CameraContainer is a VideoStreamContainer specialisation for live
 * camera input. Unlike VideoFileContainer it has no ring buffer, no
 * seek capability, and no total_frames — the container always holds
 * exactly one frame: the most recently decoded one.
 *
 * The container is pure storage. CameraReader writes decoded RGBA pixels
 * directly into m_data[0] via mutable_frame_ptr(), exactly as
 * VideoFileReader writes into mutable_slot_ptr() for ring buffer slots.
 * FrameAccessProcessor reads from the container via get_frame_pixels(0)
 * during the normal process_default() cycle.
 *
 * Data flow is driven by CameraIOService. After each process() cycle
 * the overridden update_read_position_for_channel() calls
 * IOService::request_frame(reader_id). IOManager dispatches this
 * to CameraReader::pull_frame_all(), which writes the next decoded frame
 * into mutable_frame_ptr() and marks the container READY.
 *
 * Dimensions follow VIDEO_COLOR convention:
 *   dims[0] → TIME      (always 1)
 *   dims[1] → SPATIAL_Y (height)
 *   dims[2] → SPATIAL_X (width)
 *   dims[3] → CHANNEL   (4 for RGBA)
 */
class MAYAFLUX_API CameraContainer : public VideoStreamContainer {
public:
    /**
     * @brief Construct a CameraContainer with specified resolution.
     * @param width      Frame width in pixels.
     * @param height     Frame height in pixels.
     * @param channels   Colour channels per pixel (default 4 for RGBA).
     * @param frame_rate Device frame rate in fps.
     */
    CameraContainer(uint32_t width, uint32_t height,
        uint32_t channels = 4, double frame_rate = 30.0);

    ~CameraContainer() override = default;

    /**
     * @brief Mutable pointer into m_data[0] for the caller to write decoded pixels.
     *
     * Mirrors VideoStreamContainer::mutable_slot_ptr() for ring mode, but
     * operates on the single flat-mode frame. The caller (CameraReader)
     * writes width * height * channels bytes at this address, then calls
     * mark_ready_for_processing(true).
     *
     * @return Pointer to the start of the frame pixel buffer, or nullptr if
     *         the container is not properly initialised.
     */
    [[nodiscard]] uint8_t* mutable_frame_ptr();

    /**
     * @brief Wire the CameraIOService callback for demand-driven frame pulls.
     *
     * Retrieves the CameraIOService from BackendRegistry and stores the
     * reader_id. After this call, update_read_position_for_channel() will
     * call request_frame(reader_id) each time FrameAccessProcessor
     * completes a process cycle.
     *
     * @param reader_id Opaque id assigned by IOManager, matching the
     *                  CameraReader registered in dispatch.
     */
    void setup_io(uint64_t reader_id);

    /**
     * @brief Always returns 1 — camera containers hold exactly one frame.
     */
    [[nodiscard]] uint64_t get_num_frames() const override { return 1; }

    /**
     * @brief Always returns false — camera containers do not loop.
     */
    [[nodiscard]] bool is_looping() const override { return false; }

    /**
     * @brief Create FrameAccessProcessor with auto_advance disabled.
     */
    void create_default_processor() override;

    /**
     * @brief Override to fire request_frame after each processor cycle.
     *
     * VideoStreamContainer::process_default() drives state through
     * PROCESSING → PROCESSED but never calls update_read_position_for_channel,
     * because FrameAccessProcessor::auto_advance is disabled for live input.
     * Without auto_advance there is no call site for update_read_position_for_channel,
     * so IOService::request_frame never fires and CameraReader never pulls a new frame.
     *
     * This override calls the base implementation then immediately issues
     * request_frame, replacing the position-advance trigger with a direct
     * post-process trigger that is semantically correct for a single-slot
     * live container: every completed process cycle is a demand for the next frame.
     */
    void process_default() override;

private:
    Registry::Service::IOService* m_io_service { nullptr };
};

} // namespace MayaFlux::Kakshya
