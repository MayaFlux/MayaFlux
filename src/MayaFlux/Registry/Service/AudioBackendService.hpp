#pragma once

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend audio subsystem service interface.
 *
 * Registered into BackendRegistry by AudioSubsystem during initialize().
 * Follows the same pattern as NetworkService, BufferService, DisplayService,
 * and InputService: plain struct of std::function members, natural C++ types
 * only, no engine-internal pointers.
 *
 * Two consumption models:
 *
 * Polling: call get_output_snapshot() from any non-RT thread to read the
 * last committed interleaved output buffer.
 *
 * Per-cycle notification: call register_output_observer() with a callback.
 * The callback fires on a dedicated notification thread (not the RT audio
 * thread) once per output cycle. Any number of observers may be registered
 * at any time. Each receives a unique id for later unregistration.
 * The callback must not block; post to a lock-free queue if further work
 * is needed.
 *
 * @code
 * auto* svc = BackendRegistry::instance().get_service<AudioBackendService>();
 * if (!svc) return;
 *
 * // Polling:
 * auto snap = svc->get_output_snapshot();
 *
 * // Per-cycle observer:
 * auto id = svc->register_output_observer([](const double* data, uint32_t total_samples) {
 *     // runs on notification thread, not RT thread
 * });
 *
 * // Later:
 * svc->unregister_output_observer(id);
 * @endcode
 */
struct MAYAFLUX_API AudioBackendService {

    /**
     * @brief Returns a span over the last committed interleaved output buffer.
     *
     * Lock-free. Safe from any non-RT thread. The underlying pointer is
     * engine-owned and valid between the last output cycle and the next.
     * Copy the data if it is needed beyond the current cycle.
     *
     * Returns an empty span if no output cycle has completed yet.
     */
    std::function<std::span<const double>()> get_output_snapshot;

    /**
     * @brief Register a per-cycle output observer.
     *
     * The observer fires on the dedicated notification thread once per
     * output cycle. May be called from any thread at any time.
     *
     * @param observer Callable receiving (interleaved data ptr, total samples).
     * @return Opaque id for use with unregister_output_observer.
     */
    std::function<uint32_t(std::function<void(const double*, uint32_t)>)> register_output_observer;

    /**
     * @brief Unregister a previously registered observer.
     *
     * Safe to call from any thread at any time, including from within
     * the observer callback itself.
     *
     * @param id Id returned by register_output_observer.
     */
    std::function<void(uint32_t)> unregister_output_observer;
};

} // namespace MayaFlux::Registry::Service
