#pragma once

#include "config.h"

namespace MayaFlux::Core {

class Engine;

struct GlobalStreamInfo;

/**
 * @class Stream
 * @brief Manages audio streaming between the engine and audio hardware
 *
 * The Stream class provides a unified interface for audio streaming operations,
 * abstracting the details of the underlying audio API. It currently wraps RtAudio's
 * streaming functionality, but is designed to be extensible to support multiple
 * audio backends and streaming targets in the future.
 *
 * This abstraction layer isolates the rest of the MayaFlux engine from the
 * specifics of audio I/O, making it easier to add support for additional backends
 * like JACK, ASIO, CoreAudio, or WASAPI, as well as non-hardware streaming targets
 * like network audio, file output, or web audio APIs.
 *
 * The Stream class handles the lifecycle of an audio stream, including opening,
 * starting, stopping, and closing the stream, as well as error handling and
 * status reporting.
 *
 * Example usage:
 * ```cpp
 * // Create a stream using the default output device
 * Stream stream(device.get_default_output_device(), engine);
 *
 * // Open the stream (configures but doesn't start audio)
 * stream.open();
 *
 * // Start audio processing
 * stream.start();
 *
 * // Later, stop audio processing
 * stream.stop();
 *
 * // Close the stream when done
 * stream.close();
 * ```
 *
 * The Stream class is a critical component of the audio engine, providing the
 * bridge between the engine's audio processing and the actual audio hardware
 * or other streaming targets. It ensures that audio data flows smoothly and
 * efficiently between the engine and the outside world.
 *
 * Future extensions could include:
 * - Support for multiple simultaneous output streams
 * - Network streaming capabilities
 * - File-based streaming for offline rendering
 * - Web Audio API integration for browser-based applications
 * - Multi-channel routing and mapping
 * - Dynamic stream reconfiguration
 */
class Stream {
public:
    /**
     * @brief Constructs a Stream with the specified output device and engine
     * @param out_device The device ID of the output device to use
     * @param engine Pointer to the Engine that will provide audio data
     *
     * This constructor initializes a Stream that will connect the specified
     * Engine to the specified output device. It sets up the necessary parameters
     * and options for the stream, but does not open or start the stream.
     *
     * The engine must remain valid for the lifetime of the Stream instance.
     */
    Stream(unsigned int out_device, Engine* engine);

    /**
     * @brief Destructor
     *
     * The destructor ensures that the stream is properly closed before the
     * Stream instance is destroyed, preventing resource leaks and audio glitches.
     */
    ~Stream();

    /**
     * @brief Opens the audio stream
     *
     * This method opens the audio stream, configuring it with the parameters
     * and options specified during construction. It prepares the stream for
     * audio processing, but does not start the actual audio flow.
     *
     * Opening a stream allocates the necessary resources and establishes the
     * connection to the audio device, but keeps the stream in a paused state.
     *
     * If the stream is already open, this method has no effect.
     *
     * @throws std::runtime_error if the stream cannot be opened
     */
    void open();

    /**
     * @brief Starts audio processing
     *
     * This method starts the audio stream, beginning the flow of audio data
     * between the engine and the audio device. Once started, the engine's
     * audio callback will be invoked regularly to provide audio data.
     *
     * The stream must be open before it can be started.
     *
     * If the stream is already running, this method has no effect.
     *
     * @throws std::runtime_error if the stream cannot be started
     */
    void start();

    /**
     * @brief Stops audio processing
     *
     * This method stops the audio stream, halting the flow of audio data
     * between the engine and the audio device. The stream remains open and
     * can be restarted later.
     *
     * Stopping a stream pauses audio processing but keeps the connection to
     * the audio device open, allowing for quick resumption of audio.
     *
     * If the stream is not running, this method has no effect.
     */
    void stop();

    /**
     * @brief Closes the audio stream
     *
     * This method closes the audio stream, releasing any resources allocated
     * for it. The stream must be stopped before it can be closed.
     *
     * Closing a stream completely disconnects from the audio device and frees
     * all associated resources. To resume audio, the stream must be reopened.
     *
     * If the stream is not open, this method has no effect.
     */
    void close();

    /**
     * @brief Checks if the stream is currently running
     * @return true if the stream is running, false otherwise
     *
     * This method returns the current running state of the stream. A running
     * stream is actively processing audio data.
     */
    bool is_running() const;

    /**
     * @brief Checks if the stream is currently open
     * @return true if the stream is open, false otherwise
     *
     * This method returns the current open state of the stream. An open
     * stream has allocated resources and established a connection to the
     * audio device, but may or may not be actively processing audio data.
     */
    bool is_open() const;

private:
    /**
     * @brief Parameters for the RtAudio stream
     *
     * This structure contains the configuration parameters for the RtAudio
     * stream, including the device ID, channel count, and first channel.
     */
    RtAudio::StreamParameters m_Parameters;

    /**
     * @brief Options for the RtAudio stream
     *
     * This structure contains the options for the RtAudio stream, such as
     * flags for stream priority, buffer management, and error reporting.
     */
    RtAudio::StreamOptions m_Options;

    /**
     * @brief Flag indicating whether the stream is open
     *
     * This flag tracks whether the stream is currently open (has allocated
     * resources and established a connection to the audio device).
     */
    bool m_is_open;

    /**
     * @brief Flag indicating whether the stream is running
     *
     * This flag tracks whether the stream is currently running (actively
     * processing audio data).
     */
    bool m_is_running;

    /**
     * @brief Handles RtAudio stream errors
     * @param error The RtAudio error type to handle
     *
     * This method handles errors reported by RtAudio during stream operations.
     * It logs appropriate error messages and may take recovery actions depending
     * on the type of error.
     */
    void handle_stream_error(RtAudioErrorType error);

    /**
     * @brief Pointer to the Engine that provides audio data
     *
     * This pointer references the Engine instance that will provide audio data
     * for the stream. The Engine's audio callback is invoked by RtAudio when
     * new audio data is needed.
     */
    Engine* m_engine;
};
}
