#pragma once

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend IO streaming service interface.
 *
 * Plain struct of @c std::function fields, following the pattern of
 * DisplayService, BufferService, and InputService. Registered into
 * BackendRegistry by the IO subsystem (currently VideoFileReader directly;
 * future: a dedicated IOSubsystem). Retrieved by VideoStreamContainer during
 * ring setup to wire demand-decode notifications.
 *
 * @c request_decode receives a @c reader_id so that a single registered
 * service instance can dispatch to multiple concurrent streaming readers
 * without per-reader service registrations.
 */
struct IOService {

    /**
     * @brief Request the identified reader to decode the next batch of frames.
     *
     * Called from VideoStreamContainer::update_read_position_for_channel()
     * when buffered-ahead frames drop below the configured threshold.
     * Must be non-blocking: the implementation signals the reader's decode
     * thread and returns immediately. Safe to call from any thread.
     *
     * @param reader_id Opaque identifier assigned by the reader at registration;
     *                  disambiguates concurrent streams sharing this service.
     */
    std::function<void(uint64_t reader_id)> request_decode;

    /**
     * @brief Request the identified camera reader to pull the next frame.
     *
     * Called from CameraContainer when FrameAccessProcessor completes
     * a process cycle. The implementation looks up the CameraReader by
     * reader_id and calls pull_frame_all(). Must be non-blocking.
     * Safe to call from any thread.
     *
     * @param reader_id Opaque identifier assigned by IOManager at
     *                  open_camera() time.
     */
    std::function<void(uint64_t reader_id)> request_frame;
};

} // namespace MayaFlux::Registry::Service
