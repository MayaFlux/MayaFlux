#pragma once

#ifdef RTAUDIO_BACKEND
#include "RtAudio.h"
#endif

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

/**
 * @class RtAudioSingleton
 * @brief Thread-safe global access point for audio system resources
 *
 * Implements the Singleton pattern to provide controlled, centralized access
 * to the RtAudio subsystem. Ensures that only one instance of the audio driver
 * exists throughout the application lifecycle, preventing resource conflicts
 * and maintaining system stability.
 *
 * This class enforces strict resource management with the following guarantees:
 * - Thread safety through mutex-protected access
 * - Lazy initialization of the audio subsystem
 * - Exclusive stream ownership validation
 * - Proper resource cleanup on application termination
 */
class RtAudioSingleton {
private:
    /** @brief Singleton instance of the RtAudio driver (nullptr until first access) */
    static std::unique_ptr<RtAudio> s_instance;

    /** @brief Synchronization primitive for thread-safe access to the singleton */
    static std::mutex s_mutex;

    /** @brief Stream state flag to enforce exclusive stream ownership */
    static bool s_stream_open;

    /** @brief Preferred RtAudio API, if specified */
    static std::optional<RtAudio::Api> s_preferred_api;

    /**
     * @brief Private constructor prevents direct instantiation
     *
     * Enforces the Singleton pattern by making the constructor inaccessible
     * to client code, ensuring all access occurs through the static interface.
     */
    RtAudioSingleton() = default;

public:
    /**
     * @brief Provides access to the RtAudio instance with lazy initialization
     * @return Pointer to the singleton RtAudio instance
     *
     * Thread-safe accessor that creates the RtAudio instance on first access
     * and returns the existing instance on subsequent calls. The returned
     * pointer remains valid until cleanup() is called.
     */
    static RtAudio* get_instance()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_instance) {
            if (s_preferred_api.has_value()) {
                s_instance = std::make_unique<RtAudio>(*s_preferred_api);
            } else {
                s_instance = std::make_unique<RtAudio>();
            }
        }
        return s_instance.get();
    }

    /**
     * @brief Sets the preferred audio API before instance creation
     * @param api RtAudio API to use (JACK, ALSA, PULSE, etc.)
     * @throws std::runtime_error if instance already exists
     */
    static void set_preferred_api(RtAudio::Api api)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_instance) {
            error<std::runtime_error>(
                Journal::Component::Core,
                Journal::Context::AudioBackend,
                std::source_location::current(),
                "Cannot set API preference after RtAudio instance created");
        }
        s_preferred_api = api;
    }

    /**
     * @brief Registers an active audio stream in the system
     * @throws std::runtime_error if a stream is already open
     *
     * Thread-safe method that enforces the constraint of having only
     * one active audio stream at any time. This prevents resource conflicts
     * that could lead to audio glitches or system instability.
     */
    static void mark_stream_open()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_stream_open) {
            error<std::runtime_error>(
                Journal::Component::Core,
                Journal::Context::AudioBackend,
                std::source_location::current(),
                "Attempted to open a second RtAudio stream when one is already open");
        }
        s_stream_open = true;
    }

    /**
     * @brief Deregisters an active audio stream from the system
     *
     * Thread-safe method that updates the internal state to reflect
     * that no audio stream is currently active, allowing a new stream
     * to be opened if needed.
     */
    static void mark_stream_closed()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stream_open = false;
    }

    /**
     * @brief Checks if an audio stream is currently active
     * @return True if a stream is open, false otherwise
     *
     * Thread-safe method that provides the current state of stream
     * ownership without modifying any internal state.
     */
    static bool is_stream_open()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_stream_open;
    }

    /**
     * @brief Releases all audio system resources
     *
     * Thread-safe method that performs complete cleanup of the audio
     * subsystem, including stopping and closing any active streams
     * and releasing the RtAudio instance. This method is idempotent
     * and can be safely called multiple times.
     *
     * This method should be called only before application termination to
     * ensure proper resource deallocation and prevent memory leaks. It is not
     * intended for general use and should not be called during normal
     * application operation.
     */
    static void cleanup()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_instance && s_stream_open) {
            try {
                if (s_instance->isStreamRunning()) {
                    s_instance->stopStream();
                }
                if (s_instance->isStreamOpen()) {
                    s_instance->closeStream();
                }
                s_stream_open = false;
            } catch (const RtAudioErrorType& e) {
                error_rethrow(
                    Journal::Component::Core,
                    Journal::Context::AudioBackend,
                    std::source_location::current(),
                    "Error during RtAudio cleanup: {}",
                    s_instance->getErrorText());
            }
        }
        if (s_instance) {
            s_instance.reset();
        }
    }
};

}
